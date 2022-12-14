#pragma once
#include <stdio.h>
#include <pico/stdlib.h>
#include "water-ctrl.h"

#ifdef HAS_NET
#include <time.h>
#include <string.h>
#include "lwipopts.h"
#include <pico/cyw43_arch.h>
#include <lwip/apps/httpd.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>

/**
 * Plug in this when not in AP mode
 */
#define TRNS_SCRIPT "<script src=\"https://translate.google.com/translate_a/element.js?cb=googleTranslateElementInit\"></script>"

/**
 * Max length of the tags defaults to be 8 chars: LWIP_HTTPD_MAX_TAG_NAME_LEN
 * Let the CPP take care of the enumeration and its strings.
 */
#define FOREACH_TAG(TAG) \
    TAG(SSID)  \
    TAG(PASS)  \
    TAG(NTP)   \
    TAG(COU)   \
    TAG(TOTV)  \
    TAG(GTOTV) \
    TAG(TNKV)  \
    TAG(FQV)   \
    TAG(FAGE)  \
    TAG(FVOL)  \
    TAG(FLRT)  \
    TAG(TDS)   \
    TAG(TEMP)  \
    TAG(KVAL)  \
    TAG(KVUP)  \
    TAG(KVDN)  \
    TAG(QVUP)  \
    TAG(QVDN)  \
    TAG(VERS)  \
    TAG(IPAD)  \
    TAG(IPAGW) \
    TAG(CURTM) \
    TAG(EPOCH) \
    TAG(UPTM)  \
    TAG(HNAME) \
    TAG(WSCAN) \
    TAG(NDBG)  \
    TAG(TRNS)  \
    TAG(APM)   \
    TAG(PFM)   \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum TAG_ENUM {
    FOREACH_TAG(GENERATE_ENUM)
};

#endif /* HAS_NET */
