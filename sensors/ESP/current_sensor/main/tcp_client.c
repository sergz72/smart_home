#include "lwip/sockets.h"
#include "esp_log.h"
#include "net_client.h"
#include "common.h"
#include "queue.h"

static const char *TAG = "tcp_client";

static struct sockaddr_in dest_addr;

void net_client_init(void)
{
  dest_addr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);
  queue_init();
}

static int send_env1(void *data, int len)
{
  int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (sock < 0) {
    ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
    return sock;
  }
  ESP_LOGI(TAG, "Socket created, connecting to %s:%d", HOST_IP_ADDR, PORT);
  // Marking the socket as non-blocking
  int flags = fcntl(sock, F_GETFL);
  int err = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  if (err < 0)
  {
    ESP_LOGE(TAG, "Unable to set socket non blocking");
    shutdown(sock, 0);
    close(sock);
    return err;
  }
  err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err < 0) {
    if (errno == EINPROGRESS)
    {
      ESP_LOGI(TAG, "connection in progress");
      fd_set fdset;
      FD_ZERO(&fdset);
      FD_SET(sock, &fdset);

      // Connection in progress -> have to wait until the connecting socket is marked as writable, i.e. connection completes
      err = select(sock+1, NULL, &fdset, NULL, NULL);
      if (err < 0)
      {
        ESP_LOGE(TAG, "Error during connection: select for socket to be writable");
        shutdown(sock, 0);
        close(sock);
        return err;
      } else if (err == 0)
      {
        ESP_LOGE(TAG, "Connection timeout: select for socket to be writable");
        shutdown(sock, 0);
        close(sock);
        return -1;
      }
      else
      {
        int sockerr;
        socklen_t len = (socklen_t)sizeof(int);

        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void*)(&sockerr), &len) < 0)
        {
          ESP_LOGE(TAG, "Error when getting socket error using getsockopt()");
          shutdown(sock, 0);
          close(sock);
          return -1;
        }
        if (sockerr)
        {
          ESP_LOGE(TAG, "Connection error");
          shutdown(sock, 0);
          close(sock);
          return -1;
        }
      }
    }
    else
    {
      ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
      shutdown(sock, 0);
      close(sock);
      return err;
    }
  }
  ESP_LOGI(TAG, "Successfully connected, sending data...");
  err = send(sock, data, len, 0);
  if (err < 0)
    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
  else
    ESP_LOGI(TAG, "Message sent");
  shutdown(sock, 0);
  close(sock);
  return err;
}

void send_env(void *data, int len)
{
  void *p;

  for (;;)
  {
    p = queue_peek();
    if (!p)
      break;
    if (send_env1(p, len) >= 0)
      queue_pop();
    else
      return;
  }
  if (send_env1(data, len) < 0)
    queue_push(data);
}
