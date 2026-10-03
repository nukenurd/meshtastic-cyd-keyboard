# Meshtastic CYD Keyboard v2.1 + Channel Swap

### What this does
Turns a £10 ESP32-2432S028R CYD into a touch keyboard terminal for a Heltec V3 running stock Meshtastic.

### v2.1 New:
- Splash screen boot animation
- 4-point touch calibration saved to flash (NVS) - auto on first boot, hold top-left 2s to recalibrate
- Improved keyboard layout - rounded keys, centered, larger hit areas
- Status bar with tappable CH selector
- Channel swap: tap blue CH box OR type /ch0-7
- 8 channel names: LongFast, ShortFast, LongSlow, VeryLong, MedSlow, ShortTurbo, LongTurbo, Private
- Message log with color coding

### Wiring
Heltec V3 GPIO6 TX -> CYD GPIO22 RX
Heltec V3 GPIO7 RX -> CYD GPIO27 TX
GND->GND 5V->5V

### Heltec Setup
Flash normal Meshtastic then:
```
meshtastic --set serial.enabled true
meshtastic --set serial.mode 2
meshtastic --set serial.txd 6
meshtastic --set serial.rxd 7
meshtastic --set serial.baud 115200
```

### How to get compiled .bin (GitHub Action)
1. Create new GitHub repo
2. Upload: src/main.cpp, platformio.ini, manifest.json, flasher.html, .github/workflows/build.yml
3. Actions tab -> Build -> Run workflow
4. Download artifact `cyd-keyboard-firmware` -> contains `cyd-keyboard-v2.1-factory.bin`
5. Flash via: flasher.html (open in Chrome) or esptool

### Manual flash
```
esptool.py --chip esp32 --port COMx --baud 921600 write_flash 0x0 cyd-keyboard-v2.1-factory.bin
```

### Channel Swap Logic
Current TEXTMSG mode sends to primary channel. Channel name shown is for your organization - if you want true multi-channel TX, set Heltec to Protobuf mode and we can extend code to send MeshPacket with channel index. For now, use /ch to tag and organize.

For true channel TX, change Heltec serial.mode to PROTO and update CYD to send protobuf - ask me for v3.
