#include "lwip/sockets.h"
#include "esp_log.h"
#include "net_client.h"
#include "common.h"

static const char *TAG = "udp_client";

static struct sockaddr_in dest_addr;

void net_client_init(void)
{
  dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);
}

void send_env(void *data, int len)
{
  int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    return;
  }
  ESP_LOGI(TAG, "Socket created, sending to %s:%d", HOST_IP_ADDR, PORT);
  int err = sendto(sock, data, len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0)
    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
  else
    ESP_LOGI(TAG, "Message sent");
  shutdown(sock, 0);
  close(sock);
}