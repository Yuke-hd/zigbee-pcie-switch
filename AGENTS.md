# Repository Guidelines

## Project Structure & Module Organization

- `zigbee-pcie-switch.ino` contains the ESP32-C6/H2 Arduino firmware, Zigbee endpoints, GPIO handling, and device status logic.
- `pcie-switch.mjs` is the current ES-module Zigbee2MQTT external converter.
- `z2m-converter.js` is the legacy CommonJS converter retained for older Zigbee2MQTT installations. Keep behavior aligned between both converters when changing endpoints or exposes.
- `zha_quirks/` contains the Home Assistant ZHA Quirks v2 device handler. Keep its endpoint commands and reported entities aligned with the firmware and both Zigbee2MQTT converters.
- `.github/workflows/compile.yaml` defines the authoritative CI compile environment.
- `img/` stores documentation assets. `build/` contains generated Arduino artifacts and is ignored; do not commit it.

## Build, Test, and Development Commands

Install Arduino CLI and ESP32 Arduino core `3.3.5`, then compile with the same target options as CI:

```sh
arduino-cli compile --fqbn "esp32:esp32:esp32h2:PartitionScheme=zigbee,ZigbeeMode=ed" .
```

In Arduino IDE, select Zigbee End Device mode and the Zigbee 4 MB partition scheme before using **Verify** or **Upload**. CI runs the compile workflow on every push and pull request. There is no npm build or repository-local dependency installation.

## Coding Style & Naming Conventions

Follow the style of the file being edited: firmware uses two-space indentation and K&R braces; converters use four spaces. Use `UPPER_SNAKE_CASE` for pin/configuration macros, `snake_case` for C/C++ functions and variables, and descriptive endpoint names such as `power` and `reset`. Preserve endpoint IDs 10 and 11 and the model identifier unless intentionally making a compatibility-breaking change. The project has no configured formatter or linter, so keep diffs focused and remove trailing whitespace.

## Testing Guidelines

At minimum, run the Arduino compile above. Validate Python handler syntax with `python -m py_compile zha_quirks/pcie_switch.py`. Hardware-facing changes should also be checked on a supported ESP32-C6 or ESP32-H2 board: pairing, power/reset pulses, status reporting, and LED states. For integration changes, verify exposes, entities, and endpoint commands in Zigbee2MQTT or ZHA. Describe manual test hardware and results in the pull request.

## Commit & Pull Request Guidelines

Recent history favors short, imperative subjects, sometimes with Conventional Commit prefixes such as `feat:` and `chore:`. Prefer that form (for example, `fix: correct power status reporting`). Keep each commit scoped to one concern. Pull requests should explain the user-visible effect, identify tested board/core versions, link related issues, and include logs or screenshots when UI, pairing, or reporting behavior changes. Ensure the `test-compile` check passes before requesting review.
