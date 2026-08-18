# zigbee-pcie-switch

![compile](https://github.com/Yuke-hd/zigbee-pcie-switch/actions/workflows/compile.yaml/badge.svg)


# Supported Targets

Currently, this example supports the following targets.

| Supported Targets | ESP32-C6 | ESP32-H2 |
| ----------------- | -------- | -------- |


### Demo
[![demo](https://img.youtube.com/vi/ddB3xJL942k/0.jpg)](https://www.youtube.com/shorts/ddB3xJL942k?feature=share)


#### Using Arduino IDE

To get more information about the Espressif boards see [Espressif Development Kits](https://www.espressif.com/en/products/devkits).

* Before Compile/Verify, select the correct board: `Tools -> Board`.
* Select the End device Zigbee mode: `Tools -> Zigbee mode: Zigbee ED (end device)`
* Select Partition Scheme for Zigbee: `Tools -> Partition Scheme: Zigbee 4MB with spiffs`
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

#### Adding the device handler to ZHA

The handler exposes **Power button**, **Reset button**, and a **PC power status**
binary sensor in Home Assistant.

1. Copy `zha_quirks/pcie_switch.py` to `/config/custom_zha_quirks/` on the Home Assistant host.
2. Add the following to Home Assistant's `configuration.yaml`:

   ```yaml
   zha:
     enable_quirks: true
     custom_quirks_path: /config/custom_zha_quirks
   ```

3. Restart Home Assistant, then pair the device with ZHA. If it was already
   paired and the handler is not shown on the device page, remove and pair it
   again.

The handler uses the current ZHA Quirks v2 API and matches manufacturer
`Custom devices (DiY)` with model `ESP32C6.PCIE-switch` exactly.

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
