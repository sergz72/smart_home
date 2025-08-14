#ifndef MAIN_HAL_H
#define MAIN_HAL_H

esp_err_t i2c_master_init(void);
esp_err_t spi_master_init(void);
void gpio_init(void);

#endif //MAIN_HAL_H
