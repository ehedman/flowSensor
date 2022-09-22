/*****************************************************************************
* | File      	:   water-ctrl.c
* | Author      :   erland@hedmanshome.se
* | Function    :   Messure water flow and filter properties.
* | Info        :   
* | Depends     :   Rasperry Pi Pico W, Waveshare Pico LCD 1.14 V2, DS3231 piggy-back RTC module
*----------------
* |	This version:   V1.0
* | Date        :   2022-08-22
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
#include <stdarg.h>
#include <time.h>
#include <pico/stdlib.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>
#include "EPD_Test.h"
#include "LCD_1in14.h"
#include "waterbcd.h"
#include "water-ctrl.h"
#ifdef HAS_NET
#include "ssi.h"
#endif /* HAS_NET */

/**
 * For debug purposes this app enters flash
 * mode when the reset button is pressed.
 */
#define FLASHMODE true

/**
 * Monitor the HALL sensor run state by frequency measurement.
 */
#include <hardware/pwm.h>
#include <hardware/clocks.h>
#include <pico/multicore.h>
#define FLAG_VALUE          123     // Multicore check flag

/**
 * Display properties
 */
//#define SMALL_FONT  1

#ifdef SMALL_FONT
#define MAX_CHAR            21
#define MAX_LINES           7
#define FONT                Font16
#define FONT_PADDING        16
#else
#define MAX_CHAR            14
#define MAX_LINES           6
#define FONT                Font24
#define FONT_PADDING        24
#endif
#define DEF_PWM             50      // Display brightness
#define LOW_PWM             4
#define HDR_OK              GREEN
#define HDR_ERROR           0xF8C0  // Reddish
#define HDR_INFO            YELLOW

#define LCD_HEIGHT          LCD_1IN14_HEIGHT
#define LCD_WIDTH           LCD_1IN14_WIDTH

#define POLLRATE            250     // Main loop interval in ms

static UWORD *BlackImage;
static char HdrStr[100]         = { "Header" };
static uint16_t HdrTxtColor     = HDR_OK;
static bool FirstLogline        = true;
static bool FirmwareMode        = FLASHMODE;

/**
 * Joystick buttons on the LCD_1in14
 */
static const uint8_t JsDown =       18;
static const uint8_t JsUp =         2;
static const uint8_t JsRight =      28; //Pin 20 currently in conflict with the RTCs I2C_SCL, so it is hardware rerouted to 28.
static const uint8_t JsLeft =       16;
static const uint8_t JsOk =         3;

/**
 * Right hand buttons on the LCD_1in14
 */
static const uint8_t StatusButt =   15;
static const uint8_t ResetButt =    17;

/**
 * Use this GPIO pin to wake us up from dormant.
 * Could be connected directly to the HzmeasurePin
 */
static const uint8_t dWakeUpPin =   4;

/**
 * FLow sesnsing
 */
static const uint HzmeasurePin =    5;  // Square wave 1-110Hz
static uint16_t FlowFreq =          0;  // Live flow frequency

/**
 * Session tick counter from the HAL sensor.
 */
static int sessTick;

/**
 * Persistent data in flash
 */
persistent_data pdata;

/**
 * Shared data for this app
 */
shared_data sdata;

/**
 * Format the text header.
 */
static void printHdr(const char *format , ...)
{
    static char txt[MAX_CHAR*2];
    va_list arglist;

    va_start(arglist, format);
    vsprintf(txt, format, arglist);
    va_end(arglist);

    txt[MAX_CHAR-1] = '\0';

    // Center align and trim with white spaces
    int padlen = (MAX_CHAR - (int)strlen(txt)) / 2;
    sprintf(HdrStr, "%*s%s%*s", padlen, "", txt, 0, "");

    Paint_DrawRectangle(0,0,LCD_HEIGHT,FONT_PADDING,HdrTxtColor, DOT_PIXEL_1X1,DRAW_FILL_FULL);
    Paint_DrawString_EN(0, 0, HdrStr, &FONT, HdrTxtColor, BLACK);

    // Refresh the picture in RAM to LCD
    LCD_1IN14_Display(BlackImage);

}

