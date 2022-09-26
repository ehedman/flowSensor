#ifndef _WATERCTRL_H_
#define _WATERCTRL_H_

#if __has_include("digiflow.h")
 #include "digiflow.h"
#else
 #error Run cmake to create digiflow.h
#endif

/** Eliminates compiler warning about unused arguments (GCC -Wextra -Wunused). */
#ifndef APP_UNUSED_ARG
#define APP_UNUSED_ARG(x) (void)x
#endif

/**
 * Check if we are building for Pico w, i.e, build invoked as 'PICO_BOARD=pico_w cmake  ..'
 */
#if defined _BOARDS_PICO_W_H
#define HAS_NET     // Enable support for for pico_w network else pico legacy with or without LCD hat for testing only.
#endif

/*
 * The RTC hat is attached
 */
//#define HAS_RTC

/**
 * For debugging
 */
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * For debug purposes this app enters flash
 * mode when the reset button is pressed.
 */
#define FLASHMODE true

/**
 * Add some debug output
 */
#define NET_DEBUG

/**
 * For net status
 */
#define nON     true
#define nOFF    false

/**
 * String lenght limits
 */
#define SSID_MAX    32
#define RSSI_MAX    10
#define SSID_LIST   10
#define PASS_MAX    64
#define URL_MAX     70

/**
 * Shared data types
 */
typedef struct wifi_data {
    char    ssid[SSID_MAX+2];
    char    rssi[RSSI_MAX];
} w_data;

typedef struct s_data {
    time_t  startTime;
    time_t inactivityTimer;
    int     outOfPcb;
    int     lostPing;
    int     versioMajor;
    int     versionMinor;
    int     ssidCount;
    float   totVolume;
    char    versionString[20];
    w_data  wfd[SSID_LIST];
} shared_data;

/**
 * Validation tag for the p_data struct in flash
 */
#define IDT         0x3FC8727A

/**
 * Main control data type to be saved in flash
 */
typedef struct p_data {
    uint8_t     checksum;
    char        ssid[SSID_MAX+2];
    char        pass[PASS_MAX+2];
    char        ntp_server[URL_MAX+2];
    uint32_t    country;
    float       totVolume;
    float       gtotVolume;
    float       tankVolume;
    float       filterVolume;
    float       sensFq;
    time_t      filterAge;
    float       version;
    time_t      rebootTime;
    int         rebootCount;
    bool        apMode;
    uint32_t    idt;
} persistent_data;

/**
 * Common functions used in this app
 */
extern bool     read_flash(persistent_data *pdata);
extern bool     write_flash(persistent_data *new_data);
extern void     goDormant(uint8_t dpin, persistent_data *pdata, shared_data *sdata, int lostPing);
extern time_t   _time(time_t *tloc);

#ifdef HAS_NET

extern bool     netNTP_connect(persistent_data *pdata);
extern bool     wifi_connect(persistent_data *pdata);
extern bool     net_checkconnection(void);
extern void     net_setconnection(bool mode);
extern void     ping_send_now(void);
extern bool     ping_status(void);
extern void     wifi_scan(int scanTurns, shared_data *sdata);
extern bool     wifi_find(char *ap, shared_data *sdata);
extern void     init_httpd(bool doIt);

#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1

#define WIFI_COUNTRY    CYW43_COUNTRY_SWEDEN
#define WIFI_SSID       "myAp"
#define WIFI_PASS       "PASSWORD"
#define NTP_SERVER      "time.google.com"       // 0.0.0.0 = auto i.e assume DHCP host also is NTP server
#define APMODE_PASSWORD "digiflow"      // Static password for AP mode

/**
 * OK chars for URLs and WiFi text properties
 */
#define OKCHAR   "abcdefghijklmnopqrstuvwxzyABCDEFGHIJKLMNOPQRTSUVWXZY0123456789-"

/**
 * For debugging: Monitor out of PCB events etc
 */
#define MAX_BAD_PCBS    4
#define MAX_BAD_PINGS   20
#define NET_SANITY_CHECK     // Undef this if tcp_in-c.patch is not applied

#endif /* HAS_NET */

#define TANK_VOLUME     725.00                      // Water tank volume

/**
 * Sensor characteristics:
 * SENS_FQC from sensors datasheet (F) or better
 * messured in actual installation environment.
 */
#define SENS_FQC    11.00           // Pulse Characteristics: F=11.00 x Q (Rate of flow (Q) in litres/min)

/**
 * Waveshare RTC properties:
 * The first verison use i2c1(GP6,GP7)
 * The new vesion use i2c0(GP20,GP21)
 */
#define I2C_PORT	    i2c0
#define I2C_SCL		    20	
#define I2C_SDA		    21
#define DS3231_ADDR     0x68

/**
 * Epoch related constants
 */
#define SYEAR           (time_t)31556926
#define SMONTH          (time_t)(SWEEK*4)
#define S6MONTH         (time_t)(SYEAR/2)
#define SWEEK           (time_t)604800
#define SDAY            (time_t)86400
#define SHOUR           (time_t)3600
#ifdef HAS_RTC
#define TZ_RZONE        (0*SHOUR)           // Add relevant zone hours to RTC UTC
#else
#define TZ_RZONE        (2*SHOUR) 
#endif
#define FLTR_LIFETIME   (6*SMONTH)
#define INACTIVITY_TIME (SHOUR*4)

/**
 * Private wrapper for time()
 */
#define time(a)         _time(a)            // Use our time() to get current epoch from RTC

#endif


