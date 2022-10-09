/*****************************************************************************
* | File      	:   water-cnfg.c
* | Author      :   erland@hedmanshome.se
* | Function    :   Functions for water-ctrl.c
* | Info        :   
* | Depends     :   Rasperry Pi Pico W, LwIP, DS3231 piggy-back RTC module
*----------------
* |	This version:   V1.1
* | Date        :   2022-09-22
* | Info        :   Build context is within the Waveshare Pico SDK c/examples
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
#include <hardware/clocks.h>
#include <hardware/rosc.h>
#include <hardware/watchdog.h>
#include <hardware/structs/scb.h>
#include <hardware/adc.h>
#include "water-ctrl.h"
#ifdef HAS_NET
#include <pico/cyw43_arch.h>
#include <lwip/opt.h>
#include <lwip/dns.h>
#include <lwip/pbuf.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/ip_addr.h>
#include "dhcpserver.h"
#endif

#ifdef HAS_TEMPS
#include "one_wire_c.h"
#endif

#ifndef HAS_NET
#undef LWIP_RAW
#endif

#if LWIP_RAW
#include <lwip/mem.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/netif.h>
#include <lwip/sys.h>
#include <lwip/timeouts.h>
#include <lwip/inet_chksum.h>
#include <lwip/prot/ip4.h>
#include <lwip/opt.h>

/**
 * Default ping delay - in milliseconds
 */
#define PING_DELAY     2000

/** 
 * ping identifier - must fit on a u16_t
 */
#ifndef PING_ID
#define PING_ID        0xAFAF
#endif

/**
 * ping additional data size to include in the packet
 */
#ifndef PING_DATA_SIZE
#define PING_DATA_SIZE 32
#endif

/**
 * ping variables
 */
static ip_addr_t* ping_target;
static u16_t ping_seq_num;
static struct raw_pcb *ping_pcb;
static bool goodPing = false;
static bool pRawisInit;
static void ping_init(const char*);

#endif /* LWIP_RAW */

#ifdef HAS_NET

#define NTP_MSG_LEN 48
#define NTP_PORT 123
#define NTP_DELTA 2208988800 // seconds between 1 Jan 1900 and 1 Jan 1970
#define NTP_TEST_TIME (30 * 1000)
#define NTP_RESEND_TIME (10 * 1000)

#define CONNECTION_TMO  (time_t)20000

typedef struct NTP_T_ {
    ip_addr_t ntp_server_address;
    bool dns_request_sent;
    struct udp_pcb *ntp_pcb;
    absolute_time_t ntp_test_time;
    alarm_id_t ntp_resend_alarm;
} NTP_T;

static bool rtcIsSetDone = false;
static bool netIsConnected = nOFF;

#endif /* HAS_NET */

