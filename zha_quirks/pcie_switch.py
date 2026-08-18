"""ZHA device handler for the ESP32 Zigbee PCIe switch."""

from zhaquirks.builder import (
    BinarySensorDeviceClass,
    EntityType,
    QuirkBuilder,
    ReportingConfig,
)
from zigpy.zcl.clusters.general import BinaryInput, OnOff

MANUFACTURER = "Custom devices (DiY)"
MODEL = "ESP32C6.PCIE-switch"

POWER_ENDPOINT = 10
RESET_ENDPOINT = 11

# ZCL On/Off times are expressed in tenths of a second. These values match the
# Zigbee2MQTT converters and produce a 200 ms momentary button press.
BUTTON_COMMAND = {
    "on_off_control": 0,
    "on_time": 2,
    "off_wait_time": 2,
}

(
    QuirkBuilder(MANUFACTURER, MODEL)
    .friendly_name(
        manufacturer="Custom devices (DIY)",
        model="Zigbee PCIe switch",
    )
    .binary_sensor(
        attribute_name=BinaryInput.AttributeDefs.present_value.name,
        cluster_id=BinaryInput.cluster_id,
        endpoint_id=POWER_ENDPOINT,
        entity_type=EntityType.STANDARD,
        device_class=BinarySensorDeviceClass.POWER,
        reporting_config=ReportingConfig(
            min_interval=0,
            max_interval=300,
            reportable_change=1,
        ),
        unique_id_suffix="pc_power_status",
        fallback_name="PC power status",
        primary=True,
    )
    .command_button(
        command_name=OnOff.ServerCommandDefs.on_with_timed_off.name,
        cluster_id=OnOff.cluster_id,
        endpoint_id=POWER_ENDPOINT,
        command_kwargs=BUTTON_COMMAND,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="power_button",
        translation_key="power_button",
        fallback_name="Power button",
    )
    .command_button(
        command_name=OnOff.ServerCommandDefs.on_with_timed_off.name,
        cluster_id=OnOff.cluster_id,
        endpoint_id=RESET_ENDPOINT,
        command_kwargs=BUTTON_COMMAND,
        entity_type=EntityType.STANDARD,
        unique_id_suffix="reset_button",
        translation_key="reset_button",
        fallback_name="Reset button",
    )
    # The firmware declares On/Off server clusters so that it can receive the
    # timed commands. They are momentary inputs, not persistent switch states.
    .prevent_default_entity_creation(
        endpoint_id=POWER_ENDPOINT,
        cluster_id=OnOff.cluster_id,
    )
    .prevent_default_entity_creation(
        endpoint_id=RESET_ENDPOINT,
        cluster_id=OnOff.cluster_id,
    )
    .prevent_default_entity_creation(
        endpoint_id=POWER_ENDPOINT,
        cluster_id=BinaryInput.cluster_id,
    )
    .add_to_registry()
)
