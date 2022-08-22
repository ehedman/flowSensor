/*****************************************************************************
* | File      	:   water-ctrl.c
* | Author      :   erland@hedmanshome.se
* | Function    :   Messure water flow and filter properties.
* | Info        :   
* | Depends     :   Rasperry Pi Pico W, Waveshare Pico LCD 1.14 V2, DS3231 piggy-back RTC module
*----------------
* |	This version:   V1.0
* | Date        :   2022-08-22
* | Info        :   Build context is within the Waveshare Pico SDK c/examples2
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <pico/stdlib.h>
#include <hardware/irq.h>
#include <hardware/i2c.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/sleep.h>
#include <pico/util/datetime.h>
#include "hardware/clocks.h"
#include "hardware/rosc.h"
#include "hardware/structs/scb.h"
#include "water-ctrl.h"
#include <pico/cyw43_arch.h>
#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>
#include "lwip/tcp.h"

#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME (30 * 1000)
#define NTP_RESEND_TIME (10 * 1000)

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    bool dns_request_sent;
    struct udp_pcb *ntp_pcb;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;
} NTP_T;

static bool rtcIsSetDone = false;
static bool netIsConnected = false;

/**
 * We're going to erase and reprogram a region 512k from the start of flash.
 * Once done, we can access this at XIP_BASE + 512k.
*/
#define FLASH_TARGET_OFFSET (512 * 1024)    // 512 for W

static const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

#if 0
static void print_buf(const uint8_t *buf, size_t len) {
    int ints = save_and_disable_interrupts();
    for (size_t i = 0; i < len; ++i) {
        printf("%02x", buf[i]);
        if (i % 16 == 15)
            printf("\n");
        else
            printf(" ");
    }
    restore_interrupts(ints);
}
#endif

/**
 * Read from flash
 */
void read_flash(persistent_data *pdata)
{
    memcpy((uint8_t*)pdata, (void*)flash_target_contents, sizeof(persistent_data));
}

/**
 * Write to flash if needed
 */
bool write_flash(persistent_data *new_data)
{
    static uint8_t flash_data[FLASH_PAGE_SIZE];
    uint8_t *ndata = (uint8_t *)new_data;
    uint32_t ints;
    bool rval = true;  
    uint8_t cs;
    static persistent_data old_data;

    read_flash(&old_data);

    for (int i = sizeof(uint8_t); i < sizeof(persistent_data); i++) {
        cs += ndata[i];
    }

    if (cs == old_data.checksum) {
        printf("Flash content is up to date\n");
        return rval;
    }

    printf("Re-program flash with new content\n");
    new_data->checksum = cs;

    ints = save_and_disable_interrupts();

    memset(flash_data, 0, sizeof(flash_data));

    for (int i = 0; i < sizeof(persistent_data); i++) {
        flash_data[i] = ndata[i];
    }

    // Note that a whole number of sectors must be erased at a time.
    //printf("\nErasing target region and re-porgram the flash ...\n");

    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    flash_range_program(FLASH_TARGET_OFFSET, flash_data, FLASH_PAGE_SIZE);

    for (int i = 0; i < sizeof(persistent_data); ++i) {
        if (flash_data[i] != flash_target_contents[i])
            rval = false;
    }

    restore_interrupts(ints);
  
    return rval;   
}

/**
 * Connect to AP
 */
