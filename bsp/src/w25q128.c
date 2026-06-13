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
    const uint8_t cmd[] = {0x90, 0x00, 0x00, 0x00};
    uint8_t id[2] = {0};

    bsp_spi_device_select(&flash_device);
    bsp_spi_device_write(&flash_device, cmd, sizeof(cmd));
    bsp_spi_device_read(&flash_device, id, sizeof(id), 0xff);
    bsp_spi_device_deselect(&flash_device);

    return ((uint16_t)id[0] << 8) | id[1];
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
