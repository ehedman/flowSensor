# Raspberry Pi Pico W flowSensor and Meter
Marine water flow sensor and meter

### Hardware used
- Raspberry Pi Pico W
- Waveshare Precision RTC module
- Waveshare 1.14" color display
- A water flow sensor

### Features
- Monitor filter elapse time
- Monitor total flow through filter
- Monitor volume / time
- Near end of filter life alert
- End of filter life alert
- Automatic data memorised in flash
- WiFi connectivity, NTP and HTTPD functions
- WiFi connectivity management for AP and STA modes
    
### Environment
- Linux Mint (Debian/Ubuntu) platform for development
- The folder structure of this application follows the Waveshare SDK "Pico_code/c" structure. The free Waveshare libs are included here.

### Build
- Install Pico SDK (if not installed already)
- Install pico_extras (if not installed previousy) using the script preparePico-extras.sh
- Alter the default WiFi credentials in ./examples/water-ctrl.h
- mkdir build; cd build; PICO_BOARD=pico_w cmake  ..
- make fsdata (make embedded file system to hold html index files etc. Run each time html files has been changed)
- make (to produce the executable main.uf2)
- make install will install the main.uf2 into /media/(your logname)/RPI-RP2 assuming here that this folder is automounted when the Pico is set to "BOOTSEL" mode.
- The hostname of this Pico W is set to DigiFlow, so it should be possible to surf in to the device referenced that name (+plus your domain name).

### Test
It is possible to test this app on a bare Pico W without the extra hats and just browse in to this device home page. In such case, disable the RTC define in the water-ctrl.h to get sane date timestamps.

The first time this app is installed on a fresh Pico W, the pico will enter AP (Access Point) mode and from there it can be configured as a ordinary (STA) wifi node. The passord for AP mode is "digiflow" and the GW I.P is 192.168.4.1 to point your browsser to.

It is also possibe to switch between these modes by using the display (GPIO) buttons. The AP mode is considered a "rescue" mode if the pico is somehow lost in the wifi universe.

It is possible to simulate a flow sensor by applying a 2.5 Volt square wave to GPIO(5) at a frequency between 1 to 110 Hz.

To folow the console output, attach a seral to USB dongle to the Picos' No 1 (output)  pin.

### External dependencies
- ARM Cross delevopment tools for the Pico (apt install gcc-arm-none-eabi)
- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Pico SDK reference manual](https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf)
- [Pico Extras](https://github.com/raspberrypi/pico-extras)
- [LwIP Lightweight IP stack](https://www.nongnu.org/lwip/2_1_x/index.html)
- [Waveshare SDK](https://www.waveshare.com/w/upload/0/06/Pico-LCD-1.14.zip)

### Screenshot
[Check THIS out](https://www.hedmanshome.se/digiflow.html)

<img src="https://hedmanshome.se/digi242-10.png" width=100%></br>
<img src="https://hedmanshome.se/digi242-21.png" width=100%></br>

    