bool wifi_connect(char *ssid, char *pass, uint32_t country)
{
    int rval = 0;
    time_t retry;

    if (netIsConnected == true) {
        printf("Repeated connection request ignored\n");
        return netIsConnected;
    }

    netIsConnected = false;

    ssid[SSID_MAX-1] = pass[PASS_MAX-1] = '\0';

    if (!ssid[strspn(ssid, OKCHAR)] == '\0') {
        printf("SSID contains illegal characters\n");
        return netIsConnected;
    }

    if (!pass[strspn(pass, OKCHAR)] == '\0') {
        printf("WPA2 password contains illegal characters\n");
        return netIsConnected;
    }

    printf("Attempt connection to %s with pass %s and country code %d\n", ssid, pass, country);

    if ((rval = cyw43_arch_init_with_country(country))) {
        printf("\ncyw43_arch_init_with_country failed: %s/%d/%d/%d\n", __FILENAME__, __LINE__, country, rval);
        return netIsConnected;
    }

    cyw43_arch_enable_sta_mode();

    // this seems to be the best be can do using the predefined `cyw43_pm_value` macro:
    // cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    // however it doesn't use the `CYW43_NO_POWERSAVE_MODE` value, so we do this instead:
    cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

    for (int i = 0; i < 3; i++) {
        retry = time(NULL);
        if ((rval = cyw43_arch_wifi_connect_timeout_ms(ssid, pass, CYW43_AUTH_WPA2_AES_PSK, 10000))) {
            printf("\ncyw43_arch_wifi_connect_timeout_ms failed: %s/%d/%s/%d\n", __FILENAME__, __LINE__, ssid, rval);
            if (rval < 0 && time(NULL) - retry < 4) {
                printf("Retry %d/3\n", i+1);
                sleep_ms(2000);
            } else {
                return netIsConnected;
            }
        }
        if (rval == 0) {
            netIsConnected = true;
            break;
        }
    }

    return netIsConnected;
}

/**
 * Initialize the DS3231 piggy-back RTC module
 */
static void ds3231Init(void)
{
    static int ds3231InitStatus;
    
    if (!ds3231InitStatus) {
	    i2c_init(I2C_PORT,100*1000);
	    gpio_set_function(I2C_SDA,GPIO_FUNC_I2C);
	    gpio_set_function(I2C_SCL,GPIO_FUNC_I2C);
	    gpio_pull_up(I2C_SDA);
	    gpio_pull_up(I2C_SCL);
	    //ds3231InitStatus = 1;
    }
}

/**
 * converts BCD to decimal
 */
static uint8_t bcd_to_decimal(uint8_t number)
{
  return ( (number >> 4) * 10 + (number & 0x0F) );
}

/**
 * converts decimal to BCD
 */
static uint8_t decimal_to_bcd(uint8_t number)
{
  return ( ((number / 10) << 4) + (number % 10) );
}

/**
 * write data to the RTC chip
 */
static void ds3231SetTime(struct tm *utc)
{
    char tbuf[2];

    ds3231Init();

    //set second
    tbuf[0]=0x00;
    tbuf[1]=decimal_to_bcd(utc->tm_sec);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set minute
    tbuf[0]=0x01;
    tbuf[1]=decimal_to_bcd(utc->tm_min);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set hour
    tbuf[0]=0x02;
    tbuf[1]=decimal_to_bcd(utc->tm_hour+2);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set weekday
    tbuf[0]=0x03;
    tbuf[1]=decimal_to_bcd(utc->tm_wday);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set day
    tbuf[0]=0x04;
    tbuf[1]=decimal_to_bcd(utc->tm_mday);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set month
    tbuf[0]=0x05;
    tbuf[1]=decimal_to_bcd(utc->tm_mon +1);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);
    //set year
    tbuf[0]=0x06;
    tbuf[1]=decimal_to_bcd(utc->tm_year - 100);
    i2c_write_blocking(I2C_PORT,DS3231_ADDR,tbuf,2,false);

}

/**
 * Reads time and date and returns epoch
 */
static time_t ds3231ReadTime() 
{
    static struct tm utc;
    uint8_t val = 0; 
    static char  buf[10];

    ds3231Init();

    memset(&utc, 0, sizeof(struct tm));
    memset(buf, 0, sizeof(buf));

    i2c_write_blocking(I2C_PORT,DS3231_ADDR,&val,1,true);
    i2c_read_blocking(I2C_PORT,DS3231_ADDR,buf,7,false);

    utc.tm_sec  = bcd_to_decimal(buf[0]);
    utc.tm_min  = bcd_to_decimal(buf[1]);;
    utc.tm_hour = bcd_to_decimal(buf[2]);
    //utc.tm_wday = bcd_to_decimal(buf[3]);
    utc.tm_mday = bcd_to_decimal(buf[4]);
    utc.tm_mon  = bcd_to_decimal(buf[5]) -1;
    utc.tm_year = bcd_to_decimal(buf[6]) +100;
	utc.tm_isdst = 0;

    return mktime(&utc);;
}