/**
 * We're going to erase and reprogram a region 740k from the start of flash.
 * Once done, we can access this at XIP_BASE + 740k.
*/
#define FLASH_TARGET_OFFSET (1024 * 1024)    // Eventually move up as the application growes.

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
bool read_flash(persistent_data *pdata)
{
    if (sizeof(persistent_data) > FLASH_PAGE_SIZE) {
        printf("\nFlash data exceeds size of page size (%d bytes)\n", FLASH_PAGE_SIZE);
        return false;
    }

    memcpy((uint8_t*)pdata, (void*)flash_target_contents, sizeof(persistent_data));

    return true;
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
    uint8_t cs = 0;
    static persistent_data old_data;

    if (sizeof(persistent_data) > FLASH_PAGE_SIZE) {
        // May grow during development
        printf("\nFlash data exceeds size of page size (%d bytes)\n", FLASH_PAGE_SIZE);
        return false; 
    }

    read_flash(&old_data);

    for (int i = (int)sizeof(uint8_t); i < (int)sizeof(persistent_data); i++) {
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

    for (int i = 0; i < (int)sizeof(persistent_data); i++) {
        flash_data[i] = ndata[i];
    }

    /**
     * Note that a whole number of sectors must be erased at a time.
     * printf("\nErasing target region and re-porgram the flash ...\n");
     */
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    flash_range_program(FLASH_TARGET_OFFSET, flash_data, FLASH_PAGE_SIZE);

    for (int i = 0; i < (int)sizeof(persistent_data); ++i) {
        if (flash_data[i] != flash_target_contents[i])
            rval = false;
    }

    restore_interrupts(ints);
  
    return rval;   
}

#ifdef HAS_RTC
/**
 * Initialize the DS3231 piggy-back RTC module
 */
static void ds3231Init(void)
{
    static bool ds3231InitStatus;
    
    if (!ds3231InitStatus) {
	    i2c_init(I2C_PORT,100*1000);
	    gpio_set_function(I2C_SDA,GPIO_FUNC_I2C);
	    gpio_set_function(I2C_SCL,GPIO_FUNC_I2C);
	    gpio_pull_up(I2C_SDA);
	    gpio_pull_up(I2C_SCL);
	    ds3231InitStatus = true;
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
    uint8_t tbuf[2];

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
    static uint8_t buf[10];

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
 * Wrapper for the ds3231
 */
static void set_rtc(struct tm *utc, time_t *epoch)
{
    APP_UNUSED_ARG(epoch);
    ds3231SetTime(utc);
}

/**
 * A private implementation of time()
 */
time_t _time(time_t *tloc)
{
    APP_UNUSED_ARG(tloc);
    return ds3231ReadTime() + TZ_RZONE;
}

#else /* HAS_RTC */

#if defined COMPILE_TIME_EPOCH
/*
 * Remains as start time unless overwritten by a successful NTP action.
 */
static time_t volatile_epoch = COMPILE_TIME_EPOCH;
#else
static time_t volatile_epoch = 1660411426;   // Fallback fake time around Sat Aug 13 17:20 202
#endif

time_t _time(time_t *tloc)
{
    APP_UNUSED_ARG(tloc);

    return volatile_epoch +  TZ_RZONE + get_absolute_time()/1000000;
}

static void set_rtc(struct tm *utc, time_t *epoch)
{
    APP_UNUSED_ARG(utc);

    volatile_epoch = *epoch;
}

#endif  /* HAS_RTC */

#if defined HAS_TDS

/**
 * Return water temperature
 */
#ifdef HAS_TEMPS
static float getTemperature(void)
{
    static rom_address_t address;
    static bool init;

    if (init == false) {
        one_wire_single_device_read_rom(&address);
        init = true;
    }

    one_wire_convert_temperature(&address, true, false);

    return one_wire_temperature(&address);
}
#else
static float getTemperature(void)
{
    return 20.0;

}
#endif /* HAS_TEMPS */

/**
 * Return TDS value for a Gravity TDS module
 * TDS module output voltage: 0 ~ 2.3V @ Range: 0 ~ 1000ppm
*/
void tdsConvert(shared_data *sdata, persistent_data *pdata)
{
    const float TdsFactor = 0.5;                // TDS value is half of the electrical conductivity value, that is: TDS = EC / 2. 
    //pdata->kValue                             // K value of the probe, eventually calibrated in a known (xxx ppm) water buffer solution
    const float adcConv =  3.3f / (1 << 12);    // 2-bit conversion, assume max value == ADC_VREF == 3.3 V for pico
    float voltage = adc_read() * adcConv;       // Voltage reading copensated for 3.3 volt
    float temperature = getTemperature();       // The temp sensors value in Co   

    if (voltage > 3.3 || temperature > 55.0 || temperature < 1) {
        return; // A glitch of bad readings
    }
    sdata->waterTemp = temperature;

	float ecValue=(133.42*voltage*voltage*voltage - 255.86*voltage*voltage + 857.39*voltage)*pdata->kValue;    // Before compensation
	float ecValue25 = ecValue / (1.0+0.02*(sdata->waterTemp-25.0));  // After compensation (1.0+0.02*(Measured_Temperature-25.0))
    sdata->tdsValue = (int)(ecValue25 * TdsFactor);
}

#else /* HAS_TDS */

void tdsConvert(shared_data *sdata, persistent_data *pdata)
{
    APP_UNUSED_ARG(sdata);
    APP_UNUSED_ARG(pdata);
}

#endif /* HAS_TDS */

#ifdef HAS_NET
/**
 * Return logical connection status
 */
bool net_checkconnection(void)
{
    return netIsConnected;
}

/**
 * Set logical connection status
 */
void net_setconnection(bool mode)
{
    netIsConnected = mode;
}

/**
 * Connect to AP
 */
bool wifi_connect(persistent_data *pdata)
{
    int rval = 0;
    time_t retry;
    static bool partInit;

    if (netIsConnected == nON) {
        printf("Repeated connection request ignored\n");
        return netIsConnected;
    }

    netIsConnected = nOFF;

    pdata->ssid[SSID_MAX-1] = pdata->pass[PASS_MAX-1] = '\0';

    if (!pdata->ssid[strspn(pdata->ssid, OKCHAR)] == '\0') {
        printf("SSID (%s) contains illegal characters\n", pdata->ssid);
        return netIsConnected;
    }


    if (!pdata->pass[strspn(pdata->pass, OKCHAR)] == '\0') {
        printf("WPA2 password contains illegal characters\n");
        return netIsConnected;
    }

    if ( pdata->apMode == false)
        printf("Attempt connection to %s with pass %s and country code %lu\n", pdata->ssid, pdata->pass, pdata->country);

    if (partInit == false) {

        if ((rval = cyw43_arch_init_with_country(pdata->country))) {
            printf("\ncyw43_arch_init_with_country failed: %s/%d/%lu/%d\n", __FILENAME__, __LINE__, pdata->country, rval);
            return netIsConnected;
        }

        if ( pdata->apMode == true) {

            ping_init(NULL);
            goodPing = true;

            cyw43_arch_enable_ap_mode(CYW43_HOST_NAME, APMODE_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);
            static ip4_addr_t gw, mask;
            IP4_ADDR(&gw, 192, 168, 4, 1);
            IP4_ADDR(&mask, 255, 255, 255, 0);

            // Start the dhcp server
            static dhcp_server_t dhcp_server;
            dhcp_server_init(&dhcp_server, &gw, &mask);
            printf("Initialized as AP %s, GW is 192.168.4.1, password: %s\n", CYW43_HOST_NAME, APMODE_PASSWORD);

        } else {       
            cyw43_arch_enable_sta_mode();
        }

        /**
         * this seems to be the best be can do using the predefined `cyw43_pm_value` macro:
         * cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
         * however it doesn't use the `CYW43_NO_POWERSAVE_MODE` value, so we do this instead:
         */
        //cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_NO_POWERSAVE_MODE, 20, 1, 1, 1));

        partInit = true;
    }

    if ( pdata->apMode == false) {
        for (int i = 0; i < 3; i++) {
            retry = time(NULL);
            if ((rval = cyw43_arch_wifi_connect_timeout_ms(pdata->ssid, pdata->pass, CYW43_AUTH_WPA2_AES_PSK, CONNECTION_TMO))) {
                //printf("\ncyw43_arch_wifi_connect_timeout_ms failed: %s/%d/%s/%d\n", __FILENAME__, __LINE__, ssid, rval);
                switch (rval) {
                    case CYW43_LINK_NONET:      printf("No matching SSID found");       break;
                    case CYW43_LINK_BADAUTH:    printf("Authenticatation failure");     break;
                    case CYW43_LINK_FAIL:       printf("Link failure");                 break;
                    default: printf("Connection fail with return value = %d", rval);    break;
                }
                printf(" after %llu seconds. Timeout was set to %llus\n", time(NULL)-retry, CONNECTION_TMO/1000);

                if (rval < 0 && time(NULL) - retry < 4) {
                    printf("Retry %d/3\n", i+1);
                    sleep_ms(2000);
                } else {
                    netIsConnected = nOFF;
                }
            }
            if (rval == 0) {
                char *gw;
                printf("Got I.P adress %s for %s\n", ip4addr_ntoa(netif_ip4_addr(netif_list)), CYW43_HOST_NAME);
                printf("Got gateway adress %s\n", (gw=ip4addr_ntoa(netif_ip4_gw(netif_list))));
                goodPing = netIsConnected = nON;
                ping_init(gw);  // Initialize the periodic ping function.

                break;
            }
        }
    }

    return netIsConnected;  // false in AP mode
}

/**
 * Called with results of the NTP operation
 */
static void ntp_result(NTP_T* state, int status, time_t *result)
{
    if (status == 0 && result) {
        struct tm *utc = gmtime(result);
        printf("Got ntp response: %02d/%02d/%04d %02d:%02d:%02d\n", utc->tm_mday, utc->tm_mon + 1, utc->tm_year + 1900,
               utc->tm_hour, utc->tm_min, utc->tm_sec);
        printf("Set RTC accordingly\n");
        set_rtc(utc, result);
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
 * NTP request failed callback
 */
static int64_t ntp_failed_handler(alarm_id_t id, void *user_data)
{
    APP_UNUSED_ARG(id);

    NTP_T* state = (NTP_T*)user_data;
    printf("\nntp request failed\n");
    ntp_result(state, -1, NULL);
    return 0;
}

/**
 * Make an NTP request
 */
static void ntp_request(NTP_T *state)
{
    /**
     * cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
     * You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
     * these calls are a no-op and can be omitted, but it is a good practice to use them in
     * case you switch the cyw43_arch type later.
     */
    cyw43_arch_lwip_begin();
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, NTP_MSG_LEN, PBUF_RAM);
    uint8_t *req = (uint8_t *) p->payload;
    memset(req, 0, NTP_MSG_LEN);
    req[0] = 0x1b;
    udp_sendto(state->ntp_pcb, p, &state->ntp_server_address, NTP_PORT);
    pbuf_free(p);
    cyw43_arch_lwip_end();
}

/**
 * Call back with a DNS result
 */
static void ntp_dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg)
{
    NTP_T *state = (NTP_T*)arg;
    if (ipaddr) {
        state->ntp_server_address = *ipaddr;
        printf("Got ntp address: %s for host %s\n", ip4addr_ntoa(ipaddr), hostname);
        ntp_request(state);
    } else {
        printf("ntp dns request failed for: %s\n", hostname);
        ntp_result(state, -1, NULL);
    }
}

/**
 * NTP data received
 */
static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port)
{

    APP_UNUSED_ARG(pcb);

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
        printf("\ninvalid ntp response\n");
        ntp_result(state, -1, NULL);
    }
    pbuf_free(p);
}

/**
 * Perform initialisation
 */
static NTP_T* ntp_init(void)
{
    NTP_T *state = calloc(1, sizeof(NTP_T));
    if (!state) {
        printf("\nfailed to allocate state for ntp\n");
        return NULL;
    }
    state->ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
    if (!state->ntp_pcb) {
        printf("\nfailed to create pcb for ntp\n");
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
        printf("\nntp_init failed\n");
        return rval;
    }

    ntp_server[URL_MAX-1] = '\0';
        
    for (int i=0; i <3; i++) {
        if (absolute_time_diff_us(get_absolute_time(), state->ntp_test_time) < 0 && !state->dns_request_sent) {

            // Set alarm in case udp requests are lost
            state->ntp_resend_alarm = add_alarm_in_ms(NTP_RESEND_TIME, ntp_failed_handler, state, true);

            /**
             * cyw43_arch_lwip_begin/end should be used around calls into lwIP to ensure correct locking.
             * You can omit them if you are in a callback from lwIP. Note that when using pico_cyw_arch_poll
             * these calls are a no-op and can be omitted, but it is a good practice to use them in
             * case you switch the cyw43_arch type later.
             */
            cyw43_arch_lwip_begin();
            int err = dns_gethostbyname(ntp_server, &state->ntp_server_address, ntp_dns_found, state);
            cyw43_arch_lwip_end();

            state->dns_request_sent = true;
            if (err == ERR_OK) {
                ntp_request(state); // Cached result
                break;
            } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
                printf("\ndns request failed(%d)\n", i+1);
                ntp_result(state, -1, NULL);
            }
        }
    }
    free(state);

    return true;
}

