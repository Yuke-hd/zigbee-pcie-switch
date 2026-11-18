import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import test from "node:test";
import { chips, validateImage, writeFirmware } from "./firmware.mjs";

for (const [name, chip] of Object.entries(chips)) {
  for (const role of Object.keys(chip.files)) test(`${name} ${role} image preserves pairing on a compatible layout`, async () => {
    const image = await readFile(new URL(`../release/${chip.files[role]}`, import.meta.url));
    validateImage(image, chip);
    const table = image.subarray(0x8000, 0x8c00);
    const writes = [];
    const loader = {
      chip: { CHIP_NAME: name },
      detectFlashSize: async () => "4MB",
      flashSizeBytes: () => 0x400000,
      readFlash: async (address, size) => {
        assert.equal(address, 0x8000);
        assert.equal(size, 0xc00);
        return table;
      },
      writeFlash: async (options) => writes.push(options),
    };

    await writeFirmware(loader, image, true, () => {});
    assert.equal(writes[0].eraseAll, false);
    assert.deepEqual(writes[0].fileArray, [
      { address: 0x10000, data: image.subarray(0x10000, 0x150000) },
      { address: 0xe000, data: image.subarray(0xe000, 0x10000) },
    ]);

    loader.readFlash = async () => new Uint8Array(table.length).fill(0xff);
    await assert.rejects(writeFirmware(loader, image, true), /same Zigbee partition layout/);
    assert.equal(writes.length, 1);

    loader.readFlash = async () => assert.fail("Fresh install must work on blank flash");
    await writeFirmware(loader, image, false, () => {});
    assert.deepEqual(writes[1].fileArray, [{ address: 0, data: image }]);
    const storageOffset = role === "router" ? 0x3ea000 : 0x3eb000;
    assert.ok(image.subarray(storageOffset, 0x3f0000).every((byte) => byte === 0xff));

    loader.chip.CHIP_NAME = name === "ESP32-H2" ? "ESP32-C6" : "ESP32-H2";
    await assert.rejects(writeFirmware(loader, image, false), /invalid for this device/);
    assert.equal(writes.length, 2);
  });
}