/**
 * A private implementation of time()
 */
time_t _time(time_t *tloc)
{
#if NETRTC
    return ds3231ReadTime() + TZ_RZONE;
#else
    static int sec;
    sec += 10;
    return 1660411426 + sec;    // Fake time around Sat Aug 13 17:23 2022
#endif
}

/**
 * Wrapper for the ds3231
 */
static void set_rtc(struct tm *utc)
{
    ds3231SetTime(utc);
}

/**
 * Called with results of the NTP operation
*/
static void ntp_result(NTP_T* state, int status, time_t *result) {
    if (status == 0 && result) {
        struct tm *utc = gmtime(result);
        printf("got ntp response: %02d/%02d/%04d %02d:%02d:%02d\n", utc->tm_mday, utc->tm_mon + 1, utc->tm_year + 1900,
               utc->tm_hour, utc->tm_min, utc->tm_sec);
        printf("set RTC accordingly\n");
        set_rtc(utc);
        rtcIsSetDone = true;
    }

    if (state->ntp_resend_alarm > 0) {
        cancel_alarm(state->ntp_resend_alarm);
        state->ntp_resend_alarm = 0;
    }
    state->ntp_test_time = make_timeout_time_ms(NTP_TEST_TIME);
    state->dns_request_sent = false;
}

/**
 * Foward decalaration of ntp_falied handler
 */
static int64_t ntp_failed_handler(alarm_id_t id, void *user_data);

/**
 * Make an NTP request
*/
static void ntp_request(NTP_T *state) {
    // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
    // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
    // these calls are a no-op and can be omitted, but it is a good practice to use them in
    // case you switch the cyw43_arch type later.
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
    NTP_T* state = (NTP_T*)user_data;
    printf("ntp request failed\n");
    ntp_result(state, -1, NULL);
    rtcIsSetDone = true;
    return 0;
}

/**
 * Call back with a DNS result
*/
static void ntp_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    NTP_T *state = (NTP_T*)arg;
    if (ipaddr) {
        state->ntp_server_address = *ipaddr;
        printf("ntp address %s\n", ip4addr_ntoa(ipaddr));
        ntp_request(state);
    } else {
        printf("ntp dns request failed\n");
        ntp_result(state, -1, NULL);
    }
}

/**
 * NTP data received
 */
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    NTP_T *state = (NTP_T*)arg;
    uint8_t mode = pbuf_get_at(p, 0) & 0x7;
    uint8_t stratum = pbuf_get_at(p, 1);

    // Check the result
    if (ip_addr_cmp(addr, &state->ntp_server_address) && port == NTP_PORT && p->tot_len == NTP_MSG_LEN &&
        mode == 0x4 && stratum != 0) {
        uint8_t seconds_buf[4] = {0};
        pbuf_copy_partial(p, seconds_buf, sizeof(seconds_buf), 40);
        uint32_t seconds_since_1900 = seconds_buf[0] << 24 | seconds_buf[1] << 16 | seconds_buf[2] << 8 | seconds_buf[3];
        uint32_t seconds_since_1970 = seconds_since_1900 - NTP_DELTA;
        time_t epoch = seconds_since_1970;
        ntp_result(state, 0, &epoch);
    } else {
        printf("invalid ntp response\n");
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

/**
 * Perform initialisation
 */
static NTP_T* ntp_init(void) {
    NTP_T *state = calloc(1, sizeof(NTP_T));
    if (!state) {
        printf("failed to allocate state\n");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb) {
        printf("failed to create pcb\n");
        free(state);
        return NULL;
    }
    udp_recv(state->ntp_pcb, ntp_recv, state);
    return state;
}

/**
 * Entry point for an NTP request
 */
static bool net_ntp(char *ntp_server)
{   
    bool rval = false;

    NTP_T *state = ntp_init();
    if (!state) {
        printf("ntp_init failed\n");
        return rval;
    }

    ntp_server[URL_MAX-1] = '\0';
        
    for (int i=0; i <3; i++) {
        if (absolute_time_diff_us(get_absolute_time(), state->ntp_test_time) < 0 && !state->dns_request_sent) {

            // Set alarm in case udp requests are lost
            state->ntp_resend_alarm = add_alarm_in_ms(NTP_RESEND_TIME, ntp_failed_handler, state, true);

            // cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
            // You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
            // these calls are a no-op and can be omitted, but it is a good practice to use them in
            // case you switch the cyw43_arch type later.
            cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(ntp_server, &state->ntp_server_address, ntp_dns_found, state);
            cyw43_arch_lwip_end();

            state->dns_request_sent = true;
            if (err == ERR_OK) {
                ntp_request(state); // Cached result
                rval = true;
                break;
            } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
                printf("dns request failed(%d)\n", i+1);
                ntp_result(state, -1, NULL);
            }
        }
    }
    free(state);

    return rval;
}

