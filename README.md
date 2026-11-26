# zigbee-pcie-switch

![compile](https://github.com/Yuke-hd/zigbee-pcie-switch/actions/workflows/compile.yaml/badge.svg)


# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |

## Hardware

| Front | Back |
| --- | --- |
| <img src="img/front.png" alt="Front of the Zigbee PCIe switch board" width="420"> | <img src="img/back.png" alt="Back of the Zigbee PCIe switch board" width="420"> |


### Demo
[![demo](https://img.youtube.com/vi/ddB3xJL942k/0.jpg)](https://www.youtube.com/shorts/ddB3xJL942k?feature=share)


### Flashing from a browser

Connect the switch with a USB data cable and open the [firmware installer](https://yuke-hd.github.io/zigbee-pcie-switch/) in desktop Chrome or Edge. It detects the ESP32-H2 or ESP32-C6 and installs the latest stable GitHub release. Choose **Zigbee end device**, **Zigbee router extender** or, on ESP32-C6, **Matter over Thread** when available.

- **Keep pairing** updates an existing switch using the same role and Zigbee partition layout. It preserves Zigbee pairing and device settings.
- **Wipe** installs a fresh copy of the firmware and clears device settings and pairing. Choose this for a new switch, when changing roles, or when you want to pair it again.

If the installer cannot connect, put the board in download mode by holding its BOOT button while resetting or reconnecting USB, then retry.

### Zigbee range extender

The router firmware extends an existing Zigbee mesh while keeping PC power and reset controls available. Keep the board powered continuously so other devices can route through it.

Use the browser installer, download the matching `esp32h2-router.bin` or `esp32c6-router.bin` from [GitHub Releases](https://github.com/Yuke-hd/zigbee-pcie-switch/releases), or build the router firmware in Arduino IDE. For a merged release binary, use Espressif's esptool to write the image at address `0x0`.

When changing roles, remove the switch from your coordinator, use **Wipe** in the browser installer or erase the board's flash before uploading manually, then pair it again. The roles use different flash layouts for Zigbee settings.

#### Publishing firmware

Set the repository's **Settings → Pages → Source** to **GitHub Actions**. Publish a stable GitHub release from a tag that includes the firmware workflow. CI builds both chip variants with Arduino-ESP32 3.3.5 and attaches `esp32h2.bin`, `esp32c6.bin`, `esp32h2-router.bin`, and `esp32c6-router.bin` to the release. A separate workflow deploys the browser installer and serves those binaries over HTTPS.

To redeploy the installer for the current stable release, run **Deploy firmware installer** from the Actions tab on `main`.

#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* For an end device, select `Zigbee ED (end device)` under **Zigbee mode** and `Zigbee 4MB with spiffs` under **Partition Scheme**.
* For a range extender, select `Zigbee ZCZR (coordinator/router)` under **Zigbee mode** and `Zigbee ZCZR 4MB with spiffs` under **Partition Scheme** (Arduino CLI options `ZigbeeMode=zczr,PartitionScheme=zigbee_zczr`).
* Select the COM port: `Tools -> Port: xxx` where the `xxx` is the detected COM port.
* **Make sure you `esp32` library 3.3.5 installed**

#### Adding the external converter to Zigbee2mqtt
1. Copy the `z2m-converter.js` to your Zigbee2mqtt `data` folder, name the file if needed
2. Open Z2M web ui, go to "Setting -> External converters "
3. Add a new entry with the name of the converter js file
![External converters](/img/external_converters.png "External converters")
4. Click `Submit`
5. Restart Zigbee2mqtt
   
#### [Z2M 2.0] Adding the external converter to Zigbee2mqtt 
1. Copy the `z2m-converter.js` to your Zigbee2mqtt `data/external_converters` folder, name the file if needed
2. Restart Zigbee2mqtt

## Matter over Thread version (ESP32-C6)

[`matter-thread/`](matter-thread/main/app_main.cpp) is the same switch running Matter over Thread instead of Zigbee, so it works with Apple Home, Google Home, Home Assistant and other Matter controllers through a Thread border router (HomePod mini, Apple TV, Nest Hub, Home Assistant SkyConnect/ZBT-1, ...). It is an ESP-IDF project, not an Arduino sketch, because the Arduino Matter library for the C6 is prebuilt for Wi-Fi.

Install it from the [firmware installer](https://yuke-hd.github.io/zigbee-pcie-switch/) by choosing **Matter over Thread**, or download `esp32c6-matter.bin` from [GitHub Releases](https://github.com/Yuke-hd/zigbee-pcie-switch/releases) and write it at address `0x0`. Switching between Zigbee and Matter always wipes the device, so remove it from your old controller first.

To build it yourself, use ESP-IDF v5.5.4 (the `esp_matter` component is downloaded automatically on the first build):

```bash
cd matter-thread
. ~/esp/esp-idf/export.sh   # wherever your ESP-IDF v5.5 lives
idf.py set-target esp32c6
idf.py build flash monitor
```

Pairing: the serial monitor prints a QR code link and a manual pairing code. Add the device in your Matter app with either one (the test defaults are manual code `34970112332`, passcode `20202021`). The app pairs over Bluetooth, then hands the device your Thread network credentials. The firmware uses Espressif's test certificates, so your app will warn that the device is uncertified. That is expected for a DIY device. The status LED blinks until the device is paired and on Thread, then stays on. Hold BOOT for 3 s to factory reset.

The device is named "PC Switch" (product "PCIe Switch"). It shows up with three parts, which Home Assistant names "Power", "Reset" and "Status" from each endpoint's FixedLabel:

| Part | Matter type | Behaviour |
| --- | --- | --- |
| Power | On/off plug | Turning it on presses the power button for 0.2 s, then it turns itself back off |
| Reset | On/off plug | The same, for the reset button |
| Status | Contact sensor | Closed (contact) = PC is on |

## Resources

The ESP Zigbee SDK provides more examples:
* ESP Zigbee SDK Docs: [Link](https://docs.espressif.com/projects/esp-zigbee-sdk)
* ESP Zigbee SDK Repo: [Link](https://github.com/espressif/esp-zigbee-sdk)

* Official ESP32 Forum: [Link](https://esp32.com)
* Arduino-ESP32 Official Repository: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
* ESP32 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)
* ESP32-S2 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s2_datasheet_en.pdf)
* ESP32-C3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
* ESP32-S3 Datasheet: [Link to datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
* Official ESP-IDF documentation: [ESP-IDF](https://idf.espressif.com)