/**
 * Text display with colored fixed header and scrolled text.
 */
static void printLog(const char *format , ...)
{
    static char buf[MAX_CHAR*2];
    static va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);

    int curLine;
    static char lines[MAX_LINES][MAX_CHAR];
    if (FirstLogline == true) {
        for (int i=0; i <MAX_LINES; i++) {
            memset(lines[i], 0, MAX_CHAR);
        }

        FirstLogline = false;
    }

    buf[MAX_CHAR-1] = '\0';
    int blenght = (int)strlen(buf);

    // To stdio serial also
    printf("%s\r\n", buf);

    for (curLine=0; curLine <MAX_LINES; curLine++) {
        if (lines[curLine][0] == (unsigned char)0x0) {
            strncpy(lines[curLine], buf, blenght < MAX_CHAR? blenght: MAX_CHAR);
            break;
        }

        // Scroll
        if (curLine >= MAX_LINES-1) {
            Paint_Clear(WHITE);
            for (curLine=1; curLine <MAX_LINES; curLine++) {
                strncpy(lines[curLine-1], lines[curLine], MAX_CHAR);
            }
            strncpy(lines[MAX_LINES-1], buf, blenght < MAX_CHAR? blenght: MAX_CHAR);
        }
    }

    // Print header and text
    for (curLine=0; curLine <MAX_LINES; curLine++) {
            if (curLine == 0) { // Plug in the header first             
                Paint_DrawRectangle(0,0,LCD_HEIGHT,FONT_PADDING, HdrTxtColor, DOT_PIXEL_1X1,DRAW_FILL_FULL);
                Paint_DrawString_EN(0, 0, HdrStr, &FONT, HdrTxtColor, BLACK);
            }
            if (lines[curLine][0] != (unsigned char)0x0) {
                Paint_DrawString_EN(1, (curLine+1)*FONT_PADDING, lines[curLine], &FONT, WHITE, BLACK);
            }

    }

    // Refresh the picture in RAM to LCD
    LCD_1IN14_Display(BlackImage);

}

/**
 * Reset the log
 */
static void clearLog(int hdrColor)
{
    HdrTxtColor = hdrColor;
    FirstLogline = true;
    Paint_Clear(WHITE); 
}

/**
 * Display initialization.
 * Display: https://www.waveshare.com/wiki/Pico-LCD-1.14 (V2)
 * SDK: https://www.waveshare.com/w/upload/0/06/Pico-LCD-1.14.zip
 */
static int initDisplay(void)
{
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    // LCD Init
    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(WHITE);

    UDOUBLE Imagesize = LCD_HEIGHT*LCD_WIDTH*2;

    if((BlackImage = (UWORD *)malloc(Imagesize)) == NULL) {
        return -1;
    }

    // Create a new image cache named IMAGE_RGB and fill it with white
    Paint_NewImage((UBYTE *)BlackImage, LCD_1IN14.WIDTH, LCD_1IN14.HEIGHT, 0, WHITE);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);

    return 0;
}

/**
 * Initialize all gpio pins here.
 */
static void gpioInit(void)
{
        // Initialize our pins
        gpio_init(JsUp);
        gpio_set_dir(JsUp, GPIO_IN);
        gpio_pull_up(JsUp);

        gpio_init(JsDown);
        gpio_set_dir(JsDown, GPIO_IN);
        gpio_pull_up(JsDown);

        gpio_init(JsOk);
        gpio_set_dir(JsOk, GPIO_IN);
        gpio_pull_up(JsOk);

        gpio_init(JsRight);
        gpio_set_dir(JsRight, GPIO_IN);
        gpio_pull_up(JsRight);

        gpio_init(JsLeft);
        gpio_set_dir(JsLeft, GPIO_IN);
        gpio_pull_up(JsLeft);

        gpio_init(StatusButt); 
        gpio_set_dir(StatusButt, GPIO_IN);
        gpio_pull_up(StatusButt);

        gpio_init(ResetButt);
        gpio_set_dir(ResetButt, GPIO_IN);
        gpio_pull_up(ResetButt);

}

