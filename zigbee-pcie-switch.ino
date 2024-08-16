/**
 * Proper Zigbee mode must be selected in Tools->Zigbee mode
 * and also the correct partition scheme must be selected in Tools->Partition Scheme.
*/

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"

#define LED_PIN RGB_BUILTIN
#define BOOT_PIN 9
#define POWER_PIN 10
#define RESET_PIN 11
#define POWER_STATUS_PIN 15

/* Default End Device config */
#define ESP_ZB_ZED_CONFIG() \
  { \
    .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED, .install_code_policy = INSTALLCODE_POLICY_ENABLE, \
    .nwk_cfg = { \
      .zed_cfg = { \
        .ed_timeout = ED_AGING_TIMEOUT, \
        .keep_alive = ED_KEEP_ALIVE, \
      }, \
    }, \
  }

#define ESP_ZB_DEFAULT_RADIO_CONFIG() \
  { .radio_mode = ZB_RADIO_MODE_NATIVE, }

#define ESP_ZB_DEFAULT_HOST_CONFIG() \
  { .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE, }

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE false /* enable the install code policy for security */
#define ED_AGING_TIMEOUT ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE 3000                                               /* 3000 millisecond */
#define HA_ESP_SW1_ENDPOINT 10                                           /* used to process power status and power switch */
#define HA_ESP_SW2_ENDPOINT 11                                           /* used to process reset switch */
#define ESP_ZB_PRIMARY_CHANNEL_MASK ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK /* Zigbee primary channel mask use in the example */
#define POWER_SOURCE 0x04

/********************* Zigbee functions **************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
  ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
  uint32_t *p_sg_p = signal_struct->p_app_signal;
  esp_err_t err_status = signal_struct->esp_err_status;
  esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
  switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
      log_i("Zigbee stack initialized");
      esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
      break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
      if (err_status == ESP_OK) {
        log_i("Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
        if (esp_zb_bdb_is_factory_new()) {
          log_i("Start network formation");
          esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
          log_i("Device rebooted");
        }
      } else {
        /* commissioning failed */
        log_w("Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
      }
      break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
      if (err_status == ESP_OK) {
        esp_zb_ieee_addr_t extended_pan_id;
        esp_zb_get_extended_pan_id(extended_pan_id);
        log_i(
          "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
          extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4], extended_pan_id[3], extended_pan_id[2], extended_pan_id[1],
          extended_pan_id[0], esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
      } else {
        log_i("Network steering was not successful (status: %s)", esp_err_to_name(err_status));
        esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
      }
      break;
    default: log_i("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status)); break;
  }
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message) {
  esp_err_t ret = ESP_OK;
  switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID: ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message); break;
    default: log_w("Receive Zigbee action(0x%x) callback", callback_id); break;
  }
  return ret;
}

