/*****************************************************************************
* | File      	:   ssi.c
* | Author      :   erland@hedmanshome.se
* | Function    :   Construct the web page and handle form posts
* | Info        :   
* | Depends     :   Rasperry Pi Pico W and LwIP
*----------------
* |	This version:   V1.2
* | Date        :   2022-10-10
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
#include "ssi.h"

#ifdef HAS_NET

/**
 * Let the CPP build the tag array
 */
const char * __not_in_flash("httpd") ssi_html_tags[] = {
    FOREACH_TAG(GENERATE_STRING)
};

/**
 * Helper for the uptime tag.
 */
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

/**
 * The httpd callback is called for each evaluated tag in the ssi.shtml file.
 */
u16_t __time_critical_func(ssi_handler)(int iIndex, char *pcInsert, int iInsertLen)
{
    extern persistent_data pdata;
    extern shared_data sdata;
    static char buffer_t[60];
    static char wscans[sizeof(sdata.wfd)+SSID_LIST+2];
    int printed;
    static time_t curtime;
#ifdef NET_DEBUG
    static int httpdReq;
#endif

    if (iIndex == CURTM) { // CURTM expected to occur only one time and early in ssi.shtml
        if (sdata.inactivityTimer < SHOUR/2) {  // Don't go dormant anythime soon ..
            sdata.inactivityTimer = SHOUR/2;
        }
        curtime=time(NULL);
        memset(wscans,0, sizeof(wscans));
        for (int i=0; i < sdata.ssidCount; i++) {
            strncat(wscans, sdata.wfd[i].ssid, SSID_MAX);
            strcat(wscans, "|");
            strncat(wscans, sdata.wfd[i].rssi, RSSI_MAX);
            if (i < sdata.ssidCount-1) {
                strcat(wscans, "|");
            }
        }
#ifdef NET_DEBUG      
        printf("Httpd request no. %d\n", ++httpdReq);
#endif
    }

    switch (iIndex) {
        case SSID:  // SSID
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ssid);
        break;
        case PASS:  // PASS
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.pass);
        break;
        case NTP:   // NTP
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.ntp_server);
        break;
        case COU:   // Country code
            printed = snprintf(pcInsert, iInsertLen, "%lu", pdata.country);
        break;
        case TOTV:  // Total consumed volume
            printed = snprintf(pcInsert, iInsertLen, "%.1f", sdata.totVolume);
        break;
        case GTOTV: // Grand Total volume consumed
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.gtotVolume);
        break;
        case TNKV:  // Tank volume
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.tankVolume);
        break;
        case FQV:   // Q/F value
            printed = snprintf(pcInsert, iInsertLen, "%.1f", pdata.sensFq);
        break;
        case QVUP:  // Q/F value calibration Up
            if (pdata.sensFq +0.2 < 14.2) {pdata.sensFq += 0.2;}
            printed = snprintf(pcInsert, iInsertLen, "%.2f", pdata.sensFq);
        break;
        case QVDN:  // Q/F value calibration Down
            if (pdata.sensFq -0.2 > 4.2) {pdata.sensFq -= 0.2;}
            printed = snprintf(pcInsert, iInsertLen, "%.2f", pdata.sensFq);
        break;
        case FAGE:  // Filter age
            printed = snprintf(pcInsert, iInsertLen, "%llu", pdata.filterAge);
        break;
        case FVOL:  // Filter volume
            printed = snprintf(pcInsert, iInsertLen, "%.0f", pdata.filterVolume);
        break;
        case FLRT:  // Flow rate
            printed = snprintf(pcInsert, iInsertLen, "%.1f", sdata.flowRate);
        break;
        case TDS:  // TDS value
            printed = snprintf(pcInsert, iInsertLen, "%d", sdata.tdsValue);
        break;
        case TEMP:  // Water temp
            printed = snprintf(pcInsert, iInsertLen, "%3.1f", sdata.waterTemp);
        break;
        case KVAL:  // K value calibration
            printed = snprintf(pcInsert, iInsertLen, "%.2f", pdata.kValue);
        break;
        case KVUP:  // K value calibration Up
            if (pdata.kValue +0.05 < 2.1) {pdata.kValue += 0.05;}
            printed = snprintf(pcInsert, iInsertLen, "%.2f", pdata.kValue);
        break;
        case KVDN:  // K value calibration Down
            if (pdata.kValue -0.05 > 0.5) {pdata.kValue -= 0.05;}
            printed = snprintf(pcInsert, iInsertLen, "%.2f", pdata.kValue);
        break;
        case VERS:  // Program version
            printed = snprintf(pcInsert, iInsertLen, "%.1f", pdata.version);
        break;
        case IPAD:  // I.P address
            printed = snprintf(pcInsert, iInsertLen, "%s", ip4addr_ntoa(netif_ip4_addr(netif_list)));
        break;
        case IPAGW: // I.P address default gateway
            printed = snprintf(pcInsert, iInsertLen, "%s",  ip4addr_ntoa(netif_ip4_gw(netif_list)));
        break;
        case CURTM: // Today
            printed = snprintf(pcInsert, iInsertLen, "%s", ctime_r(&curtime, buffer_t));
        break;
        case EPOCH: // Today in epoch
            printed = snprintf(pcInsert, iInsertLen, "%llu", curtime);
        break;
        case UPTM:  // Uptime
            printed = snprintf(pcInsert, iInsertLen, "%s", sec_today(curtime-sdata.startTime, buffer_t));
        break;
        case HNAME: // Our official name
            printed = snprintf(pcInsert, iInsertLen, "%s", CYW43_HOST_NAME);
        break;
        case WSCAN: // Scanned WiFi hosts
            printed = snprintf(pcInsert, iInsertLen, "%s", wscans);
        break;
        case TRNS: // Do translation ?
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.apMode == true? "" : TRNS_SCRIPT);
        break;
        case APM: // AP mode ?
            printed = snprintf(pcInsert, iInsertLen, "%s", pdata.apMode == true? "1" : "0");
        break;
        case NDBG: // Some debug data
