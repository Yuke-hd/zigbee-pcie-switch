/*
 * Matter over Thread version of the PCIe switch (ESP32-C6).
 *
 * Endpoints:
 *   Power  - on/off plug unit: "on" pulses POWER_PIN for PRESS_MS, then reports itself off again
 *   Reset  - same, for RESET_PIN
 *   Status - contact sensor: StateValue true (contact) = PC powered on
 */

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_timer.h>
#include <nvs_flash.h>

#include <app/clusters/boolean-state-server/boolean-state-cluster.h>
#include <app/server/Server.h>
#include <data_model_provider/esp_matter_data_model_provider.h>
#include <esp_matter_providers.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/DeviceInfoProvider.h>
#include <platform/ESP32/OpenthreadLauncher.h>

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "pcie_switch";

static constexpr gpio_num_t BOOT_PIN = GPIO_NUM_9;
static constexpr gpio_num_t POWER_PIN = GPIO_NUM_10;
static constexpr gpio_num_t RESET_PIN = GPIO_NUM_11;
static constexpr gpio_num_t POWER_STATUS_PIN = GPIO_NUM_15;
static constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_22;  // active low

static constexpr uint64_t PRESS_US = 200 * 1000;  // same 0.2 s pulse the Zigbee converter sends
static constexpr int LONG_PRESS_MS = 3000;
static constexpr int POLL_MS = 20;

struct Button {
    gpio_num_t pin;
    uint16_t endpoint_id;
    esp_timer_handle_t timer;
};

static Button s_power = {POWER_PIN, 0, nullptr};
static Button s_reset = {RESET_PIN, 0, nullptr};
static uint16_t s_status_endpoint_id = 0;

// One FixedLabel per endpoint. Home Assistant appends the "ha_entitylabel" value to the entity name
// for the test VID/PID (0xFFF1/0x8000), so the entities show up as "PC Switch Power" etc.
class LabelProvider : public chip::DeviceLayer::DeviceInfoProvider {
public:
    FixedLabelIterator *IterateFixedLabel(chip::EndpointId endpoint) override
    {
        const char *name = endpoint == s_power.endpoint_id ? "Power"
                         : endpoint == s_reset.endpoint_id ? "Reset"
                         : endpoint == s_status_endpoint_id ? "Status"
                         : nullptr;
        return name ? chip::Platform::New<OneLabel>(name) : nullptr;
    }
    // No UserLabel cluster, locales or calendar types on this device.
    UserLabelIterator *IterateUserLabel(chip::EndpointId) override { return nullptr; }
    SupportedLocalesIterator *IterateSupportedLocales() override { return nullptr; }
    SupportedCalendarTypesIterator *IterateSupportedCalendarTypes() override { return nullptr; }

protected:
    CHIP_ERROR SetUserLabelLength(chip::EndpointId, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR GetUserLabelLength(chip::EndpointId, size_t &) override { return CHIP_ERROR_NOT_IMPLEMENTED; }
    CHIP_ERROR SetUserLabelAt(chip::EndpointId, size_t, const UserLabelType &) override
    {
        return CHIP_ERROR_NOT_IMPLEMENTED;
    }
    CHIP_ERROR DeleteUserLabelAt(chip::EndpointId, size_t) override { return CHIP_ERROR_NOT_IMPLEMENTED; }

private:
    class OneLabel : public FixedLabelIterator {
    public:
        explicit OneLabel(const char *value) : mValue(value) {}
        size_t Count() override { return 1; }
        bool Next(FixedLabelType &out) override
        {
            if (mDone) {
                return false;
            }
            out.label = chip::CharSpan::fromCharString("ha_entitylabel");
            out.value = chip::CharSpan::fromCharString(mValue);
            mDone = true;
            return true;
        }
        void Release() override { chip::Platform::Delete(this); }

    private:
        const char *mValue;
        bool mDone = false;
    };
};

static LabelProvider s_label_provider;

// StartUpOnOff can turn OnOff on during start(); that must not press the PC button.
static bool s_ready = false;

static void set_on_off(intptr_t endpoint_id)
{
    esp_matter_attr_val_t val = esp_matter_bool(false);
    attribute::update((uint16_t)endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
}

static void set_contact(intptr_t contact)
{
    // BooleanState is a code-driven cluster in esp_matter 1.5: attribute::update() returns NOT_SUPPORTED.
    auto *cluster = static_cast<BooleanStateCluster *>(
        data_model::provider::get_instance().registry().Get({s_status_endpoint_id, BooleanState::Id}));
    if (cluster) {
        cluster->SetStateValue(contact != 0);
    }
}

static void release_button(void *arg)
{
    Button *button = (Button *)arg;
    gpio_set_level(button->pin, 0);
    // attribute::update must run on the Matter thread
    chip::DeviceLayer::PlatformMgr().ScheduleWork(set_on_off, button->endpoint_id);
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    if (type != attribute::PRE_UPDATE || cluster_id != OnOff::Id || attribute_id != OnOff::Attributes::OnOff::Id) {
        return ESP_OK;
    }
    Button *button = (Button *)priv_data;
    if (s_ready && button && val->val.b) {
        ESP_LOGI(TAG, "%s pressed", button == &s_power ? "POWER" : "RESET");
        gpio_set_level(button->pin, 1);
        esp_timer_stop(button->timer);  // a repeat "on" restarts the pulse
        esp_timer_start_once(button->timer, PRESS_US);
    }
    return ESP_OK;
}

static uint16_t create_button(node_t *node, Button *button)
{
    on_off_plug_in_unit::config_t config;
    config.on_off.on_off = false;
    endpoint_t *ep = on_off_plug_in_unit::create(node, &config, ENDPOINT_FLAG_NONE, button);
    assert(ep);
    cluster::fixed_label::config_t label_config;
    cluster::fixed_label::create(ep, &label_config, CLUSTER_FLAG_SERVER);
    button->endpoint_id = endpoint::get_id(ep);

    esp_timer_create_args_t args = {};
    args.callback = release_button;
    args.arg = button;
    args.name = "press";
    ESP_ERROR_CHECK(esp_timer_create(&args, &button->timer));
    return button->endpoint_id;
}

static void io_task(void *)
{
    int last_status = -1;
    int boot_held_ms = 0;
    int led_ms = 0;
    bool led_on = false;

    while (true) {
        int status = gpio_get_level(POWER_STATUS_PIN);
        if (status != last_status) {
            last_status = status;
            chip::DeviceLayer::PlatformMgr().ScheduleWork(set_contact, status);
        }

        // Hold BOOT for 3 s to factory-reset (leave the Matter fabric and Thread network).
        if (gpio_get_level(BOOT_PIN) == 0) {
            boot_held_ms += POLL_MS;
            if (boot_held_ms == LONG_PRESS_MS) {
                ESP_LOGI(TAG, "Factory reset");
                factory_reset();
            }
        } else {
            boot_held_ms = 0;
        }

        // LED: solid when commissioned and attached to Thread, blinking otherwise.
        led_ms += POLL_MS;
        if (led_ms >= 500) {
            led_ms = 0;
            chip::DeviceLayer::PlatformMgr().LockChipStack();
            bool joined = chip::Server::GetInstance().GetFabricTable().FabricCount() > 0 &&
                          chip::DeviceLayer::ConnectivityMgr().IsThreadAttached();
            chip::DeviceLayer::PlatformMgr().UnlockChipStack();
            led_on = joined ? true : !led_on;
            gpio_set_level(STATUS_LED_PIN, led_on ? 0 : 1);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}

static void init_gpio()
{
    gpio_config_t out = {};
    out.pin_bit_mask = (1ULL << POWER_PIN) | (1ULL << RESET_PIN) | (1ULL << STATUS_LED_PIN);
    out.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&out));
    gpio_set_level(POWER_PIN, 0);
    gpio_set_level(RESET_PIN, 0);
    gpio_set_level(STATUS_LED_PIN, 1);

    gpio_config_t boot = {};
    boot.pin_bit_mask = 1ULL << BOOT_PIN;
    boot.mode = GPIO_MODE_INPUT;
    boot.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&boot));

