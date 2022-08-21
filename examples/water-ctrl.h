#ifndef _WATERCTRL_H_
#define _WATERCTRL_H_

/**
 * For debugging
 */
#define NETRTC      1       // 0 = Disable suport for network and RTC hat i.e, legacy pico mode.
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * Version string in splash screen
 */
#define VERSION "1.1"

/**
 * String lenght limits
 */
#define SSID_MAX    40
#define PASS_MAX    63
#define URL_MAX     70

/**
 * Main control data type
 */
typedef struct p_data {
    uint8_t     checksum;
    char        ssid[SSID_MAX];
    char        pass[PASS_MAX];
    char        ntp_server[URL_MAX];
    uint32_t    country;
    float       totVolume;
    float       tankVolume;
    float       sensFq;
    time_t      filterAge;
    float       filterVolume;
    float       version;
} persistent_data;

/**
 * Common functions used in this app
 */
extern bool     netNTP_connect(persistent_data *pdata);
extern void     net_disconnect(void);
extern bool     wifi_connect(char *ssid, char *pass, uint32_t country);
extern bool     net_checkconnection(void);
extern void     read_flash(persistent_data *pdata);
extern bool     write_flash(persistent_data *new_data);
extern void     goDormant(int dpin);
extern time_t   _time();

#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1

#define WIFI_COUNTRY    CYW43_COUNTRY_SWEDEN
#define WIFI_SSID       "sy-madonna-24"
#define WIFI_PASS       "090a0b0c0d"
#define NTP_SERVER      "0.0.0.0"                   // 0.0.0.0 = auto i.e assume DHCP host also is NTP server

#define TANK_VOLUME     725.00                      // Water tank volume

/**
 * Sensor characteristics:
 * SENS_FQC from sensors datasheet (F) or better
 * messured in actual installation environment.
 */
#define SENS_FQC    11.00           // Pulse Characteristics: F=11.00 x Q (Rate of flow (Q) in litres/min)

/**
 * OK chars for URLs and WiFi text properties
 */
#define OKCHAR   "abcdefghijklmnopqrstvwxzyABCDEFGHIJKLMNOPQRTSVWXZY0123456789-"

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
#define TZ_RZONE        (0*SHOUR)           // Add relevant zone hours to RTC UTC
#define FLTR_LIFETIME   (6*SMONTH)
#define INACTIVITY_TIME (SHOUR*4)

/**
 * Private wrapper for time()
 */
#define time(a)         _time(a)            // Use our time() to get current epoch from RTC

#endif


