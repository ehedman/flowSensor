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
#include <pico/sleep.h>
#include <hardware/watchdog.h>
#include "EPD_Test.h"
#include "LCD_1in14.h"
#include "waterbcd.h"
#include "water-ctrl.h"
#include <pico/cyw43_arch.h>

/**
 * For debug purposes this app enters flash
 * mode when the reset button is pressed.
 */
#define FLASHMODE false

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

#define POLLRATE            250     // Main loop interval in ms

/**
 * Sensor characteristics:
 * SENS_FQC from sensors datasheet (F) or better
 * messured in actual installation environment.
 */
#define SENS_FQC    11.00           // Pulse Characteristics: F=7.8 x Q (Rate of flow (Q) in litres/min)
//#define PUPM_CAPM   5.2             // Pump Litres per minute
//#define VOLTICK     (PUPM_CAPM/60)/(PUPM_CAPM*SENS_FQC) // Volume per sessTick @ 1 HAL pulse

static UWORD *BlackImage;
static char HdrStr[100]     = { "Header" };
static int HdrTxtColor      = HDR_OK;
static bool FirstLogline    = true;
static bool FirmwareMode    = FLASHMODE;

/**
 * Joystick buttons on the LCD_1in14
 */
static const uint8_t JsDown =       18;
static const uint8_t JsUp =         2;
static const uint8_t JsRight =      20;
static const uint8_t JsLeft =       16;
static const uint8_t JsOk =         3;

/**
 * Right hand buttons on the LCD_1in14
 */
static const uint8_t TestButt =     15;
static const uint8_t ResetButt =    17;

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
static persistent_data pdata;

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

    txt[MAX_CHAR] = '\0';

    // Center align and trim with white spaces
    int padlen = (MAX_CHAR - strlen(txt)) / 2;
    sprintf(HdrStr, "%*s%s%*s", padlen, "", txt, padlen, "");
  
    Paint_DrawString_EN(4, 0, HdrStr, &FONT, HdrTxtColor, BLACK);

    // Refresh the picture in RAM to LCD
    LCD_1IN14_Display(BlackImage);

}

/**
 * Text display with colored fixed header and scrolled text.
 */
void printLog(const char *format , ...)
{
    static char buf[100];
    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);

    int curLine;
    static char lines[MAX_LINES][MAX_CHAR+1];
    if (FirstLogline == true) {
        for (int i=0; i <MAX_LINES; i++) {
            memset(lines[i], 0x0, MAX_CHAR+1);
        }

        FirstLogline = false;
    }

    printf("%s\n", buf);

    for (curLine=0; curLine <MAX_LINES; curLine++) {
        if (lines[curLine][0] == (unsigned char)0x0) {
            strncpy(lines[curLine], buf, MAX_CHAR);
            break;
        }

        // Scroll
        if (curLine >= MAX_LINES-1) {
            Paint_Clear(WHITE);
            for (curLine=1; curLine <MAX_LINES; curLine++) {
                strncpy(lines[curLine-1], lines[curLine], MAX_CHAR);
            }
            strncpy(lines[MAX_LINES-1], buf, MAX_CHAR);
        }
    }

    // Print header and text
    for (curLine=0; curLine <MAX_LINES; curLine++) {
            if (curLine == 0) { // Plug in the header first
                Paint_DrawString_EN(4, 0, HdrStr, &FONT, HdrTxtColor, BLACK);
            }
            if (lines[curLine][0] != (unsigned char)0x0) {
                Paint_DrawString_EN(1, (curLine+1)*FONT_PADDING, lines[curLine], &FONT, WHITE, BLACK);
            }

    }

    // Refresh the picture in RAM to LCD
    LCD_1IN14_Display(BlackImage);

}

static void clearLog(int hdrColor)
{
    HdrTxtColor = hdrColor;
    FirstLogline = true;
    Paint_Clear(WHITE);
}

/**
 * Display initialization.
 * Display: https://www.waveshare.com/wiki/Pico-LCD-1.14 (V1)
 * SDK; https://www.waveshare.com/w/upload/2/28/Pico_code.7z
 */
