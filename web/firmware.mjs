export const chips = {
  "ESP32-H2": { file: "esp32h2.bin", id: 16 },
  "ESP32-C6": { file: "esp32c6.bin", id: 13 },
};

const flashSize = 0x400000;
const tableOffset = 0x8000;
const tableSize = 0xc00;
const otaOffset = 0xe000;
const appOffset = 0x10000;
const appEnd = 0x150000;

export function validateImage(image, chip) {
  const headerMatches = (offset) =>
    image[offset] === 0xe9 && image[offset + 12] === chip.id && image[offset + 13] === 0;
  if (image.length !== flashSize || !headerMatches(0) || !headerMatches(appOffset)) {
    throw new Error("The release image is invalid for this device.");
  }

  // Check the partition entries before deriving any write ranges from this image.
  const table = new DataView(image.buffer, image.byteOffset + tableOffset, tableSize);
  const partitions = new Map();
  for (let entry = 0; entry < tableSize && table.getUint16(entry, true) === 0x50aa; entry += 32) {
    const type = table.getUint8(entry + 2);
    const subtype = table.getUint8(entry + 3);
    const offset = table.getUint32(entry + 4, true);
    const size = table.getUint32(entry + 8, true);
    if ((type !== 0 || subtype !== 0x10) && (type !== 1 || subtype !== 0)) {
      if (offset < appEnd && otaOffset < offset + size) {
        throw new Error("The release image overlaps data needed for Keep pairing.");
      }
    }
    partitions.set(type * 256 + subtype, { offset, size });
  }
  const app = partitions.get(0x10);
  const ota = partitions.get(0x100);
  if (app?.offset !== appOffset || app.size !== appEnd - appOffset ||
      ota?.offset !== otaOffset || ota.size !== appOffset - otaOffset) {
    throw new Error("The release image has an unsupported partition layout.");
  }
}

export async function writeFirmware(loader, image, keep, reportProgress) {
  const chip = chips[loader.chip.CHIP_NAME];
  if (!chip) throw new Error("Connect an ESP32-H2 or ESP32-C6 Zigbee PCIe Switch.");
  validateImage(image, chip);

  const capacity = await loader.detectFlashSize();
  if (!capacity || loader.flashSizeBytes(capacity) < flashSize) {
    throw new Error("This installer requires at least 4 MB of flash.");
  }

  let fileArray = [{ address: 0, data: image }];
  if (keep) {
    const expectedTable = image.subarray(tableOffset, tableOffset + tableSize);
    const currentTable = await loader.readFlash(tableOffset, tableSize);
    if (currentTable.length !== tableSize ||
        !currentTable.every((byte, index) => byte === expectedTable[index])) {
      throw new Error("Keep pairing requires the same Zigbee partition layout. Use Wipe for a fresh install.");
    }

    // Write the new app first, then select it using Arduino's OTA initialization.
    fileArray = [
      { address: appOffset, data: image.subarray(appOffset, appEnd) },
      { address: otaOffset, data: image.subarray(otaOffset, appOffset) },
    ];
  }

  await loader.writeFlash({
    fileArray,
    flashMode: "keep",
    flashFreq: "keep",
    flashSize: "keep",
    eraseAll: false,
    compress: true,
    reportProgress: (index, written, total) => reportProgress((index + written / total) / fileArray.length),
  });
}
