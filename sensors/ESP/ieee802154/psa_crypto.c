#include <psa_crypto.h>
#define IEEE_802_15_4_COMMON_FUNCTIONS_ONLY_TYPES
#include <common_functions.h>
#include <stdlib.h>
#include <string.h>

#define ENCRYPTION_AES    0
#define ENCRYPTION_CHACHA 1

static mbedtls_svc_key_id_t psa_key_hmac[2];
static mbedtls_svc_key_id_t psa_key_aes[2];
static mbedtls_svc_key_id_t psa_key_chacha[2];
static uint32_t packet_counter;
static const psa_key_attributes_t psa_attributes_init = PSA_KEY_ATTRIBUTES_INIT;

static mac_mapping_t *search_mac_mapping(const uint64_t mac_address)
{
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
  {
    const uint64_t address = main_config.mac_mappings[i].device_mac_address;
    if (address == 0)
      break;
    if (address == mac_address)
      return &main_config.mac_mappings[i];
  }
  return nullptr;
}

psa_status_t init_keys( const uint8_t* key, const int idx)
{
  psa_key_attributes_t psa_attributes = PSA_KEY_ATTRIBUTES_INIT;

  psa_set_key_usage_flags(&psa_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
  psa_set_key_algorithm(&psa_attributes, PSA_ALG_CTR);
  psa_set_key_type(&psa_attributes, PSA_KEY_TYPE_AES);
  psa_set_key_bits(&psa_attributes, 256);
  //psa_set_key_lifetime(&psa_attributes, PSA_KEY_LIFETIME_ESP_HMAC_VOLATILE);
  psa_status_t rc = psa_import_key(&psa_attributes, key, 32, &psa_key_aes[idx]);
  if (rc != PSA_SUCCESS)
    return rc;

  memcpy(&psa_attributes, &psa_attributes_init, sizeof(psa_attributes));
  psa_set_key_usage_flags(&psa_attributes, PSA_KEY_USAGE_SIGN_HASH);
  psa_set_key_algorithm(&psa_attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
  psa_set_key_type(&psa_attributes, PSA_KEY_TYPE_HMAC);
  psa_set_key_bits(&psa_attributes, 256);
  //psa_set_key_lifetime(&psa_attributes, PSA_KEY_LIFETIME_ESP_HMAC_VOLATILE);
  rc = psa_import_key(&psa_attributes, key, 32, &psa_key_hmac[idx]);
  if (rc != PSA_SUCCESS)
    return rc;

  memcpy(&psa_attributes, &psa_attributes_init, sizeof(psa_attributes));
  psa_set_key_usage_flags(&psa_attributes, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
  psa_set_key_algorithm(&psa_attributes, PSA_ALG_STREAM_CIPHER);
  psa_set_key_type(&psa_attributes, PSA_KEY_TYPE_CHACHA20);
  psa_set_key_bits(&psa_attributes, 256);
  //psa_set_key_lifetime(&psa_attributes, PSA_KEY_LIFETIME_ESP_HMAC_VOLATILE);
  return psa_import_key(&psa_attributes, key, 32, &psa_key_chacha[idx]);
}
psa_status_t crypto_init(void)
{
  packet_counter = 1;

  psa_status_t rc = psa_crypto_init();
  if (rc != PSA_SUCCESS)
    return rc;

  rc = init_keys(main_config.payload_encryption_key, 0);
  if (rc != PSA_SUCCESS)
    return rc;
  return init_keys(main_config.server_parameters.aes_key, 1);
}

static psa_status_t decrypt(const mbedtls_svc_key_id_t key, psa_algorithm_t alg, const int iv_size, uint8_t*encrypted,
  const unsigned int length, void *output)
{
  psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
  psa_status_t rc = psa_cipher_decrypt_setup(&operation, key, alg);
  if (rc != PSA_SUCCESS)
    return rc;
  size_t output_len;
  rc = psa_cipher_set_iv(&operation, encrypted, iv_size);
  if (rc != PSA_SUCCESS)
    return rc;
  rc = psa_cipher_update(&operation, encrypted + iv_size, length, output, length, &output_len);
  if (rc != PSA_SUCCESS)
    return rc;
  return psa_cipher_finish(&operation, encrypted + iv_size + output_len, 0, &output_len);
}

psa_status_t decrypt_payload(const int key_id, const uint64_t source_mac, uint8_t *payload, unsigned int payload_size,
  uint8_t **output, unsigned int *output_size, uint32_t *device_id)
{
  mac_mapping_t *mapping = search_mac_mapping(source_mac);
  if (mapping == NULL)
    return PSA_ERROR_DOES_NOT_EXIST;
  unsigned int packet_number_offset;
  switch (payload[0])
  {
    case ENCRYPTION_AES:
      packet_number_offset = 13;
      break;
    case ENCRYPTION_CHACHA:
      packet_number_offset = 9;
      break;
    default:
      return PSA_ERROR_NOT_SUPPORTED;
  }
  if (payload_size < packet_number_offset+4+33)
    return PSA_ERROR_DATA_INVALID;
  uint8_t hmac[32];
  size_t hmac_length;
  psa_status_t rc = psa_mac_compute(psa_key_hmac[key_id], PSA_ALG_HMAC(PSA_ALG_SHA_256), payload,
    payload_size - 32, hmac, sizeof(hmac), &hmac_length);
  if (rc != PSA_SUCCESS)
    return rc;
  if (memcmp(hmac, payload + payload_size - 32, 32) != 0)
    return PSA_ERROR_INVALID_SIGNATURE;
  uint32_t src_packet_number;
  memcpy(&src_packet_number, payload + packet_number_offset, 4);
  if (src_packet_number != 1 && src_packet_number <= mapping->last_packet_number)
    return PSA_ERROR_ALREADY_EXISTS;
  mapping->last_packet_number = src_packet_number;
  payload_size -= packet_number_offset+4+32;
  *output_size = payload_size;
  void *o = malloc(payload_size);
  if (o == NULL)
    return PSA_ERROR_INSUFFICIENT_MEMORY;
  switch (payload[0])
  {
    case ENCRYPTION_AES:
      rc = decrypt(psa_key_aes[key_id], PSA_ALG_CTR, 16, payload + 1, payload_size, o);
      break;
    case ENCRYPTION_CHACHA:
      rc = decrypt(psa_key_chacha[key_id], PSA_ALG_STREAM_CIPHER, 12, payload + 1, payload_size, o);
      break;
    default:
      free(o);
      return PSA_ERROR_NOT_SUPPORTED;
  }
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  *output = o;
  *device_id = mapping->device_id;
  return PSA_SUCCESS;
}

void increment_packet_counter(void)
{
  packet_counter++;
}

static psa_status_t encrypt_payload(const uint8_t encryption_type, psa_algorithm_t alg, const mbedtls_svc_key_id_t key, const int key_id, const int iv_size,
  const uint8_t *payload, const unsigned int payload_size, uint8_t **output, unsigned int *output_size)
{
  *output_size = payload_size + iv_size + 33;
  uint8_t *o = malloc(*output_size);
  if (o == NULL)
    return PSA_ERROR_INSUFFICIENT_MEMORY;
  o[0] = encryption_type;
  fill_random(o+1, iv_size - 4);
  memcpy(o + iv_size - 3, &packet_counter, 4);
  psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
  psa_status_t rc = psa_cipher_encrypt_setup(&operation, key, alg);
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  size_t output_len;
  rc = psa_cipher_set_iv(&operation, o+1, iv_size);
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  rc = psa_cipher_update(&operation, payload, payload_size, o + iv_size + 1, payload_size, &output_len);
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  rc = psa_cipher_finish(&operation, o + iv_size + 1 + output_len, 0, &output_len);
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  size_t hmac_length;
  rc = psa_mac_compute(psa_key_hmac[key_id], PSA_ALG_HMAC(PSA_ALG_SHA_256), o,
    payload_size + iv_size + 1, o + payload_size + iv_size + 1, 32, &hmac_length);
  if (rc != PSA_SUCCESS)
  {
    free(o);
    return rc;
  }
  *output = o;
  return PSA_SUCCESS;
}

psa_status_t encrypt_payload_aes(const int key_id, const uint8_t *payload, const unsigned int payload_size, uint8_t **output,
  unsigned int *output_size)
{
  return encrypt_payload(ENCRYPTION_AES, PSA_ALG_CTR, psa_key_aes[key_id], key_id, 16,
    payload, payload_size, output, output_size);
}

psa_status_t encrypt_payload_chacha(const int key_id, const uint8_t *payload, const unsigned int payload_size,
  uint8_t **output, unsigned int *output_size)
{
  return encrypt_payload(ENCRYPTION_CHACHA, PSA_ALG_STREAM_CIPHER, psa_key_chacha[key_id], key_id,
    12, payload, payload_size, output, output_size);
}
