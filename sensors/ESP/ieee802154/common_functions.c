#include "common_functions.h"
#include "aes/esp_aes.h"
#include "esp_hmac.h"
#include <esp_partition.h>
#include <nvs_flash.h>
#include <string.h>
#include <psa/crypto.h>
#include <esp_random.h>
#include "wifi.h"

#define STORAGE_PARTITION_NAME "storage"
#define MAIN_CONFIG_MAGIC 0x33

main_config_t main_config;

static esp_aes_context payload_ctx;
static psa_key_attributes_t psa_attributes = PSA_KEY_ATTRIBUTES_INIT;
static mbedtls_svc_key_id_t psa_key;
static uint32_t packet_counter;

static mac_mapping_t *search_mac_mapping(const uint64_t mac_address)
{
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
  {
    if (main_config.mac_mappings[i].device_mac_address == mac_address)
      return &main_config.mac_mappings[i];
  }
  return nullptr;
}

void common_init(void)
{
  packet_counter = 0;
  memset(&main_config, 0, sizeof(main_config));
}

esp_err_t read_main_configuration(void)
{
  // Find the partition labeled "storage"
  const esp_partition_t *partition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      ESP_PARTITION_SUBTYPE_DATA_UNDEFINED,
      STORAGE_PARTITION_NAME
  );

  if (partition == NULL)
    return ESP_ERR_NVS_PART_NOT_FOUND;

  const esp_err_t rc = esp_partition_read(partition, 0, &main_config, sizeof(main_config));
  if (rc != ESP_OK)
    return rc;
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
    main_config.mac_mappings[i].last_packet_number = 0;
  return ESP_OK;
}

void set_wifi_configuration_from_main_configuration(void)
{
  if (main_config.magic != MAIN_CONFIG_MAGIC)
    return;
  strncpy((char*)wifi_config.sta.ssid, (const char*)main_config.wifi_ssid, sizeof(main_config.wifi_ssid));
  strncpy((char*)wifi_config.sta.password, (const char*)main_config.wifi_password, sizeof(main_config.wifi_password));
}

esp_err_t common_nvs_init(void)
{
  // Initialize NVS
  esp_err_t rc = nvs_flash_init();
  if (rc == ESP_ERR_NVS_NO_FREE_PAGES || rc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    nvs_flash_erase();
    rc = nvs_flash_init();
  }
  return rc;
}

esp_err_t crypto_init(void)
{
  esp_aes_init(&payload_ctx);
  esp_aes_setkey(&payload_ctx, main_config.payload_encryption_key, 256);
  psa_crypto_init();
  psa_set_key_usage_flags(&psa_attributes, PSA_KEY_USAGE_SIGN_HASH);
  psa_set_key_algorithm(&psa_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
  psa_set_key_type(&psa_attributes, PSA_KEY_TYPE_HMAC);
  psa_set_key_bits(&psa_attributes, 256);
  //psa_set_key_lifetime(&psa_attributes, PSA_KEY_LIFETIME_ESP_HMAC_VOLATILE);
  psa_import_key(&psa_attributes, main_config.payload_encryption_key, sizeof(main_config.payload_encryption_key), &psa_key);
  return ESP_OK;
}

esp_err_t decrypt_payload(const uint64_t source_mac, uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size, uint32_t *device_id)
{
  mac_mapping_t *mapping = search_mac_mapping(source_mac);
  if (mapping == NULL)
    return ESP_ERR_INVALID_MAC;
  if (payload_size < 17+32)
    return ESP_ERR_INVALID_SIZE;
  uint8_t hmac[32];
  size_t hmac_length;
  if (psa_mac_compute(psa_key, PSA_ALG_HMAC(PSA_ALG_SHA_256), payload, payload_size, hmac, sizeof(hmac), &hmac_length) != PSA_SUCCESS)
    return ESP_ERR_INVALID_STATE;
  if (memcmp(hmac, payload + payload_size - 32, 32) != 0)
    return ESP_ERR_INVALID_CRC;
  const uint32_t *src_packet_number = (uint32_t*)(payload + 12);
  if (*src_packet_number == 1 || *src_packet_number <= mapping->last_packet_number)
    return ESP_ERR_INVALID_RESPONSE;
  mapping->last_packet_number = *src_packet_number;
  payload_size -= 16+32;
  *output_size = payload_size;
  void *o = malloc(payload_size);
  if (o == NULL)
    return ESP_ERR_NO_MEM;
  if (esp_aes_crypt_cbc(&payload_ctx, ESP_AES_DECRYPT, payload_size, payload, payload + 16, o) != 0)
  {
    free(o);
    return ESP_ERR_INVALID_SIZE;
  }
  *output = o;
  *device_id = mapping->device_id;
  return ESP_OK;
}

esp_err_t encrypt_payload(const uint8_t *payload, const unsigned int payload_size, uint8_t **output, unsigned int *output_size)
{
  *output_size = payload_size + 16 + 32;
  void *o = malloc(*output_size);
  if (o == NULL)
    return ESP_ERR_NO_MEM;
  esp_fill_random(o, 16);
  memcpy(o + 12, &packet_counter, 4);
  if (esp_aes_crypt_cbc(&payload_ctx, ESP_AES_ENCRYPT, payload_size, o, payload, o + 16) != 0)
  {
    free(o);
    return ESP_ERR_INVALID_SIZE;
  }
  size_t hmac_length;
  if (psa_mac_compute(psa_key, PSA_ALG_HMAC(PSA_ALG_SHA_256), payload, payload_size, o + payload_size + 16, 32, &hmac_length) != PSA_SUCCESS)
    return ESP_ERR_INVALID_STATE;
  *output = o;
  packet_counter++;
  return ESP_OK;
}
