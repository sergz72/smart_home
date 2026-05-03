#ifndef IEEE_802_15_4_COMMON_FUNCTIONS_H
#define IEEE_802_15_4_COMMON_FUNCTIONS_H

#ifndef IEEE_802_15_4_COMMON_FUNCTIONS_ONLY_TYPES
#include <esp_err.h>
#endif

#define SERVER_ADDRESS_LENGTH 64
#define AES_KEY_LENGTH 32
#define DEVICE_MAPPINGS_SIZE 16
#define MAIN_CONFIG_MAGIC 0x33

typedef struct {
  char address[SERVER_ADDRESS_LENGTH];
  unsigned char aes_key[AES_KEY_LENGTH];
  uint16_t port;
  uint16_t unused;
} server_parameters_t;

typedef struct {
  uint32_t device_id;
  uint32_t last_packet_number;
  uint64_t device_mac_address;
} mac_mapping_t;

typedef struct {
  uint8_t payload_encryption_key[AES_KEY_LENGTH];
  uint16_t pan_id;
  uint8_t channel;
  uint8_t magic;
  uint64_t host_mac_address;
  uint8_t wifi_ssid[32];
  uint8_t wifi_password[64];
  server_parameters_t server_parameters;
  mac_mapping_t mac_mappings[DEVICE_MAPPINGS_SIZE];
} main_config_t;

#ifndef IEEE_802_15_4_COMMON_FUNCTIONS_ONLY_TYPES

#ifdef __cplusplus
extern "C" {
#endif

void common_init(void);
esp_err_t read_main_configuration(void);
void set_wifi_configuration_from_main_configuration(void);
esp_err_t common_nvs_init(void);
void increment_packet_counter(void);

#ifdef __cplusplus
}
#endif

#endif

extern main_config_t main_config;

#endif
