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

// spi read and write
void bsp_spi_flush_rx(SPI_Regs *device);
uint8_t bsp_spi_transfer(SPI_Regs *device, uint8_t data);
void bsp_spi_transmit(SPI_Regs *device, uint8_t data);

void bsp_spi_device_select(const bsp_spi_device_t *device);
void bsp_spi_device_deselect(const bsp_spi_device_t *device);
uint8_t bsp_spi_device_transfer(const bsp_spi_device_t *device, uint8_t data);
void bsp_spi_device_transmit(const bsp_spi_device_t *device, uint8_t data);