/**
 * User entry point for an NTP request
 */
bool netNTP_connect(persistent_data *pdata)
{
    bool rval = false;
    static char buf_ntp[URL_MAX+2];
    static bool isConnected;

    if (pdata->apMode == true)
        return true;

    if (netIsConnected == nON && isConnected == nOFF) {

        if (!strncmp(pdata->ntp_server, "0.0.0.0", 7)) {
            // Default gw host assumed to hold NTP services
            strncpy(buf_ntp, ip4addr_ntoa(netif_ip4_gw(netif_list)), sizeof(buf_ntp)-1);
        } else {
            strcpy(buf_ntp, pdata->ntp_server);
        }
        if (net_ntp(buf_ntp) == false) {
            return rval;
        }
        for (int i = 0; i < 4; i++) {   // Wait max for the ntp_result() callback
            sleep_ms(1000);
            if (rtcIsSetDone == true ) {
                rtcIsSetDone = false;
                rval = true;
                isConnected = rval =  nON;
                break;
            }
        }
    }

    return rval;
}

/**
 * Callback for wifi_scan()
 * Populate sdata->wfd[] with unique host names and rssis
 */
static int scan_result(void *env, const cyw43_ev_scan_result_t *result)
{
    shared_data *sdata = env;

    int indx;

    if (result && result->ssid_len) {

        for (indx=0; indx < sdata->ssidCount; indx++) {
            if (strncmp((char *)result->ssid, sdata->wfd[indx].ssid, SSID_MAX) == 0)
                break; // found duplicate
        }

        if (sdata->ssidCount < SSID_LIST && indx == sdata->ssidCount) {
            strncpy(sdata->wfd[sdata->ssidCount].ssid, (char *)result->ssid, SSID_MAX); // Add new
            sprintf(sdata->wfd[sdata->ssidCount].rssi, "%ddBm", result->rssi);
            sdata->ssidCount++;
        }
#if 0
        if (sdata->ssidCount) {
            printf("Results:\n");
            for(int i=0; i < sdata->ssidCount; i++) {
                printf("ssid: %s rssi: %s\n", sdata->wfd[i].ssid, sdata->wfd[i].rssi);
            }
        }
#endif
    }
    return 0;
}

