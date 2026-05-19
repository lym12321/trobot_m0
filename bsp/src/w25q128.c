//
// Created by fish on 2026/5/19.
//

#include "bsp/w25q128.h"

#include "bsp/spi.h"
#include "bsp/time.h"

static const bsp_spi_device_t flash_device = {
    .inst = SPI1_INST,
    .cs_port = SPI_CS_PORT,
    .cs_pin = SPI_CS_FLASH_PIN,
};

static uint16_t read_id() {
    uint16_t id = 0;
    bsp_spi_device_select(&flash_device);
    bsp_spi_device_transfer(&flash_device, 0x90);
    bsp_spi_device_transfer(&flash_device, 0x00);
    bsp_spi_device_transfer(&flash_device, 0x00);
    bsp_spi_device_transfer(&flash_device, 0x00);
    id |= bsp_spi_device_transfer(&flash_device, 0xff) << 8;
    id |= bsp_spi_device_transfer(&flash_device, 0xff);
    bsp_spi_device_deselect(&flash_device);
    return id;
}

void w25q128_init() {
    bsp_spi_device_deselect(&flash_device);

    uint8_t cnt = 0;
    uint16_t id = read_id();
    while (id != 0xef17 && ++ cnt < 10) {
        bsp_time_delay(10);
        id = read_id();
    }
    BSP_ASSERT(id == 0xEF17);
}