static int initDisplay(void)
{
    if (DEV_Module_Init() != 0) {
        return -1;
    }

    // LCD Init
    LCD_1IN14_Init(HORIZONTAL);
    LCD_1IN14_Clear(WHITE);

    UDOUBLE Imagesize = LCD_1IN14_HEIGHT*LCD_1IN14_WIDTH*2;

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

        gpio_init(TestButt); 
        gpio_set_dir(TestButt, GPIO_IN);
        gpio_pull_up(TestButt);

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

#if 0
int goDormant()
{

    printf("Hello Dormant!\n");

    printf("Switching to XOSC\n");
    uart_default_tx_wait_blocking();

    // UART will be reconfigured by sleep_run_from_xosc
    sleep_run_from_xosc();

    printf("Running from XOSC\n");
    uart_default_tx_wait_blocking();

    printf("XOSC going dormant\n");
    uart_default_tx_wait_blocking();

    // Go to sleep until we see a high edge on GPIO 10
    sleep_goto_dormant_until_edge_high(22);

    uint i = 0;
    while (1) {
        printf("XOSC awake %d\n", i++);
    }

    return 0;
}
#endif

/**
 * Main control loop.
 */
static void waterCtrlInit(void)
{
    static char qstr[20];

    sprintf (qstr, "Q%.2f", pdata.sensFq);

    if (initDisplay() != 0)
        return;

    gpioInit();

    HdrTxtColor = HDR_OK;

    DEV_SET_PWM(DEF_PWM);

    // Splash screen
    Paint_DrawImage(water_crane_map,0,0,240,135);
    Paint_DrawString_EN(2, 118, VERSION , &Font16, WHITE, BLACK);
    Paint_DrawString_EN(60, 118, qstr , &Font16, WHITE, BLACK);
    LCD_1IN14_Display(BlackImage);
  
    printf("Start core1\n");
    multicore_launch_core1(core1Thread);

    // Wait for it to start up
    uint32_t g = multicore_fifo_pop_blocking();

    if (g != FLAG_VALUE) {
        HdrTxtColor = HDR_ERROR;
        printLog("HALL sensing FAILED");
        while(1) sleep_ms(2000);
    } else {
        multicore_fifo_push_blocking(FLAG_VALUE);
        sleep_ms(2000);
    }

#if 0
    while(1) {
        sleep_ms(250);
        printLog("FlowFreq=%d", FlowFreq);
    }
#endif

    Paint_Clear(WHITE);


#if 0
    for (int i = 0; i < MAX_LINES; i++)
    {
        printLog("line = %d", i);   // Scroll test
    }
    sleep_ms(6000);
#endif

}

/**
 * This is the "main" entry
 */

void water_ctrl(void)
{
    time_t curtime;
    static struct tm *info_t;
    static char buffer_t[40];
    float volTick = 0.0;
    int tmo;

    stdio_init_all();

    memset(&pdata, 0, sizeof(persistent_data));

    read_flash(&pdata);
    printf("Current persitent data:\n  ssid  = %s\n  pass = %s\n  country = %d\n  ntp server = %s\n  totVolume = %.2f\n  FQ = %.2f  version = %.2f\n",
        pdata.ssid, pdata.pass, pdata.country, pdata.ntp_server, pdata.totVolume, pdata.sensFq, pdata.version);

    if (pdata.sensFq == 0.0) {
        pdata.sensFq = SENS_FQC;
        printf("Using default value for FQ: %.2f\n", SENS_FQC);
    }

    volTick = (1.0/60)/pdata.sensFq;


#if 0
    if (wifi_connect(pdata.ssid, pdata.pass, pdata.country)) {
        printf("WiFi connection failed\n");
    } else {
        wifi_ntp(pdata.ntp_server);
    }
#endif
   

    printf("Update flash data\n");
    strcpy(pdata.ssid, "sy-madonna-24");
    strcpy(pdata.pass, "XXXXXXXX");
    strcpy(pdata.ntp_server, "sy-madonna.madonna.lan");
    pdata.country = CYW43_COUNTRY_SWEDEN;
    pdata.version = atof(VERSION);
    pdata.filterLifetime = 1659942989 + 31536000;   // Fake for now
    pdata.tankVolume = 725.0;   // Fake for now
    write_flash(&pdata);

    info_t = localtime( &pdata.filterLifetime);
    strftime(buffer_t, sizeof(buffer_t), "%m/%d/%y", info_t);

/*
    sleep_ms(2000);

    for (int i = 0; i < 1; i++) {
    sleep_ms(10000);
    curtime = time(NULL) + TZ_CET;
    printf("Current time = %s", ctime(&curtime));

    }  

    wifi_disconnect();
*/

    waterCtrlInit();  

    tmo = 3;
    float sessLitre = 0.0;
    float litreMinute; 
   
    while (1) {

        DEV_SET_PWM(DEF_PWM);

        while(1) {

            if (FlowFreq > 0) {          
                static int lastHz;
                litreMinute = (FlowFreq * 1 / pdata.sensFq);
                sessLitre = (sessTick * volTick);

                //if (lastHz != FlowFreq) {
                    clearLog(HDR_OK);
                    printHdr("Flowing");
                    printf("FlowFreq=%dHz\n", FlowFreq);
                    printLog("FLOW=%.2f L/M", litreMinute);
                    printLog("USED=%.2f L", sessLitre + pdata.totVolume);
                //}
                lastHz = FlowFreq;
            } else if (tmo-- <= 0) {
                clearLog(HDR_ERROR);
                printHdr("No FLow");
                printLog("USE=%.2fL", pdata.totVolume + sessLitre);
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
            if (write_flash(&pdata)) {
                printLog("S-ERROR");
                sleep_ms(1000);
            }
            sessLitre = sessTick = 0;
        }
       
        //goDormant();
        while (FlowFreq == 0) {

            sleep_ms(1000);
#if 0
            DEV_SET_PWM(DEF_PWM);
            printLog("BUTT=%d", gpio_get(TestButt));
            sleep_ms(2000);
#else
            if (!gpio_get(TestButt)) {
                clearLog(HDR_ERROR);
                printHdr("Stats");
                DEV_SET_PWM(DEF_PWM);
                printLog("USE=%.2fL", pdata.totVolume);
                printLog("REM=%.0fL", pdata.tankVolume - pdata.totVolume);
                printLog("FXD=%s", buffer_t);
                printLog("FQ = %.2f", pdata.sensFq);
                sleep_ms(10000);
                continue;
            }

            if (!gpio_get(JsUp)) {
                clearLog(HDR_ERROR);
                printHdr("FQ INC");
                pdata.sensFq += 0.50;
                DEV_SET_PWM(DEF_PWM);
                printLog("Adj FQ + 0.50");
                printLog("New FQ = %.2f", pdata.sensFq);
                sleep_ms(2000);
                continue;
            }

            if (!gpio_get(JsDown) && pdata.sensFq > 0.50) {
                clearLog(HDR_ERROR);
                printHdr("FQ DEC");
                pdata.sensFq -= 0.50;
                DEV_SET_PWM(DEF_PWM);
                printLog("Adj FQ - 0.50");
                printLog("New FQ = %.2f", pdata.sensFq);
                sleep_ms(2000);
                continue;
            }

            if (!gpio_get(ResetButt)) {
                clearLog(HDR_ERROR);
                printHdr("Reset");
                DEV_SET_PWM(DEF_PWM);
                pdata.totVolume = 0;
                if (write_flash(&pdata)) {
                    printLog("R-ERROR");
                } else {
                    printLog("Reset OK");
                }
               

                if (FirmwareMode == true) {
                    printLog("Go Firmware");
                    sleep_ms(1000);
                    reset_usb_boot(0,0);
                }

                sleep_ms(1000);

                // System reset
                watchdog_enable(1, 1);
                while(1);

            }

            DEV_SET_PWM(0);
#endif
        }

        printf("Woken up!!\n");
    }

}


