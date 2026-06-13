//
// Created by fish on 2026/5/19.
//

#pragma once

#include "bsp/def.h"

typedef struct {
    SPI_Regs *inst;
    GPIO_Regs *cs_port;
    uint32_t cs_pin;
} bsp_spi_device_t;

/* Lock a SPI bus across multi-step transactions. Device select/deselect already
 * calls these, so most single-device transactions do not need manual use.
 */
void bsp_spi_lock(SPI_Regs *inst);
void bsp_spi_unlock(SPI_Regs *inst);

// spi read and write
void bsp_spi_flush_rx(SPI_Regs *inst);
uint8_t bsp_spi_transfer(SPI_Regs *inst, uint8_t data);
void bsp_spi_transmit(SPI_Regs *inst, uint8_t data);
void bsp_spi_write(SPI_Regs *inst, const uint8_t *data, uint32_t len);
void bsp_spi_read(SPI_Regs *inst, uint8_t *data, uint32_t len, uint8_t dummy);
void bsp_spi_transfer_buf(SPI_Regs *inst, const uint8_t *tx, uint8_t *rx, uint32_t len, uint8_t dummy);

void bsp_spi_device_select(const bsp_spi_device_t *device);
void bsp_spi_device_deselect(const bsp_spi_device_t *device);
uint8_t bsp_spi_device_transfer(const bsp_spi_device_t *device, uint8_t data);
void bsp_spi_device_transmit(const bsp_spi_device_t *device, uint8_t data);
void bsp_spi_device_write(const bsp_spi_device_t *device, const uint8_t *data, uint32_t len);
void bsp_spi_device_read(const bsp_spi_device_t *device, uint8_t *data, uint32_t len, uint8_t dummy);
void bsp_spi_device_transfer_buf(const bsp_spi_device_t *device, const uint8_t *tx, uint8_t *rx, uint32_t len, uint8_t dummy);
