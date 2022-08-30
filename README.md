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
- WiFi connectivity management
    
### Environment
- Linux Mint (Debian) platform for development
- The folder structure of this application is ment to be added to the Waveshare SDK "Pico_code/c" folder structure.

### Build
Once integrated with the Waveshare build structure:
- Alter the default WiFi credentials in ./examples/water-ctrl.h
- mkdir build; cd build; cmake -DPICO_BOARD=pico_w  ..
- make fsdata (make embedded file system to hold html index files etc.)
- make (to produce the executable main.main.uf2)

### External dependencies
- ARM Cross delevopment tools for the Pico (apt install gcc-arm-none-eabi)
- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [PIco SDK reference manual](https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf)
- [Waveshare SDK](https://www.waveshare.com/w/upload/0/06/Pico-LCD-1.14.zip)
- [LwIP Lightweight IP stack](https://www.nongnu.org/lwip/2_1_x/index.html)
- [krzmaz's blog](http://krzmaz.com/2022-08-15-creating-a-web-server-on-raspberry-pi-pico-w-using-pico-sdk-and-lwip)

### Screenshot
<img src="http://hedmanshome.se/digi242-1.png" width=100%></br>
<img src="http://hedmanshome.se/digi242-2.png" width=100%></br>

    
