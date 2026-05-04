#include "common_functions.h"
#include "ieee802154_net_client.h"
#include <lwip/sockets.h>
#include "esp_log.h"
#include "lwip/udp.h"

#define TAG "net_client"

static ip_addr_t server_addr;
struct udp_pcb *pcb = nullptr;

int net_client_init(void)
{
  int rc = ipaddr_aton(main_config.server_parameters.address, &server_addr);
  if (rc == 0)
    return 1;

  pcb = udp_new();
  if (pcb == nullptr)
  {
    ESP_LOGE(TAG, "Unable to create pcb");
    return 1;
  }
  err_t err = udp_bind(pcb, IP_ANY_TYPE, 0);
  if (err != ERR_OK)
  {
    ESP_LOGE(TAG, "Unable to bind pcb: %d", err);
    udp_remove(pcb);
    return err;
  }
  ESP_LOGI(TAG, "PCB created");
  return 0;
}

err_t send_udp(void *data, unsigned int len)
{
  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
  if (p == NULL)
  {
    ESP_LOGE(TAG, "Unable to allocate pbuf");
    return ERR_MEM;
  }
  memcpy(p->payload, data, len);
  ESP_LOGI(TAG, "Sending to %s:%d", main_config.server_parameters.address, main_config.server_parameters.port);
  err_t err = udp_sendto(pcb, p, &server_addr, main_config.server_parameters.port);
  if (err != ERR_OK)
    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
  else
    ESP_LOGI(TAG, "Message sent");
  pbuf_free(p);
  return err;
}
