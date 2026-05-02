#include <stdio.h>
#define IEEE_802_15_4_COMMON_FUNCTIONS_ONLY_TYPES
#include <stdint.h>
#include <common_functions.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#define CREATE               0
#define SET_CHANNEL          1
#define SET_HOST_MAC_ADDRESS 2
#define SET_WIFI_SSID        3
#define SET_WIFI_PASSWORD    4
#define SET_SERVER_NAME      5
#define SET_SERVER_PORT      6
#define SET_SERVER_KEY       7
#define SET_ENCRYPTION_KEY   8
#define ADD_MAC_MAPPING      9
#define REMOVE_MAC_MAPPING   10
#define SHOW                 11
#define CREATE_DEVICE_FILE   12

#define MAX_ACTIONS 16

typedef struct
{
  int action;
  unsigned long value;
  char *pvalue;
} action_t;

static main_config_t config, device_config;
static action_t actions[MAX_ACTIONS];
static const char *file_name = "";
static int action_no = 0;

static int parse_arguments(int argc, char** argv)
{
  for (int i = 1; i < argc; i++)
  {
    if (strcmp(argv[i], "--create") == 0)
    {
      actions[action_no++].action = CREATE;
      continue;
    }
    if (strcmp(argv[i], "--show") == 0)
    {
      actions[action_no++].action = SHOW;
      continue;
    }
    bool set_channel = strcmp(argv[i], "--set-channel") == 0;
    bool set_host_mac = strcmp(argv[i], "--set-host-mac") == 0;
    bool set_wifi_ssid = strcmp(argv[i], "--set-wifi-ssid") == 0;
    bool set_wifi_password = strcmp(argv[i], "--set-wifi-password") == 0;
    bool set_server_name = strcmp(argv[i], "--set-server-name") == 0;
    bool set_server_port = strcmp(argv[i], "--set-server-port") == 0;
    bool set_server_key = strcmp(argv[i], "--set-server-key") == 0;
    bool set_encryption_key = strcmp(argv[i], "--set-encryption-key") == 0;
    bool remove_mac_mapping = strcmp(argv[i], "--remove-mac-mapping") == 0;
    bool create_device_file = strcmp(argv[i], "--create-device-file") == 0;
    if (set_channel || set_host_mac || set_server_port || remove_mac_mapping)
    {
      i++;
      if (i >= argc)
        return 1;
      actions[action_no].value = strtoul(argv[i], nullptr, 0);
      actions[action_no++].action = set_channel
        ? SET_CHANNEL
        : set_host_mac
          ? SET_HOST_MAC_ADDRESS
          : set_server_port ? SET_SERVER_PORT : REMOVE_MAC_MAPPING;
    }
    else if (set_wifi_ssid || set_wifi_password || set_server_name || set_server_key || set_encryption_key || create_device_file)
    {
      i++;
      if (i >= argc)
        return 2;
      actions[action_no].pvalue = argv[i];
      actions[action_no++].action = set_wifi_ssid
        ? SET_WIFI_SSID
        : set_wifi_password
          ? SET_WIFI_PASSWORD
          : set_server_name
            ? SET_SERVER_NAME
            : set_server_key
              ? SET_SERVER_KEY
              : set_encryption_key
                ? SET_ENCRYPTION_KEY : CREATE_DEVICE_FILE;
    }
    else if (strcmp(argv[i], "--add-mac-mapping") == 0)
    {
      i += 2;
      if (i >= argc)
        return 3;
      actions[action_no].value = strtoul(argv[i-1], nullptr, 0);
      actions[action_no].pvalue = (void*)strtoul(argv[i], nullptr, 0);
      actions[action_no++].action = ADD_MAC_MAPPING;
    }
    else if (argv[i][0] == '-')
      return 4;
    else
      file_name = argv[i];
  }
  return action_no == 0 || file_name[0] == 0 ? 5 : 0;
}

static void usage(void)
{
  puts("Usage: config_builder [--create][--show][--create-device-file][--set-channel][--set-host-mac][--set-wifi-ssid][--set-wifi-password][--set-server-name][--set-server-port][--set-server-key][--set-encryption-key][--add-mac-mapping][--remove-mac-mapping] file_name");
}

