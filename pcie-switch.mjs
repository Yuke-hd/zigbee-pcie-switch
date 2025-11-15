import {bind, presentValue} from "zigbee-herdsman-converters/lib/reporting";
import {presets, access as ea} from "zigbee-herdsman-converters/lib/exposes";
import * as utils from "zigbee-herdsman-converters/lib/utils";

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Fz.Converter<"genBinaryInput", undefined, "attributeReport">>} */
const fzLocal = {
    pci_power_status: {
        cluster: "genBinaryInput",
        type: "attributeReport",
        convert: (model, msg, publish, options, meta) => {
            return {status: msg.data["presentValue"] === 1};
        },
    },
};

/** @type{Record<string, import('zigbee-herdsman-converters/lib/types').Tz.Converter>} */
const tzLocal = {
    pci_power_control: {
        key: ["Power"],
        convertSet: async (entity, key, value, meta) => {
            const state = "on";

            utils.validateValue(state, ["toggle", "off", "on"]);

            const payload = {ctrlbits: 0, ontime: Math.round(2), offwaittime: Math.round(2)};
            await entity.command("genOnOff", "onWithTimedOff", payload, utils.getOptions(meta.mapped, entity));

            return {state_10: {state: state.toUpperCase()}};
        },
    },
    pci_reset_control: {
        key: ["Reset"],
        convertSet: async (entity, key, value, meta) => {
            const state = "on";

            utils.validateValue(state, ["toggle", "off", "on"]);

            const payload = {ctrlbits: 0, ontime: Math.round(2), offwaittime: Math.round(2)};
            await entity.command("genOnOff", "onWithTimedOff", payload, utils.getOptions(meta.mapped, entity));

            return {state_11: {state: state.toUpperCase()}};
        },
    },
};

/** @type{import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend | import('zigbee-herdsman-converters/lib/types').DefinitionWithExtend[]} */
export default {
    zigbeeModel: ["ESP32C6.PCIE-switch"],
    model: "ESP32C6.PCIE-switch",
    vendor: "Custom devices (DiY)",
    description: "PCIE switch",
    meta: {multiEndpoint: true},
    endpoint: (device) => ({power: 10, reset: 11}),
    fromZigbee: [fzLocal.pci_power_status],
    toZigbee: [tzLocal.pci_power_control, tzLocal.pci_reset_control],
    exposes: [
        presets.binary("status", ea.STATE, true, false).withDescription("Indicates if the PC is powered on (= true) or off (= false)"),
        presets.enum("Power", ea.SET, ["press"]).withDescription("Power button").withEndpoint("power"),
        presets.enum("Reset", ea.SET, ["press"]).withDescription("Reset button").withEndpoint("reset"),
    ],
    // The configure method below is needed to make the device reports on/off state changes
    // when the device is controlled manually through the button on it.
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10);
        await bind(endpoint, coordinatorEndpoint, ["genBinaryInput"]);
        await presentValue(endpoint, {min: 0, max: 300, change: 0});
    },
};