bool netNTP_connect(persistent_data *pdata)
{
    bool rval = false;
    static char buf_ntp[20];

    if (netIsConnected == false) {

        if (wifi_connect(pdata->ssid, pdata->pass, pdata->country) == false) {
            return rval;
        } else {
            if (!strncmp(pdata->ntp_server, "0.0.0.0", 7)) {
                // Default gw host assumed to hold NTP services
                strncpy(buf_ntp, ip4addr_ntoa(netif_ip4_gw(netif_list)), sizeof(buf_ntp));
            } else {
                strncpy(buf_ntp, pdata->ntp_server, sizeof(buf_ntp));
            }
            if (net_ntp(buf_ntp) == false) {
                return rval;
            }
            for (int i = 0; i < NTP_TEST_TIME/1000; i++) {
                sleep_ms(1000);
                if (rtcIsSetDone == true ) { // Set in the ntp_result() or ntp_failed_handler() callbacks
                    rtcIsSetDone = false;
                    rval =  true;
                    break;
                }
            }
        }
    }

    return rval;
}

/**
 * Return logical connection status
 */
bool net_checkconnection(void)
{
    return netIsConnected;
}

/**
 * De-init the network
 */
void net_disconnect(void)
{
    if (netIsConnected == true) {
        cyw43_arch_deinit();
        netIsConnected = false;
    }
}

#if 0
static void measure_freqs(void)
{

    printf("usb=%lu\n", clock_get_hz(clk_usb));
    printf("sys=%lu\n", clock_get_hz(clk_sys));
    printf("peri=%lu\n", clock_get_hz(clk_peri));
    printf("usb=%lu\n", clock_get_hz(clk_usb));
    printf("adc=%lu\n", clock_get_hz(clk_adc));
    printf("rtc=%lu\n", clock_get_hz(clk_rtc));

}
#endif

static void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig)
{

    // Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    // Reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    // Reset clocks
    clocks_init();
    stdio_init_all();
}

void goDormant(int dpin)
{
    static char buffer_t[60];

    time_t curtime = time(NULL);

    printf("Enter dormant mode at %s", ctime_r(&curtime, buffer_t));
    /*
     * Some clocks at resume have losts its original values
     * and the performce is lost to almost an useless level.
     * Note that this function is funtionality wise untuched
     * from the pico "extra" package example but to get the
     * complete picture and a solution, check this out:
     * https://ghubcoder.github.io/posts/awaking-the-pico/
     * https://github.com/ghubcoder/PicoSleepDemo
     */

    // Save values for later
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    //measure_freqs();

    // Switching to XOSC
    uart_default_tx_wait_blocking();

    // UART will be reconfigured by sleep_run_from_xosc
    sleep_run_from_xosc();

    // Running from XOSC
    uart_default_tx_wait_blocking();

    // XOSC going dormant
    uart_default_tx_wait_blocking();

    // Go to sleep until we see a high edge on GPIO dormantPin
    sleep_goto_dormant_until_edge_high(dpin);

    // Reset processor and clocks back to defaults
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig);

    curtime = time(NULL);
    printf("Resuming from dormant at %s", ctime_r(&curtime, buffer_t));

    //measure_freqs();

}


