#pragma once
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "water-ctrl.h"

#if defined NETRTC && NETRTC == 1

#include "lwipopts.h"
#include <lwip/apps/httpd.h>
#include <pico/cyw43_arch.h>

// max length of the tags defaults to be 8 chars
// LWIP_HTTPD_MAX_TAG_NAME_LEN
#define FOREACH_TAG(TAG) \
    TAG(SSID)  \
    TAG(PASS)  \
    TAG(NTP)   \
    TAG(COU)   \
    TAG(TOTV)  \
    TAG(TNKV)  \
    TAG(FQV)   \
    TAG(FAGE)  \
    TAG(FVOL)  \
    TAG(VERS)  \
    TAG(IPAD)  \
    TAG(IPAGW) \
    TAG(CURTM) \
    TAG(EPOCH) \
    TAG(UPTM)  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum TAG_ENUM {
    FOREACH_TAG(GENERATE_ENUM)
};

const char * __not_in_flash("httpd") ssi_html_tags[] = {
    FOREACH_TAG(GENERATE_STRING)
};

static inline char* sec_today(time_t nsec, char* buf_t)
{
    int day = nsec / (24 * 3600);
 
    nsec= nsec % (24 * 3600);
    int hour = nsec / 3600;
 
    nsec %= 3600;
    int minutes = nsec / 60 ;

    sprintf(buf_t, "%d days, %d hours, %d minutes", day, hour, minutes);

    return buf_t;
}

u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen)
{
    extern persistent_data pdata;
    extern time_t startTime;
    extern time_t inactivityTimer;
    static char buffer_t[60];
    size_t printed;
    time_t curtime;

    if (inactivityTimer < SHOUR/2) {  // Don't go dormant anythime soon ..
        inactivityTimer = SHOUR/2;
    }

    switch (iIndex) {
        case SSID:  /* "SSID" */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ssid);
        break;
        case PASS:  /* "PASS" */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.pass);
        break;
        case NTP:   /* NTP */
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ntp_server);
        break;
        case COU:   /* Country code */
            printed = snprintf(pcInsert, iInsertLen, "%d", pdata.country);
        break;
        case TOTV:  /* Total volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.totVolume);
        break;
        case TNKV:  /* Tank volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.tankVolume);
        break;
        case FQV:   /* Q/F value */
            printed = snprintf(pcInsert, iInsertLen, "%.1f", pdata.sensFq);
        break;
        case FAGE: /* Filter age */
            printed = snprintf(pcInsert, iInsertLen, "%llu", pdata.filterAge);
        break;
        case FVOL: /* Filter volume */
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.filterVolume);
        break;
        case VERS: /* Program version */
            printed = snprintf(pcInsert, iInsertLen, "%.1f", pdata.version);
        break;
        case IPAD:  /* I.P address */
            printed = snprintf(pcInsert, iInsertLen, "%s", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        break;
        case IPAGW: /* I.P address  GW*/
            printed = snprintf(pcInsert, iInsertLen, "%s",  ip4addr_ntoa(netif_ip4_gw(netif_list)));
        break;
        case CURTM: /* Today */
            curtime=time(NULL);
            printed = snprintf(pcInsert, iInsertLen, "%s", ctime_r(&curtime, buffer_t));
        break;
        case EPOCH: /* Today in epoch */
            printed = snprintf(pcInsert, iInsertLen, "%llu", time(NULL));
        break;
        case UPTM: /* Uptime */
            curtime=time(NULL);
            printed = snprintf(pcInsert, iInsertLen, "%s", sec_today(curtime-startTime, buffer_t));
        break;
        default:    /* unknown tag */
        printed = 0;
        break;
    }

    LWIP_ASSERT("sane length", printed <= 0xFFFF);
    return (u16_t)printed;
}

/**
 * Set the SSI handler function and start httpd
 */
