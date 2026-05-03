#ifndef _PSA_CRYPTO_H
#define _PSA_CRYPTO_H

#include <psa/crypto.h>

#ifdef __cplusplus
extern "C" {
#endif

psa_status_t crypto_init(void);
void fill_random(void *buf, size_t len);
psa_status_t decrypt_payload(uint64_t source_mac, uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size, uint32_t *device_id);
psa_status_t encrypt_payload_aes(const uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size);
psa_status_t encrypt_payload_chacha(const uint8_t *payload, unsigned int payload_size, uint8_t **output, unsigned int *output_size);

#ifdef __cplusplus
}
#endif

#endif
