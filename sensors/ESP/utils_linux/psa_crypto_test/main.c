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

static int test_aes(void)
{
  unsigned int output_size;
  uint8_t *output;
  psa_status_t rc = encrypt_payload_aes(message, sizeof(message), &output, &output_size);
  if (rc != PSA_SUCCESS)
  {
    printf("AES encryption error %d\n", rc);
    return 1;
  }
  unsigned int decrypted_size;
  uint8_t *decrypted;
  uint32_t device_id;
  rc = decrypt_payload(main_config.mac_mappings[0].device_mac_address, output, output_size, &decrypted, &decrypted_size, &device_id);
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

static int test_chacha(void)
{
  unsigned int output_size;
  uint8_t *output;
  psa_status_t rc = encrypt_payload_chacha(message, sizeof(message), &output, &output_size);
  if (rc != PSA_SUCCESS)
  {
    printf("Chahca encryption error %d\n", rc);
    return 1;
  }
  unsigned int decrypted_size;
  uint8_t *decrypted;
  uint32_t device_id;
  rc = decrypt_payload(main_config.mac_mappings[0].device_mac_address, output, output_size, &decrypted, &decrypted_size, &device_id);
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
  uint64_t mac;
  getrandom(&mac, sizeof(mac), 0);
  main_config.mac_mappings[0].device_id = 1;
  main_config.mac_mappings[0].device_mac_address = mac;
  int rc = test_aes();
  if (rc)
    return rc;
  return test_chacha();
}