    gpio_config_t status = {};
    status.pin_bit_mask = 1ULL << POWER_STATUS_PIN;
    status.mode = GPIO_MODE_INPUT;
    ESP_ERROR_CHECK(gpio_config(&status));
}

extern "C" void app_main()
{
    nvs_flash_init();
    init_gpio();

    node::config_t node_config;
    strcpy(node_config.root_node.basic_information.node_label, "PC Switch");
    node_t *node = node::create(&node_config, app_attribute_update_cb, nullptr);
    assert(node);

    create_button(node, &s_power);
    create_button(node, &s_reset);

    contact_sensor::config_t status_config;
    status_config.boolean_state.state_value = gpio_get_level(POWER_STATUS_PIN);
    endpoint_t *status_ep = contact_sensor::create(node, &status_config, ENDPOINT_FLAG_NONE, nullptr);
    assert(status_ep);
    s_status_endpoint_id = endpoint::get_id(status_ep);
    cluster::fixed_label::config_t label_config;
    cluster::fixed_label::create(status_ep, &label_config, CLUSTER_FLAG_SERVER);

    ESP_LOGI(TAG, "Endpoints: power=%u reset=%u status=%u", s_power.endpoint_id, s_reset.endpoint_id,
             s_status_endpoint_id);

    esp_openthread_platform_config_t ot_config = {};
    ot_config.radio_config.radio_mode = RADIO_MODE_NATIVE;
    ot_config.host_config.host_connection_mode = HOST_CONNECTION_MODE_NONE;
    ot_config.port_config.storage_partition_name = "nvs";
    ot_config.port_config.netif_queue_size = 10;
    ot_config.port_config.task_queue_size = 10;
    set_openthread_platform_config(&ot_config);
    set_custom_device_info_provider(&s_label_provider);

    ESP_ERROR_CHECK(start(nullptr));
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

    // OnOff is persisted; a reboot mid-press would otherwise leave a button reported "on".
    chip::DeviceLayer::PlatformMgr().ScheduleWork(set_on_off, s_power.endpoint_id);
    chip::DeviceLayer::PlatformMgr().ScheduleWork(set_on_off, s_reset.endpoint_id);
    chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) { s_ready = true; }, 0);

    xTaskCreate(io_task, "io", 4096, nullptr, 5, nullptr);
}
