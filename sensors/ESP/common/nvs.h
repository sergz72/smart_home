//
// Created by sergi on 04.09.2022.
//

#ifndef TEMP_HUMI_PRES_SENSOR_NVS_H
#define TEMP_HUMI_PRES_SENSOR_NVS_H

#ifdef __cplusplus
extern "C" {
#endif

void nvs_init(void);
void nvs_set_wifi_name(void);
void nvs_set_wifi_pwd(void);
void nvs_set_server_parameters(void);

#ifdef __cplusplus
}
#endif

#endif //TEMP_HUMI_PRES_SENSOR_NVS_H