static void esp_zb_task(void *pvParameters) {
  char modelid[] = {19, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'P', 'C', 'I', 'E', '-', 's', 'w', 'i', 't', 'c', 'h'};
  char manufname[] = {20, 'C', 'u', 's', 't', 'o', 'm', ' ', 'd', 'e', 'v', 'i', 'c', 'e', 's', ' ', '(', 'D', 'i', 'Y', ')'};

  esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
  esp_zb_init(&zb_nwk_cfg);
  // esp_zb_on_off_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
  // esp_zb_ep_list_t *esp_zb_on_off_sw1_ep = esp_zb_on_off_switch_ep_create(HA_ESP_SW1_ENDPOINT, &switch_cfg);

  uint8_t zcl_version, power_source;
  zcl_version = 3;
  power_source = POWER_SOURCE;
  uint16_t on_time = 0x0014;
  /* basic cluster create with fully customized */
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
  esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manufname);
  esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, modelid);
  esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &zcl_version);
  esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &power_source);
  /* on-off cluster create with standard cluster config*/
  esp_zb_on_off_cluster_cfg_t on_off_cfg;
  on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
  esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);
  esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, &on_time);
  esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster, ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME, &on_time);
  esp_zb_attribute_list_t *esp_zb_on_off_cluster2 = esp_zb_on_off_cluster_create(&on_off_cfg);
  esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster2, ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME, &on_time);
  esp_zb_on_off_cluster_add_attr(esp_zb_on_off_cluster2, ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME, &on_time);
  // for future development
  // on-off-switch-config clsuter
  // esp_zb_on_off_switch_cluster_cfg_t on_off_switch_cfg;
  // on_off_switch_cfg.switch_type = 0x01;
  // on_off_switch_cfg.switch_action = 0x00;
  // esp_zb_attribute_list_t *esp_zb_on_off_switch_cluster = esp_zb_on_off_switch_cfg_cluster_create(&on_off_switch_cfg);
  
  /* binary-input cluster create with standard cluster config*/
  char active_text[] = { 2, 'O', 'n' };
  char inactive_text[] = { 3, 'O', 'f', 'f' };
  esp_zb_binary_input_cluster_cfg_t binary_input_cfg;
  binary_input_cfg.out_of_service = ESP_ZB_ZCL_BINARY_INPUT_OUT_OF_SERVICE_DEFAULT_VALUE;
  binary_input_cfg.status_flags = ESP_ZB_ZCL_BINARY_INPUT_STATUS_FLAG_DEFAULT_VALUE;
  esp_zb_attribute_list_t *esp_zb_binary_input_cluster = esp_zb_binary_input_cluster_create(&binary_input_cfg);

  bool initial_state = false;
  esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_ACTIVE_TEXT_ID, &active_text);
  esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &initial_state);
  esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_INACTIVE_TEXT_ID, &inactive_text);
  // esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, 0x0051, &initial_state);

  /* create cluster lists for this endpoint */
  esp_zb_cluster_list_t *esp_zb_cluster_list1 = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list1, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list1, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_binary_input_cluster(esp_zb_cluster_list1, esp_zb_binary_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_zb_cluster_list_t *esp_zb_cluster_list2 = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list2, esp_zb_on_off_cluster2, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
  /* add created endpoint (cluster_list) to endpoint list */
  esp_zb_endpoint_config_t endpoint_config = {
    .endpoint = HA_ESP_SW1_ENDPOINT,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0
  };
  esp_zb_endpoint_config_t endpoint_config2 = {
    .endpoint = HA_ESP_SW2_ENDPOINT,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
    .app_device_version = 0
  };

  esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list1, endpoint_config);
  esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list2, endpoint_config2);
  
  esp_zb_device_register(esp_zb_ep_list);

  esp_zb_core_action_handler_register(zb_action_handler);
  esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

  //Erase NVRAM before creating connection to new Coordinator
  // esp_zb_nvram_erase_at_start(true);  //Comment out this line to erase NVRAM data if you are conneting to new Coordinator

  ESP_ERROR_CHECK(esp_zb_start(false));
  esp_zb_main_loop_iteration();
}

/* Handle the light attribute */

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message) {
  esp_err_t ret = ESP_OK;
  bool pin_state = 0;

  if (!message) {
    log_e("Empty message");
  }
  if (message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Received message: error status(%d)", message->info.status);
  }

  log_i(
    "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster, message->attribute.id,
    message->attribute.data.size);
  if (message->info.dst_endpoint == HA_ESP_SW1_ENDPOINT) {
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
      if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        pin_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : pin_state;
        log_i("POWER sets to %s", pin_state ? "On" : "Off");
        digitalWrite(POWER_PIN, pin_state);
      }
    }
  }
  if (message->info.dst_endpoint == HA_ESP_SW2_ENDPOINT) {
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
      if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        pin_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : pin_state;
        log_i("RESET sets to %s", pin_state ? "On" : "Off");
        digitalWrite(RESET_PIN, pin_state);
      }
    }
  }
  return ret;
}

bool pcie_power_state;
void IRAM_ATTR check_power_status() {
  uint64_t interrupt_time = esp_timer_get_time();
  pcie_power_state = digitalRead(POWER_STATUS_PIN);

  log_i("power state changed");
  log_i("current state %s", new_state ? "high" : "low");
  esp_zb_zcl_set_attribute_val(10, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &pcie_power_state, false);
}

/********************* Arduino functions **************************/
void setup() {
  // Init Zigbee
  esp_zb_platform_config_t config = {
    .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
    .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
  };
  ESP_ERROR_CHECK(esp_zb_platform_config(&config));

  pinMode(POWER_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  pinMode(POWER_STATUS_PIN, INPUT);

  // Start Zigbee task
  xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 50, NULL);
  
  attachInterrupt(POWER_STATUS_PIN, check_power_status, CHANGE);
}

int lastState = HIGH;  // the previous state from the input pin
int currentState = LOW;     // the current reading from the input pin
uint64_t pressedTime  = 0;
uint64_t releasedTime = 0;
uint64_t LONG_PRESS_TIME = 3000 * 1000;

void loop() {
  // reset zb network
  currentState = digitalRead(BOOT_PIN);

  if(lastState == HIGH && currentState == LOW)        // button is pressed
    pressedTime = esp_timer_get_time();
  else if(lastState == LOW && currentState == HIGH) { // button is released
    releasedTime = esp_timer_get_time();

    uint64_t pressDuration = releasedTime - pressedTime;

    if( pressDuration > LONG_PRESS_TIME )
      log_i("Resetting Zigbee network configuration");
      esp_zb_bdb_reset_via_local_action();
      esp_zb_factory_reset();
  }

  // save the the last state
  lastState = currentState;
  vTaskDelay(500 / portTICK_PERIOD_MS);
}
