#include <stdio.h>
#define IEEE_802_15_4_COMMON_FUNCTIONS_ONLY_TYPES
#include <psa_crypto.h>
#include <sys/random.h>
#include <common_functions.h>
#include <stdlib.h>
#include <string.h>

main_config_t main_config;

static uint8_t message[128];

void fill_random(void *buf, size_t len)
{
  getrandom(buf, len, 0);
}

static int test_aes(int key_id)
{
  unsigned int output_size;
  uint8_t *output;
  psa_status_t rc = encrypt_payload_aes(key_id, message, sizeof(message), &output, &output_size, 1, 10);
  if (rc != PSA_SUCCESS || output[0] != 10)
  {
    printf("AES encryption error1 %d\n", rc);
    return 1;
  }
  free(output);
  rc = encrypt_payload_aes(key_id, message, sizeof(message), &output, &output_size, 1, 0);
  if (rc != PSA_SUCCESS)
  {
    printf("AES encryption error2 %d\n", rc);
    return 1;
  }
  unsigned int decrypted_size;
  uint8_t *decrypted;
  uint32_t device_id;
  rc = decrypt_payload(key_id, main_config.mac_mappings[0].device_mac_address, output, output_size, &decrypted, &decrypted_size, &device_id);
  if (rc != PSA_SUCCESS)
  {
    printf("AES decryption error %d\n", rc);
    return 1;
  }
  printf("AES decrypted message size %d\n", decrypted_size);
  if (decrypted_size == sizeof(message))
    printf("AES decrypted message comparison result %d\n", memcmp(decrypted, message, sizeof(message)));
  free(output);
  free(decrypted);
  return 0;
}

static int test_chacha(int key_id)
{
  unsigned int output_size;
  uint8_t *output;
  psa_status_t rc = encrypt_payload_chacha(key_id, message, sizeof(message), &output, &output_size, 1, 10);
  if (rc != PSA_SUCCESS || output[0] != 10)
  {
    printf("Chahca encryption error1 %d\n", rc);
    return 1;
  }
  free(output);
  rc = encrypt_payload_chacha(key_id, message, sizeof(message), &output, &output_size, 1, 0);
  if (rc != PSA_SUCCESS)
  {
    printf("Chahca encryption error2 %d\n", rc);
    return 1;
  }
  unsigned int decrypted_size;
  uint8_t *decrypted;
  uint32_t device_id;
  rc = decrypt_payload(key_id, main_config.mac_mappings[0].device_mac_address, output, output_size, &decrypted, &decrypted_size, &device_id);
  if (rc != PSA_SUCCESS)
  {
    printf("Chacha decryption error %d\n", rc);
    return 1;
  }
  printf("Chacha decrypted message size %d\n", decrypted_size);
  if (decrypted_size == sizeof(message))
    printf("Chacha decrypted message comparison result %d\n", memcmp(decrypted, message, sizeof(message)));
  free(output);
  free(decrypted);
  return 0;
}

int main(void)
{
  crypto_init();
  getrandom(message, sizeof(message), 0);
  getrandom(main_config.payload_encryption_key, sizeof(main_config.payload_encryption_key), 0);
  getrandom(main_config.server_parameters.aes_key, sizeof(main_config.server_parameters.aes_key), 0);
  uint64_t mac;
  getrandom(&mac, sizeof(mac), 0);
  main_config.mac_mappings[0].device_id = 1;
  main_config.mac_mappings[0].device_mac_address = mac;
  puts("KEY_PAYLOAD:");
  int rc = test_aes(KEY_PAYLOAD);
  if (rc)
    return rc;
  rc = test_chacha(KEY_PAYLOAD);
  if (rc)
    return rc;

  puts("KEY_SERVER:");
  rc = test_aes(KEY_SERVER);
  if (rc)
    return rc;
  return test_chacha(KEY_SERVER);
}
