#ifndef _WATERCTRL_H_
#define _WATERCTRL_H_

/**
 * Version string in splash screen
 */
#define VERSION "1.1"

#define SSID_MAX    40
#define PASS_MAX    63
#define URL_MAX     70

typedef struct p_data {
    uint8_t checksum;
    char ssid[SSID_MAX];
    char pass[PASS_MAX];
    char ntp_server[URL_MAX];
    uint32_t country;
    float totVolume;
    float tankVolume;
    float sensFq;
    time_t filterLifetime;
    float version;
} persistent_data;

extern void printLog(const char *format , ...);


extern int wifi_connect(char *ssid, char *pass, uint32_t country);
extern void wifi_disconnect(void);
extern void wifi_ntp(char *server);
extern void read_flash(persistent_data *pdata);
extern int write_flash(persistent_data *new_data);
extern time_t _time();

#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1

#define WIFI_COUNTRY    CYW43_COUNTRY_SWEDEN
#define WIFI_SSID       "sy-madonna-24"
#define WIFI_PASS       "090a0b0c0d"
#define NTP_SERVER      "192.168.4.3"
#define TZ_CET          0   /* 3600*2      // Add two hours to RTC UTC */
#define time(a)         _time(a)    // Use our time() to get current epoch from RTC

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define OKCHAR   "abcdefghijklmnopqrstvwxzyABCDEFGHIJKLMNOPQRTSVWXZY0123456789-"

#define ds3231_addr 0x68

/*the first verison use i2c1(GP6,GP7)*/
/*the new vesion use i2c0(GP20,GP21)*/

#define I2C_PORT	i2c0
#define I2C_SCL		20	
#define I2C_SDA		21

#endif