static int create_config(void)
{
  memset(&config, 0, sizeof(config));
  getrandom(&config.payload_encryption_key, sizeof(config.payload_encryption_key), 0);
  getrandom(&config.server_parameters.aes_key, sizeof(config.server_parameters.aes_key), 0);
  getrandom(&config.pan_id, sizeof(config.pan_id), 0);
  config.channel = 26;
  config.magic = MAIN_CONFIG_MAGIC;
}

static int create_device_file(const char *device_file_name)
{
  memset(&device_config, 0, sizeof(device_config));
  memcpy(&device_config.payload_encryption_key, &config.payload_encryption_key, sizeof(config.payload_encryption_key));
  device_config.pan_id = config.pan_id;
  device_config.channel = config.channel;
  device_config.host_mac_address = config.host_mac_address;
  device_config.magic = MAIN_CONFIG_MAGIC;

  FILE *f = fopen(device_file_name, "wb");
  if (f == NULL)
    return 1;
  size_t size = fwrite(&device_config, sizeof(device_config), 1, f);
  if (ferror(f) || size != 1)
  {
    fclose(f);
    return 1;
  }
  fclose(f);
  return 0;
}

static int set_channel(unsigned long value)
{
  if (value < 11 || value > 26)
    return 1;
  config.channel = value;
  return 0;
}

static void set_host_mac_address(unsigned long value)
{
  config.host_mac_address = value;
}

static void set_wifi_ssid(char* pvalue)
{
  strncpy((char*)config.wifi_ssid, pvalue, sizeof(config.wifi_ssid) - 1);
  config.wifi_ssid[sizeof(config.wifi_ssid) - 1] = 0;
}

static void set_wifi_password(char* pvalue)
{
  strncpy((char*)config.wifi_password, pvalue, sizeof(config.wifi_password) - 1);
  config.wifi_password[sizeof(config.wifi_password) - 1] = 0;
}

static void set_server_name(char* pvalue)
{
  strncpy(config.server_parameters.address, pvalue, sizeof(config.server_parameters.address) - 1);
  config.server_parameters.address[sizeof(config.server_parameters.address) - 1] = 0;
}

static int set_server_port(unsigned long value)
{
  if (value == 0 || value > 65535)
    return 1;
  config.server_parameters.port = value;
  return 0;
}

static int set_key(char *dest, char* value)
{
  if (!strcmp(value, "random"))
  {
    getrandom(dest, 32, 0);
    return 0;
  }

  FILE *f = fopen(value, "rb");
  if (f == NULL)
    return 1;
  size_t size = fread(dest, 32, 1, f);
  if (ferror(f) || size != 1)
  {
    fclose(f);
    return 1;
  }
  fclose(f);
  return 0;
}

static int set_server_key(char* pvalue)
{
  return set_key((char*)config.server_parameters.aes_key, pvalue);
}

static int set_encryption_key(char* pvalue)
{
  return set_key((char*)config.payload_encryption_key, pvalue);
}

