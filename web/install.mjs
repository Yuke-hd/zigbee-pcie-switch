import { chips, writeFirmware } from "./firmware.mjs";

const form = document.querySelector("form");
const fieldset = form.querySelector("fieldset");
const install = form.querySelector("button");
const status = document.querySelector("#status");
const progress = document.querySelector("progress");

let release;
try {
  if (!("serial" in navigator)) {
    throw new Error("Open this page in desktop Chrome or Edge to connect over USB.");
  }

  const response = await fetch("./release.json", { cache: "no-store" });
  if (!response.ok) throw new Error("Firmware is unavailable. Try again later.");
  release = await response.json();
  const version = document.querySelector("#version");
  version.textContent = release.tag;
  version.href = release.url;
  document.querySelector("#release").hidden = false;
  fieldset.disabled = install.disabled = false;
  status.textContent = "";
} catch (error) {
  status.textContent = error.message;
}

form.addEventListener("change", () => {
  install.textContent = form.elements.pairing.value === "keep"
    ? "Install and keep pairing"
    : "Wipe and install";
});

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  if (install.disabled) return;
  const keep = form.elements.pairing.value === "keep";
  fieldset.disabled = install.disabled = true;
  progress.hidden = true;
  let transport;
  try {
    status.textContent = "Select your switch’s USB port.";
    const port = await navigator.serial.requestPort();
    status.textContent = "Connecting to the switch…";
    const { ESPLoader, Transport } = await import("./esptool.mjs");
    transport = new Transport(port);
    const loader = new ESPLoader({ transport, baudrate: 115200 });
    await loader.main();

    const chip = chips[loader.chip.CHIP_NAME];
    if (!chip) throw new Error("Connect an ESP32-H2 or ESP32-C6 Zigbee PCIe Switch.");
    status.textContent = `Preparing firmware for ${loader.chip.CHIP_NAME}…`;
    const response = await fetch(`./${release.id}/${chip.file}`);
    if (!response.ok) throw new Error("Could not download firmware. Reload the page and try again.");
    const image = new Uint8Array(await response.arrayBuffer());

    await writeFirmware(loader, image, keep, (fraction) => {
      if (progress.hidden) status.textContent = "Writing firmware. Keep the USB cable connected.";
      progress.hidden = false;
      progress.value = fraction;
    });
    status.textContent = "Firmware written. Restarting the switch…";
    await loader.after("hard_reset");
    status.textContent = keep
      ? "Installation complete. Your Zigbee pairing was kept."
      : "Installation complete. Pair the switch with your Zigbee network.";
  } catch (error) {
    status.textContent = error.name === "NotFoundError" && !transport
      ? "No USB port selected."
      : error.message;
  } finally {
    progress.hidden = true;
    try {
      if (transport?.device.readable) await transport.disconnect();
    } catch (error) {
      console.error(error);
    }
    fieldset.disabled = install.disabled = false;
  }
});
