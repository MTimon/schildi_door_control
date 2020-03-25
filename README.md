# schildi_door_control
## Functions
- [x] manually open and close door via BLYNK(R) - App 
- [x] get internet time via NTP-service
- [x] automatically open and close door by calculating heigth of sun over / below horizon
- [x] OTA OverTheAir - update by Arduino IDE --> only in "WS_Schildi_OTA.ino"
- [x] initial open/close after reboot according last BLYNK-command
## How to use
- download Arduino IDE
- install libraries: BLYNK, OTA, NTPClient, ESP8266
- use WS_Schildi.ino without OTA-update
- or use WS_Schildi_OTA.ino with OTA-update
- in both cases create or download the "WS_config.h" into the same directory as .ino-file
- make your custom configurations concerning SSID and PASSWORD in "WS_config.h"
- install BLYNK onto your mobile via PlayStore or AppStore
- register on BLYNK
- scan QR-Code
- generate your own blynk-token
- put in the token into "WS_config.h"
- upload sthe sketch
## OTA
After first upload of the WS_Schildi_OTA.ino - project, you should be able to upload further software by use of over-the-air-process. Useful information: https://arduino-esp8266.readthedocs.io/en/latest/ota_updates/readme.html