/**
 * This is a free running core1 function.
 * Messure flow pulses from the flow HALL sensor.
 * The flow pulses should then be represented as
 * a square wave stream (around 2.5v peak) to the
 * PWM B pin (GP5) of the Pico.
 */
static uint16_t measureFrequency(uint gpio, int pollRate)
{

    static uint slice_num;
    uint16_t counter;
    static bool init;

    if (init == false) {
        // Only the PWM B pins can be used as inputs.
        assert(pwm_gpio_to_channel(gpio) == PWM_CHAN_B);
        slice_num = pwm_gpio_to_slice_num(gpio);

        // Count once for every cycles the PWM B input is high
        pwm_config cfg = pwm_get_default_config();
        pwm_config_set_clkdiv_mode(&cfg, PWM_DIV_B_RISING);
        pwm_config_set_clkdiv(&cfg, 1.f);   // Set by default, increment count for each rising edge
        pwm_init(slice_num, &cfg, false);   // False means don't start pwm
        gpio_set_function(gpio, GPIO_FUNC_PWM);
        init = true;
        sessTick = 0;
    }

    pwm_set_counter(slice_num, 0);

    pwm_set_enabled(slice_num, true);
    sleep_ms(pollRate);
    pwm_set_enabled(slice_num, false);

    counter = pwm_get_counter(slice_num);

    sessTick += counter;

    return (counter * (1000/pollRate));

}

/**
 * The thread entry for the measureFrequency() function.
 */
static void core1Thread(void)
{

    multicore_fifo_push_blocking(FLAG_VALUE);

    uint32_t g = multicore_fifo_pop_blocking();

    if (g == FLAG_VALUE) {

        while(1) 
        {
            int f = measureFrequency(HzmeasurePin, 1000);

            FlowFreq = f;   // Enter result to global space
        }

    } else {
            printLog("Cannot run core1");
    }

}

/**
 * Main control loop.
 */
static bool waterCtrlInit(void)
{
    static char qstr[20];

    sprintf (qstr, "Q%.2f", pdata.sensFq);

    if (initDisplay() != 0) {
        printf("Init Display failed: %s/%d\n", __FILENAME__, __LINE__);
        return false;
    }

    gpioInit();

    HdrTxtColor = HDR_OK;

    DEV_SET_PWM(DEF_PWM);

    // Splash screen
    Paint_DrawImage(water_crane_map,0,0,LCD_HEIGHT,LCD_WIDTH);
    Paint_DrawString_EN(2, 118, sdata.versionString , &Font16, WHITE, BLACK);
    Paint_DrawString_EN(60, 118, qstr , &Font16, WHITE, BLACK);
    LCD_1IN14_Display(BlackImage);

    multicore_launch_core1(core1Thread);

    // Wait for it to start up
    uint32_t g = multicore_fifo_pop_blocking();

    if (g != FLAG_VALUE) {
        HdrTxtColor = HDR_ERROR;
        printLog("HAL sens NOK");
        printLog("SYS STOPPED");
        return false;
    } else {
        multicore_fifo_push_blocking(FLAG_VALUE);
    }

#if 0
    while(1) {
        sleep_ms(250);
        printLog("FlowFreq=%d", FlowFreq);  // Flow test
    }
#endif


#if 0
    for (int i = 0; i < MAX_LINES; i++)
    {
        printLog("line = %d", i);   // Scroll test
    }
    sleep_ms(6000);
#endif

    return true;

}

/**
 * This is the "main" entry
 */
