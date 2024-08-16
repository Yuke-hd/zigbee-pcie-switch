
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const utils = require('zigbee-herdsman-converters/lib/utils');
const e = exposes.presets;
const ea = exposes.access;

const definition = {
    zigbeeModel: ['ESP32C6.PCIE-switch'],
    model: 'ESP32C6.PCIE-switch',
    vendor: 'Custom devices (DiY)',
    description: 'PCIE switch',
    fromZigbee: [
        {
            cluster: 'genBinaryInput',
            type: 'attributeReport',
            convert: (model, msg, publish, options, meta) => {
                return {"status": msg.data['presentValue'] == 1};
            },
        }
    ],
    meta: {multiEndpoint: true},
    endpoint: (device) => {
        return {power: 10, reset: 11};
    },
    toZigbee: [
    {
        key: ['Power'],
        convertSet: async (entity, key, value, meta) => {
            const state = "on";
            
            utils.validateValue(state, ['toggle', 'off', 'on']);

            const payload = {ctrlbits: 0, ontime: Math.round(2), offwaittime: Math.round(2)};
            await entity.command('genOnOff', 'onWithTimedOff', payload, utils.getOptions(meta.mapped, entity));
            
            return {"state_10": {state: state.toUpperCase()}};

        },
    },{
        key: ['Reset'],
        convertSet: async (entity, key, value, meta) => {
            const state = "on";

            utils.validateValue(state, ['toggle', 'off', 'on']);

            const payload = {ctrlbits: 0, ontime: Math.round(2), offwaittime: Math.round(2)};
            await entity.command('genOnOff', 'onWithTimedOff', payload, utils.getOptions(meta.mapped, entity));
            
            return {"state_11": {state: state.toUpperCase()}};
        },
    }],
    exposes: [
        e.binary('status', ea.STATE, true, false).withDescription('Indicates if the PC is powered on (= true) or off (= false)'),
        e.enum('Power', ea.SET, ['press']).withDescription('Power button').withEndpoint('power'),
        e.enum('Reset', ea.SET, ['press']).withDescription('Reset button').withEndpoint('reset')
],
    // The configure method below is needed to make the device reports on/off state changes
    // when the device is controlled manually through the button on it.
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(10);
        await reporting.bind(endpoint, coordinatorEndpoint, ['genBinaryInput']);
        await reporting.presentValue(endpoint,{min: 60, max: 3600, change: 0});
    },
};

module.exports = definition;