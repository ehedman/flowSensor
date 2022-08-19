#pragma once

#include "lwip/apps/httpd.h"
#include "pico/cyw43_arch.h"
#include "time.h"
#include "lwipopts.h"
#include "water-ctrl.h"

// max length of the tags defaults to be 8 chars
// LWIP_HTTPD_MAX_TAG_NAME_LEN
const char * __not_in_flash("httpd") ssi_example_tags[] = {
    "SSID",
    "PASS",
    "NTP",
    "COU",
    "TOTV",
    "TNKV",
    "FQV",
    "FAGE",
    "FVOL",
    "VERS"
};

u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen) {
    extern persistent_data pdata;
    static char buffer_t[60];
    size_t printed;
    switch (iIndex) {
        case 0: /* "SSID" */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ssid);
        break;
        case 1: /* "PASS" */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.pass);
        break;
        case 2: /* NTP */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ntp_server);
        break;
        case 3: /* Country code */
            printed = snprintf(pcInsert, iInsertLen, "%d", pdata.country);
        break;
        case 4: /* Total volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.totVolume);
        break;
        case 5: /* Tank volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.tankVolume);
        break;
        case 6: /* Q/F value */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.sensFq);
        break;
        case 7: /* Filter age */
            printed = snprintf(pcInsert, iInsertLen, "%s", ctime_r(&pdata.filterAge, buffer_t));
        break;
        case 8: /* Filter volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.filterVolume);
        break;
        case 9: /* Program version */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.version);
        break;
        default: /* unknown tag */
        printed = 0;
        break;
    }
    LWIP_ASSERT("sane length", printed <= 0xFFFF);
    return (u16_t)printed;
}

void ssi_init() {
  //adc_init();
  //adc_gpio_init(26);
  //adc_select_input(0);
  size_t i;
  for (i = 0; i < LWIP_ARRAYSIZE(ssi_example_tags); i++) {
    LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
      strlen(ssi_example_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
  }

  http_set_ssi_handler(ssi_handler,
    ssi_example_tags, LWIP_ARRAYSIZE(ssi_example_tags)
    );
}