/**
 * Scan the 2.4 MHz wifi network
 */
void wifi_scan(int scanTurns, shared_data *sdata)
{
    absolute_time_t scan_test = nil_time;
    bool scan_in_progress = false;

    for (int i = 0; i < scanTurns; i++) {
        if (absolute_time_diff_us(get_absolute_time(), scan_test) < 0) {
            if (!scan_in_progress) {
                cyw43_wifi_scan_options_t scan_options = {0};
                int err = cyw43_wifi_scan(&cyw43_state, &scan_options, sdata, scan_result);
                if (err == 0) {
                    //printf("\nPerforming wifi scan\n");
                    scan_in_progress = true;
                } else {
                    //printf("Failed to start scan: %d\n", err);
                    scan_test = make_timeout_time_ms(1000); // wait 1s and scan again
                }
            } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
                scan_test = make_timeout_time_ms(1000); // wait 1s and scan again 
                scan_in_progress = false; 
            }
        }
    }
}

/**
 * Look up an AP in the air from a previosly completed wifi_scan() call
 */
bool wifi_find(char *ap, shared_data *sdata)
{
    bool rval = false;

    if (sdata->ssidCount) {
        for(int i=0; i < sdata->ssidCount; i++) {
            if (!strncmp(sdata->wfd[i].ssid, ap, strlen(ap))) {
                rval = true;
                break;
            }
        }
    }

    return rval;
}

