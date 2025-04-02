#include <crypto_esp.h>
#include <crypto.h>
#include <net_client.h>

esp_aes_context aes_ctx;

void crypto_init(void)
{
  esp_aes_init(&aes_ctx);
  esp_aes_setkey(&aes_ctx, server_params.aes_key, 128);
  crc_init(server_params.aes_key);
}
