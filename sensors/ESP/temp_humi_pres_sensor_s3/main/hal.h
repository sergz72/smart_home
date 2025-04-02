#ifndef MAIN_HAL_H
#define MAIN_HAL_H

esp_err_t i2c_master_init(void);
esp_err_t spi_master_init(void);
void gpio_init(void);
void uart1_init(void);
void adc_init(void);
unsigned int temt6000_get_mv(void);

#endif //MAIN_HAL_H