static int add_mac_mapping(unsigned long device_id, uint64_t mac_address)
{
  if (device_id == 0 || device_id > 255)
    return 1;
  int i;
  for (i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
  {
    uint32_t did = config.mac_mappings[i].device_id;
    if (did == 0)
      break;
    if (did == device_id)
      return 1;
  }
  if (i == DEVICE_MAPPINGS_SIZE)
    return 1;
  config.mac_mappings[i].device_id = device_id;
  config.mac_mappings[i].device_mac_address = mac_address;
  return 0;
}

static void move_mappings(int idx)
{
  idx++;
  while (idx < DEVICE_MAPPINGS_SIZE)
  {
    if (config.mac_mappings[idx].device_id == 0)
      break;
    config.mac_mappings[idx - 1] = config.mac_mappings[idx];
    idx++;
  }
  config.mac_mappings[idx - 1].device_id = 0;
  config.mac_mappings[idx - 1].device_mac_address = 0;
}

static int remove_mac_mapping(unsigned long device_id)
{
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
  {
    uint32_t did = config.mac_mappings[i].device_id;
    if (did == 0)
      break;
    if (did == device_id)
    {
      move_mappings(i);
      return 0;
    }
  }
  return 1;
}

static void print_key(char* name, uint8_t* key)
{
  printf("%s:", name);
  for (int i = 0; i < 32; i++)
  {
    if (i % 8 == 0)
      printf("\n  ");
    printf("0x%02X ", key[i]);
  }
  puts("");
}

static void show_config(void)
{
  print_key("Encryption key", config.payload_encryption_key);
  printf("Pan id: 0x%04X\n", config.pan_id);
  printf("Channel: %d\n", config.channel);
  printf("Host MAC address: 0x%lx\n", config.host_mac_address);
  printf("WIFI ssid: %s\n", (char*)config.wifi_ssid);
  printf("WIFI Password: %s\n", (char*)config.wifi_password);
  printf("Server port: %d\n", config.server_parameters.port);
  printf("Server address: %s\n", config.server_parameters.address);
  print_key("Server key", config.server_parameters.aes_key);
  puts("MAC mappings:");
  for (int i = 0; i < DEVICE_MAPPINGS_SIZE; i++)
  {
    if (config.mac_mappings[i].device_mac_address == 0)
      break;
    printf("Device id: %d, MAC: 0x%lx\n", config.mac_mappings[i].device_id, config.mac_mappings[i].device_mac_address);
  }
}

int main(int argc, char **argv)
{
  int rc = parse_arguments(argc, argv);
  if (rc != 0)
  {
    puts("Arguments parsing error");
    usage();
    return rc;
  }
  printf("File name: %s\n", file_name);
  FILE *f = fopen(file_name, "rb+");
  if (f == NULL)
  {
    f = fopen(file_name, "wb");
    if (f == NULL)
    {
      puts("Failed to open/create file");
      return 6;
    }
  }
  else
  {
    size_t size = fread(&config, sizeof(config), 1, f);
    if (ferror(f) || size != 1)
    {
      puts("Failed to read config from file");
      fclose(f);
      return 7;
    }
  }
  bool modified = false;
  for (int i = 0; i < action_no; i++)
  {
    switch (actions[i].action)
    {
      case CREATE:
        create_config();
        modified = true;
        break;
      case SET_CHANNEL:
        if (set_channel(actions[i].value))
        {
          puts("Set channel error");
          fclose(f);
          return 10;
        }
        modified = true;
      break;
      case SET_HOST_MAC_ADDRESS:
        set_host_mac_address(actions[i].value);
        modified = true;
        break;
      case SET_WIFI_SSID:
        set_wifi_ssid(actions[i].pvalue);
        modified = true;
        break;
      case SET_WIFI_PASSWORD:
        set_wifi_password(actions[i].pvalue);
        modified = true;
        break;
      case SET_SERVER_NAME:
        set_server_name(actions[i].pvalue);
        modified = true;
        break;
      case SET_SERVER_PORT:
        if (set_server_port(actions[i].value))
        {
          puts("Set server port error");
          fclose(f);
          return 11;
        }
        modified = true;
      break;
      case SET_SERVER_KEY:
        if (set_server_key(actions[i].pvalue))
        {
          puts("Set server key error");
          fclose(f);
          return 12;
        }
        modified = true;
        break;
      case SET_ENCRYPTION_KEY:
        if (set_encryption_key(actions[i].pvalue))
        {
          puts("Set encryption key error");
          fclose(f);
          return 13;
        }
        modified = true;
        break;
      case ADD_MAC_MAPPING:
        if (add_mac_mapping(actions[i].value, (uint64_t)actions[i].pvalue))
        {
          puts("Add mac mapping error");
          fclose(f);
          return 14;
        }
        modified = true;
        break;
      case REMOVE_MAC_MAPPING:
        if (remove_mac_mapping(actions[i].value))
        {
          puts("Remove mac mapping error");
          fclose(f);
          return 15;
        }
        modified = true;
        break;
      case SHOW:
        show_config();
        break;
      case CREATE_DEVICE_FILE:
        if (create_device_file(actions[i].pvalue))
        {
          puts("Device file create error");
          fclose(f);
          return 16;
        }
        break;
      default:
        printf("Unknown action %d\n", actions[i].action);
        return 1;
    }
  }
  if (modified)
  {
    if (fseek(f, 0, SEEK_SET))
    {
      puts("File seek error");
      fclose(f);
      return 8;
    }
    size_t size = fwrite(&config, sizeof(config), 1, f);
    if (ferror(f) || size != 1)
    {
      puts("Failed to write config to file");
      fclose(f);
      return 9;
    }
  }
  fclose(f);
  return 0;
}