#endif /* HAS_NET */

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

/**
 * Restore clocks after a dormant sleep.
 */
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

/**
 * Go dormant after a lwip status check.
 */
void goDormant(uint8_t dpin, persistent_data *pdata, shared_data *sdata, int lostPing)
{

    static char buffer_t[60];
    time_t curtime = time(NULL);

#if defined HAS_NET
    /** 
     * The lwip stack is prone to run out of listening PCBs from time to time (at least in my environment)
     * and it is difficult to recover from that situation without a re-boot. The variations in
     * this behavior is very inconsistent over a day period, and for this reason this sanity check will
     * attempt a reboot of the pico_w.
     */
    if (sdata->outOfPcb > MAX_BAD_PCBS || lostPing > MAX_BAD_PINGS) {

        if (pdata->rebootTime + SHOUR/4 < curtime) {   // Avoid quick boot loops
            pdata->rebootTime = curtime;
            pdata->rebootCount++;
            if (write_flash(pdata) == false) {
                printf("Failed to save reboot data\n");
            } else {
                printf("\nSystem reboot due to internal unstable network properties\n");
                watchdog_enable(1, 1);  // Reboot
                while(1);
                /** NOT REACHED **/
            }
        }
    }
#else
    APP_UNUSED_ARG(pdata); APP_UNUSED_ARG(sdata); APP_UNUSED_ARG(lostPing);
#endif

    printf("Skipping dormant for now\n"); return;   // Until tested firmly - also it only works in "pico" mode not "pico_w"

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

/** 
 * The pico w is prone to lose network connectivity from time to time (at least in my environment)
 * and it is difficult to recover from that situation without a re-boot. The variations in
 * this behavior is very inconsistent over a day period, and for this reason this "ping" probing
 * will recover the connectivity silently in the background. It will also recover from 
 * a situation where the AP is unreacable momentarely.
 * The pinged node in this configuration is the (AP) default gateway that obviously
 * must be receptive for ICMP accesses.
 *
 * The application API for this features are:
 * void ping_send_now(void)
 * bool ping_status(void)
 */
#if LWIP_RAW // Should be configured for in lwipopts.h

/** 
 * Prepare a echo ICMP request
 */
static void ping_prepare_echo( struct icmp_echo_hdr *iecho, u16_t len)
{
    size_t i;
    size_t data_len = len - sizeof(struct icmp_echo_hdr);

    ICMPH_TYPE_SET(iecho, ICMP_ECHO);
    ICMPH_CODE_SET(iecho, 0);
    iecho->chksum = 0;
    iecho->id     = PING_ID;
    iecho->seqno  = lwip_htons(++ping_seq_num);

    // fill the additional data buffer with some data
    for(i = 0; i < data_len; i++) {
        ((char*)iecho)[sizeof(struct icmp_echo_hdr) + i] = (char)i;
    }

    iecho->chksum = inet_chksum(iecho, len);
}

/**
 * Send the ping
 */
static void ping_send(struct raw_pcb *raw, const ip_addr_t *addr)
{
    struct pbuf *p;
    struct icmp_echo_hdr *iecho;
    size_t ping_size = sizeof(struct icmp_echo_hdr) + PING_DATA_SIZE;

#if defined NET_DEBUG
    static int seq;
    // ESC[2K\r = erase line and return cursor
    printf("%c[2K\r(%d)ping %s with timeout %.1fs. ", 0x1b, ++seq, ip4addr_ntoa(addr), (float)PING_DELAY/1000);
    fflush(stdout);
#endif

    goodPing = false;

    LWIP_ASSERT("ping_size <= 0xffff", ping_size <= 0xffff);

    p = pbuf_alloc(PBUF_IP, (u16_t)ping_size, PBUF_RAM);
    if (!p) {
        return;
    }

    if ((p->len == p->tot_len) && (p->next == NULL)) {
        iecho = (struct icmp_echo_hdr *)p->payload;

        ping_prepare_echo(iecho, (u16_t)ping_size);

        raw_sendto(raw, p, addr);
    }

    pbuf_free(p);
}

/**
 * Ping timeout callback
 */
static void ping_timeout(void *arg)
{
    struct raw_pcb *pcb = (struct raw_pcb*)arg;

    LWIP_ASSERT("ping_timeout: no pcb given!", pcb != NULL);
    printf("Ping timeout: send again.\n");
    sleep_ms(500);
    ping_send(pcb, ping_target);
}

/**
 * Ping using the raw ip
 */
static u8_t ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr)
{
    struct icmp_echo_hdr *iecho;
    LWIP_UNUSED_ARG(arg);
    LWIP_UNUSED_ARG(pcb);
    LWIP_UNUSED_ARG(addr);
    LWIP_ASSERT("p != NULL", p != NULL);

    if ((p->tot_len >= (PBUF_IP_HLEN + sizeof(struct icmp_echo_hdr))) && pbuf_remove_header(p, PBUF_IP_HLEN) == 0) {
        iecho = (struct icmp_echo_hdr *)p->payload;

        if ((iecho->id == PING_ID) && (iecho->seqno == lwip_htons(ping_seq_num))) {
            pbuf_free(p);
            goodPing = true;
            sys_untimeout(ping_timeout, pcb);
#if defined NET_DEBUG
            printf("Got good ping.\r");
            fflush(stdout);
#endif

            return 1; // eat the packet
        }

        // not eaten, restore original packet
        pbuf_add_header(p, PBUF_IP_HLEN);
    }

    goodPing = false;
    return 0; // don't eat the packet
}

/**
 * Set up the ping
 */
static void ping_raw_init(const char *ip)
{
    if (pRawisInit == false) {
        ping_pcb = raw_new(IP_PROTO_ICMP);
        LWIP_ASSERT("ping_pcb != NULL", ping_pcb != NULL);

        raw_recv(ping_pcb, ping_recv, NULL);
        raw_bind(ping_pcb, IP_ADDR_ANY);

        printf("Ping target %s initialized.\n", ip);
        pRawisInit = true;
    }
}

/**
 * User entry to send a (one) ping package
 */
void ping_send_now(void)
{
    if (pRawisInit == true) {
        LWIP_ASSERT("ping_pcb != NULL", ping_pcb != NULL);
        sys_timeout(PING_DELAY, ping_timeout, ping_pcb);
        ping_send(ping_pcb, ping_target);
    }
}

/**
 * Entry for a ping init
 */
static void ping_init(const char *ip)
{

    if (ip == NULL)
        return;

    static ip_addr_t no;
    ip4addr_aton(ip, &no);
    ping_target = &no;

    ping_raw_init(ip);
}

/**
 * User entry to pick up the result of a ping
 */
bool ping_status(void)
{
    return goodPing;
}

#endif /* LWIP_RAW */