void water_ctrl(void)
{
    static time_t curtime;
    static struct tm *info_t;
    static char buffer_t[60];
    float volTick = 0.0;
    float sessLitre = 0.0;
    float litreMinute = 0.0;
    int delSave = 0;
    bool doSave = false;
    int tmo;

    stdio_init_all();

    memset(&sdata, 0, sizeof(shared_data));
    sdata.versioMajor =  DigiFlow_VERSION_MAJOR;
    sdata.versionMinor = DigiFlow_VERSION_MINOR;
    sdata.inactivityTimer = INACTIVITY_TIME;
    sprintf(sdata.versionString, "%d.%d", sdata.versioMajor, sdata.versionMinor);

    memset(&pdata, 0, sizeof(persistent_data));

    if (read_flash(&pdata) == false) {
        panic("\nread_flash failed: %s line %d\n", __FILENAME__, __LINE__);
    }

    if (pdata.idt != IDT) {  // Set up defaults
        printf("Set default flash data\n");
#ifdef HAS_NET
        strcpy(pdata.ssid, WIFI_SSID);
        strcpy(pdata.pass,WIFI_PASS);
        strcpy(pdata.ntp_server, NTP_SERVER);
        pdata.country = WIFI_COUNTRY;
#endif /* HAS_NET */
        pdata.tankVolume = TANK_VOLUME;
        pdata.totVolume = 0.0;
        pdata.filterVolume = 0.0;
        pdata.filterAge = time(NULL) + SDAY;
        pdata.sensFq = SENS_FQC;
        pdata.version = (float)atof(sdata.versionString);
        pdata.rebootCount = 0;
        pdata.rebootTime = time(NULL);
        pdata.idt = IDT;
        write_flash(&pdata);
    }

    if (waterCtrlInit() == false) {
        panic("\nwaterCtrlInit failed: %s line %d\n", __FILENAME__, __LINE__);
    }

    if (pdata.filterAge <= 0) {
        pdata.filterAge = time(NULL) + SDAY; // Add a day to start with
    }

    if (pdata.sensFq <= 0.0 || pdata.sensFq > 14.00 ) {
        pdata.sensFq = SENS_FQC;
        printf("Using default value for FQ: %.2f\n", SENS_FQC);
    }

    if (pdata.version != (float)atof(sdata.versionString)) {
        pdata.version = (float)atof(sdata.versionString);
    }

#ifdef HAS_NET
    if (wifi_connect(pdata.ssid, pdata.pass, pdata.country) == true) {
        if (netNTP_connect(pdata.ntp_server) == false) {
            printf("\nWiFi netNTP_operation failed\n");
        }
        init_httpd();
        sleep_ms(1000);
    }
#endif

    sdata.startTime = curtime = time(NULL);
    printf("Current RTC time is %s", ctime_r(&curtime, buffer_t));

    tmo = 3;

    while (1) {

        while(1) {

            volTick = (1.0/60)/pdata.sensFq;    // sensFq can be updated from web. Re-evaluate here.

            if (FlowFreq > 0.0) {
                litreMinute = (FlowFreq * 1 / pdata.sensFq);
                sessLitre = (sessTick * volTick);
                clearLog(HDR_OK);
                printHdr("Flowing");
                printf("FlowFreq=%dHz\n", FlowFreq);
                printLog("FLOW=%.1f L/M", litreMinute);
                printLog("USED=%.0f L", sessLitre + pdata.totVolume);
                DEV_SET_PWM(DEF_PWM);

            } else if (tmo-- <= 0) {
                info_t = localtime( &pdata.filterAge);
                strftime(buffer_t, sizeof(buffer_t), "%d/%m/%y", info_t);
                clearLog(HDR_INFO);
                printHdr("No FLow");
                printLog("USE=%.0fL", pdata.totVolume + sessLitre);
                printLog("REM=%.0fL", pdata.tankVolume - (pdata.totVolume + sessLitre));
                printLog("FXD=%s", buffer_t);
                break;
            }
            sleep_ms(2000);
        }
 
        tmo = 3;

        printf("Enter power save mode\n");

        if (sessLitre > 0.0) {
            pdata.totVolume += sessLitre;
            pdata.filterVolume += sessLitre;
            sessLitre = sessTick = 0;
            delSave = 120;  // Assuming reuse anytime soon
            doSave = true;  // .. so don't be to agressive to save to flash
        }

        while (FlowFreq == 0.0) {

#ifdef HAS_NET
            static int tryConnect;
            static int tryPing;
            static int pingInterval;
            static int tryScan;
            static int lostPing;

            if (pingInterval++ >= 20) {
                ping_send_now();
                pingInterval = 0;
            } else sleep_ms(1000);

            if (ping_status() == false && tryPing++ >= 10) {
                net_setconnection(nOFF);
                printf("\nLost ping sequences %d times\n", ++sdata.lostPing);
                tryPing = 0;  
                lostPing++;   
            } else if (ping_status() == true) {
                lostPing = tryConnect = tryPing = 0;
                net_setconnection(nON);
            }

#ifdef NET_SANITY_CHECK  // Undef this if tcp_in-c.patch is not applied
            {
                extern int outOfPcb;

                if (outOfPcb > 0 && outOfPcb != sdata.outOfPcb ) {
                    printf("\noutOfPcb = %d\n", outOfPcb);
                    sdata.outOfPcb = outOfPcb;
                }
            }
#endif /* NET_SANITY_CHECK */

            if (sdata.outOfPcb > MAX_BAD_PCBS || lostPing > MAX_BAD_PINGS) {
                    sdata.inactivityTimer = 1;  //  Time to attempt a reboot
            }

            if (tryScan++ > 24) {
                memset(sdata.wfd, 0, sizeof(sdata.wfd));
                sdata.ssidCount = 0;
                wifi_scan(2);   // Fill wfd with fresh findings
                tryScan = 0;
            }

            if (tryConnect++ >= 10 && net_checkconnection() == false && wifi_find(pdata.ssid) == true) {
                tryConnect = 0;

                if (wifi_connect(pdata.ssid, pdata.pass, pdata.country) == true) {
                    (void)netNTP_connect(pdata.ntp_server);
                    sleep_ms(2000);
                    init_httpd();
                    tryScan = pingInterval = tryPing = 0;
                }
                continue;
            }
#else
            sleep_ms(1000);
#endif /* HAS_NET */
            DEV_SET_PWM(0); 

            if (doSave == true && delSave-- <= 0) { // Avoid repeated saves for rapid events
                printf("Delayed save\n");
                if (write_flash(&pdata) == false) {
                    printf("Delayed save failed\n");
                }
                doSave = false;
                delSave = 0;
            }
            
            if (!gpio_get(JsRight)) {       // Adjust filter expiration date +
                curtime = time(NULL);
                clearLog(HDR_INFO);
                printHdr("FLT INC DATE");
                if (pdata.filterAge < curtime+S6MONTH) { // Don't move to far
                    pdata.filterAge += SMONTH; // Add a month
                } else {                 
                    pdata.filterAge = curtime+S6MONTH;  // Max
                }

                info_t = localtime(&pdata.filterAge);
                strftime(buffer_t, sizeof(buffer_t), "%d/%m/%y", info_t);
                printLog("FXD=%s", buffer_t);
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(1000);
                delSave = 10;
                doSave = true;
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }

            if (!gpio_get(JsLeft)) {        // Adjust filter expiration date -
                curtime = time(NULL);
                clearLog(HDR_INFO);
                printHdr("FLT DEC DATE");
                if (pdata.filterAge > curtime +SMONTH) { // Don't move back in time
                    pdata.filterAge -= SMONTH;
                } else {
                    pdata.filterAge = curtime +SDAY; // Add a day fromn now
                }
                info_t = localtime( &pdata.filterAge);
                strftime(buffer_t, sizeof(buffer_t), "%d/%m/%y", info_t);
                printLog("FXD=%s", buffer_t);
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(1000);
                delSave = 10;
                doSave = true;
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }

            if (!gpio_get(JsOk)) {                  // Reset filter data (filter replaced)
                clearLog(HDR_INFO);
                printHdr("NEW FILTER");
                printLog("New period");
                pdata.filterAge = time(NULL) + FLTR_LIFETIME;
                pdata.filterVolume = 0.0;
                info_t = localtime( &pdata.filterAge);
                strftime(buffer_t, sizeof(buffer_t), "%m/%d/%y", info_t);
                printLog("FXD=%s", buffer_t);
                printLog("FLV=0.0");
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(2000);
                delSave = 1;
                doSave = true;
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }
 
            if (!gpio_get(StatusButt)) {
                clearLog(HDR_INFO);
#if defined HAS_NET && defined NET_DEBUG  // Show statistics to console only
                printf("\nLost ping sequences=%d, out of pcbs=%d, lifetime lwip exhausting reboots=%d\n", sdata.lostPing, sdata.outOfPcb, pdata.rebootCount);
                printf("Last lwip exhausting reboot time: %s", ctime_r(&pdata.rebootTime, buffer_t));
#endif
                printHdr("STATUS");
                printLog("USE=%.0fL", pdata.totVolume);
                printLog("REM=%.0fL", pdata.tankVolume - pdata.totVolume);
                info_t = localtime(&pdata.filterAge);
                strftime(buffer_t, sizeof(buffer_t), "%d/%m/%y", info_t);
                printLog("FXD=%s", buffer_t);
                printLog("FLV=%.0fL", pdata.filterVolume);
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(10000);

                if (pdata.filterAge < time(NULL)) {
                    clearLog(HDR_ERROR);
                    printHdr("FILTER past FXD");
                    printLog("FLTR past FXD");
                    sleep_ms(5000);
                } else if (pdata.filterAge - SWEEK*2 < time(NULL)) {
                    clearLog(HDR_INFO);
                    printHdr("FILTER AGING");
                    printLog("FLTR near FXD");
                    sleep_ms(5000);
                }
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }

            if (!gpio_get(JsUp) && pdata.sensFq < 14.2) {              // Fine-tune the sensors' Q value +
                clearLog(HDR_INFO);
                printHdr("FQ INC");
                pdata.sensFq += 0.20;
                volTick = (1.0/60)/pdata.sensFq;
                printLog("Adj FQ + 0.20");
                printLog("New FQ = %.2f", pdata.sensFq);
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(2000);
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }

            if (!gpio_get(JsDown) && pdata.sensFq > 4.2) {     // Fine-tune the sensors' Q value -
                clearLog(HDR_INFO);
                printHdr("FQ DEC");
                pdata.sensFq -= 0.20;
                volTick = (1.0/60)/pdata.sensFq;
                printLog("Adj FQ - 0.20");
                printLog("New FQ = %.2f", pdata.sensFq);
                DEV_SET_PWM(DEF_PWM);
                sleep_ms(2000);
                sdata.inactivityTimer = INACTIVITY_TIME;
                continue;
            }

            if (!gpio_get(ResetButt)) {         // Reset total volume and restart this app
                clearLog(HDR_ERROR);
                printHdr("RESET");
                DEV_SET_PWM(DEF_PWM);

                if (FirmwareMode == true) {
                    printLog("Go Firmware");
                    (void)write_flash(&pdata);
                    sleep_ms(3000);
                    reset_usb_boot(0,0);
                    /** NOT REACHED **/
                }

                pdata.totVolume = 0.0;
                if (write_flash(&pdata) == false) {
                    printLog("SAVE-ERROR");
                } else {
                    printLog("USED = 0");
                }

                sleep_ms(3000);
            }

            if (--sdata.inactivityTimer == 0) {
                goDormant(dWakeUpPin, &pdata, &sdata, lostPing);    // Save some more energy (some ticks may be lost at resume)
                sdata.inactivityTimer = INACTIVITY_TIME;
                break;  // We do have flow now
            }
        }

        printf("Woken up!!\n");
    }

} /*  water_ctrl() */