void init_httpd()
{
    size_t i;

    for (i = 0; i < LWIP_ARRAYSIZE(ssi_html_tags); i++) {
        LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
        strlen(ssi_html_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }

    /**
    * http_set_ssi_handler Parameters:
    *    ssi_handler	the SSI handler function
    *    tags	        an array of SSI tag strings to search for in SSI-enabled files
    *    num_tags	    number of tags in the 'tags' array
    */ 
    http_set_ssi_handler(ssi_handler, ssi_html_tags, LWIP_ARRAYSIZE(ssi_html_tags));

    httpd_init();

    printf("Http server initialized.\n");

}

static inline int ishex(int x)
{
	return	(x >= '0' && x <= '9')	||
		(x >= 'a' && x <= 'f')	||
		(x >= 'A' && x <= 'F');
}
 
/**
 * URL-decode a string.
 * Parameters:
 *  s string to be decoded
 *  out the supplied buffer to hold the decoded string
 */
static int decode(const char *s, char *out)
{
	char *o;
	const char *end = s + strlen(s);
	int c;
 
	for (o = out; s <= end; o++) {
		c = *s++;
		if (c == '+') c = ' ';
		else if (c == '%' && (	!ishex(*s++)	||
					!ishex(*s++)	||
					!sscanf(s - 2, "%2x", &c)))
			return -1;
 
		if (out) *o = c;
	}
 
	return o - out;
}

#define USER_PASS_BUFSIZE 40

static void *current_connection;
static void *valid_connection;

/**
 * Called when a POST request has been received.
 * The application can decide whether to accept it or not.
 * Parameters:
 *  connection	Unique connection identifier, valid until httpd_post_end is called.
 *  uri	The HTTP header URI receiving the POST request.
 *  http_request	The raw HTTP request (the first packet, normally).
 *  http_request_len	Size of 'http_request'.
 *  content_len	Content-Length from HTTP header.
 *  response_uri	Filename of response file, to be filled when denying the request
 *  response_uri_len	Size of the 'response_uri' buffer.
 *  post_auto_wnd	Set this to 0 to let the callback code handle 
 *  window updates by calling 'httpd_post_data_recved' (to throttle rx speed)
 *  default is 1 (httpd handles window updates automatically) .
 *
 * Returns:
 *  ERR_OK: Accept the POST request, data may be passed in another err_t:
 *  Deny the POST request, send back 'bad request'. 
 */
err_t httpd_post_begin (
    void *  	    connection,
    const char *    uri,
    const char *  	http_request,
    u16_t       	http_request_len,
    int  	        content_len,
    char *  	    response_uri,
    u16_t  	        response_uri_len,
    u8_t *  	    post_auto_wnd 
	)
{
    LWIP_UNUSED_ARG(connection);
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(post_auto_wnd);
    if (!memcmp(uri, "/index.html", 12)) {
        if (current_connection != connection) {
          current_connection = connection;
          valid_connection = NULL;
          /* default page is "index" */
          snprintf(response_uri, response_uri_len, "/index.html");
          *post_auto_wnd = 1;
          return ERR_OK;
        }
    }
    return ERR_VAL;
}


/**
 * Called for each pbuf of data that has been received for a POST. 
 * ATTENTION: The application is responsible for freeing the pbufs passed in!
 * Parameters:
 *  connection	Unique connection identifier.
 *  p	Received data.
 *
 * Returns:
 *  ERR_OK: Data accepted. another err_t: Data denied, http_post_get_response_uri will be called. 
 */

err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    extern persistent_data pdata;
    err_t ret;
    u16_t token, value_token, len_token;
    static char buf_t[100];

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection) {
        for (int indx = 0; indx < LWIP_ARRAYSIZE(ssi_html_tags); indx++) {
            char *tag = (char*)ssi_html_tags[indx];

            memset(buf_t, 0, sizeof(buf_t));

            for (int j = 0; tag[j] != '\0'; j++) {
                buf_t[j] = tolower(tag[j]);
            }
            strcat(buf_t, "=");

            if ((token = pbuf_memfind(p, buf_t, strlen(buf_t), 0)) == 0xFFFF) {
                //printf("not found %s\n", buf_t);
                continue;
            }

            value_token = token + strlen(buf_t);
            len_token = 0;
            u16_t tmp;
            tmp = pbuf_memfind(p, "&", 1, value_token);

            if (tmp != 0xFFFF) {
                len_token = tmp - value_token;
            } else {
                len_token = p->tot_len - value_token;
            }

            if ((len_token > 0) && (len_token < USER_PASS_BUFSIZE)) {
                /* provide contiguous storage if p is a chained pbuf */
                char buf_token[USER_PASS_BUFSIZE];
                char *param = (char *)pbuf_get_contiguous(p, buf_token, sizeof(buf_token), len_token, value_token);
                if (param) {
                    char dec[USER_PASS_BUFSIZE];
                    param[len_token] = 0;
                    decode(param, dec); /* URL decode string */
                    //printf("(%d)param=%s\n", indx, dec);

                    switch (indx) {
                        case SSID: strcpy(pdata.ssid, dec);         break;
                        case PASS: strcpy(pdata.pass, dec);         break;
                        case NTP:  strcpy(pdata.ntp_server, dec);   break;
                        case COU:  pdata.country = atol(dec);       break;
                        case TOTV: pdata.totVolume = atof(dec);     break;
                        case TNKV: pdata.tankVolume = atof(dec);    break;
                        case FQV:  pdata.sensFq = atof(dec);        break;
                        case FAGE: pdata.filterAge = atol(dec);     break;
                        case FVOL: pdata.filterVolume = atof(dec);  break;
                        default: /* unknown/ignored tag */          break;
                    }      
                }
            }      
        }

        if (pdata.filterAge < time(NULL)) {
            pdata.filterAge = time(NULL) + SDAY;
        }

        if (pdata.totVolume > pdata.tankVolume) {
            pdata.totVolume = pdata.tankVolume;
        }

#if 0
char buffer_t[60];
printf("\n\n  ssid  = %s\n  pass = %s\n  country = %d\n  ntp server = %s\n  \
totVolume = %.2f\n  tankVolume = %.2f\n  FQ = %.2f\n  filterAge to = %s  filterVolume = %.0f\n\n",
pdata.ssid, pdata.pass, pdata.country, pdata.ntp_server, pdata.totVolume, pdata.tankVolume,
pdata.sensFq, ctime_r(&pdata.filterAge, buffer_t)), pdata.filterVolume ;
#endif

        write_flash(&pdata);

        ret = ERR_OK;
    } else {
        ret = ERR_VAL;
    }

    // This function must ALWAYS free the pbuf it is passed or it will leak memory
    pbuf_free(p);

    return ret;
}

/**
 * Called when all data is received or when the connection is closed.
 * The application must return the filename/URI of a file to send in response
 * to this POST request. If the response_uri buffer is untouched, a 404 response is returned.
 *
 * Parameters:
 *  connection	Unique connection identifier.
 *  response_uri	Filename of response file, to be filled when denying the request
 *  response_uri_len	Size of the 'response_uri' buffer.
 */
void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    // default page is "index"
    snprintf(response_uri, response_uri_len, "/index.html");

    if (current_connection == connection) {
        if (valid_connection == connection) {
            // page succeeded
            snprintf(response_uri, response_uri_len, "/index.html");
        }
        current_connection = NULL;
        valid_connection = NULL;
    }
}
#endif /* NETRTC */
