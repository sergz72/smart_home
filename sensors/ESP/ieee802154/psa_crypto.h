#ifndef _PSA_CRYPTO_H
#define _PSA_CRYPTO_H

#include <psa/crypto.h>

#define KEY_PAYLOAD 0
#define KEY_SERVER  1

#ifdef __cplusplus
extern "C" {
#endif

psa_status_t crypto_init(void);
void fill_random(void *buf, size_t len);
psa_status_t decrypt_payload(int key_id, uint64_t source_mac, uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size, uint32_t *device_id);
psa_status_t encrypt_payload_aes(int key_id, const uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size, uint32_t packet_counter, uint8_t byte0);
psa_status_t encrypt_payload_chacha(int key_id, const uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size, uint32_t packet_counter, uint8_t byte0);

#ifdef __cplusplus
}
#endif

extern int psa_error_step;

#endif