#ifdef NET_DEBUG
            printed = snprintf(pcInsert, iInsertLen, "%d,%d,%d,%d,%llu", sdata.lostPing, sdata.outOfPcb, pdata.rebootCount, httpdReq, pdata.rebootTime);
#else
            printed = snprintf(pcInsert, iInsertLen, "false");
#endif
        break;
        default:    // unknown tag
        printed = 0;
        break;
    }

    LWIP_ASSERT("sane length", printed <= 0xFFFF);
    return (u16_t)printed;
}

/**
 * Set the SSI handler function and start httpd
 */
void init_httpd(bool doIt)
{
    size_t i;

    if (doIt == false) return;

    for (i = 0; i < LWIP_ARRAYSIZE(ssi_html_tags); i++) {
        LWIP_ASSERT("tag too long for LWIP_HTTPD_MAX_TAG_NAME_LEN",
        strlen(ssi_html_tags[i]) <= LWIP_HTTPD_MAX_TAG_NAME_LEN);
    }

    /**
     * http_set_ssi_handler Parameters:apMode
     *    ssi_handler	the SSI handler function
     *    tags	        an array of SSI tag strings to search for in SSI-enabled files
     *    num_tags	    number of tags in the 'tags' array
     */ 
    http_set_ssi_handler(ssi_handler, ssi_html_tags, LWIP_ARRAYSIZE(ssi_html_tags));

    /**
     * Initialize the httpd: set up a listening PCB and bind it to the defined port
     */
    httpd_init();

    printf("Http server initialized.\n");

}

/**
 * Helper for decode().
 */
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
	unsigned int c;
 
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
          // default page is "index"
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
 *  connection	Unique connection identifier.apmDiv
 *  p	Received data.
 *
 * Returns:
 *  ERR_OK: Data accepted. another err_t: Data denied, http_post_get_response_uri will be called. 
 */
err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    extern persistent_data pdata;
    extern shared_data sdata;
    err_t ret;
    u16_t token, value_token, len_token;
    int apMode = 0;
    static char buf_t[100];

    LWIP_ASSERT("NULL pbuf", p != NULL);

    if (current_connection == connection) {
        for (int indx = 0; indx < (int)LWIP_ARRAYSIZE(ssi_html_tags); indx++) {
            char *tag = (char*)ssi_html_tags[indx];

            memset(buf_t, 0, sizeof(buf_t));

            for (int j = 0; tag[j] != '\0'; j++) {
                buf_t[j] = tolower(tag[j]);
            }
            strcat(buf_t, "=");

            if ((token = pbuf_memfind(p, buf_t, (u16_t)strlen(buf_t), 0)) == 0xFFFF) {
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
                // provide contiguous storage if p is a chained pbuf
                char buf_token[USER_PASS_BUFSIZE];
                char *param = (char *)pbuf_get_contiguous(p, buf_token, sizeof(buf_token), len_token, value_token);
                if (param) {
                    char dec[USER_PASS_BUFSIZE];
                    param[len_token] = 0;
                    decode(param, dec); // URL decode string
                    //printf("(%d)param=%s\n", indx, dec);

                    switch (indx) {
                        case SSID:  strcpy(pdata.ssid, dec);                 break;
                        case PASS:  strcpy(pdata.pass, dec);                 break;
                        case NTP:   strcpy(pdata.ntp_server, dec);           break;
                        case COU:   pdata.country = (float)atol(dec);        break;
                        case TOTV:  pdata.totVolume = (float)atof(dec);      break;
                        case GTOTV: pdata.gtotVolume = (float)atof(dec);     break;
                        case TNKV:  pdata.tankVolume = (float)atof(dec);     break;
                        case FQV:   pdata.sensFq = (float)atof(dec);         break;
                        case FAGE:  pdata.filterAge = (float)atol(dec);      break;
                        case FVOL:  pdata.filterVolume = (float)atof(dec);   break;
                        case KVAL:  pdata.kValue = (float)atof(dec);         break;
                        case APM:   apMode = atoi(dec);                      break;
                        default:    /* unknown/ignored tag */                break;
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

        sdata.totVolume = pdata.totVolume;

        if (apMode == 2 && pdata.apMode == true) {
            pdata.apMode = false;
            write_flash(&pdata);
            watchdog_enable(1, 1);  // Reboot to STA mode
            while(1);
            /** NOT REACHED **/
        }

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
#endif
