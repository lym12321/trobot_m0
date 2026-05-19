//
// Created by fish on 2026/5/19.
//

#include "bsp/spi.h"

void bsp_spi_flush_rx(SPI_Regs *device) {
    while (!DL_SPI_isRXFIFOEmpty(device)) {
        (void)DL_SPI_receiveData8(device);
    }
}

uint8_t bsp_spi_transfer(SPI_Regs *device, uint8_t data) {
    DL_SPI_transmitDataBlocking8(device, data);
    return DL_SPI_receiveData8(device);
}

void bsp_spi_transmit(SPI_Regs *device, uint8_t data) {
    DL_SPI_transmitDataBlocking8(device, data);
    if (DL_SPI_isRXFIFOFull(device)) {
        bsp_spi_flush_rx(device);
    }
}

void bsp_spi_device_select(const bsp_spi_device_t *device) {
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_clearPins(device->cs_port, device->cs_pin);
}

void bsp_spi_device_deselect(const bsp_spi_device_t *device) {
    while (DL_SPI_isBusy(device->inst)) {
    }
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_setPins(device->cs_port, device->cs_pin);
}

uint8_t bsp_spi_device_transfer(const bsp_spi_device_t *device, uint8_t data) {
    return bsp_spi_transfer(device->inst, data);
}

void bsp_spi_device_transmit(const bsp_spi_device_t *device, uint8_t data) {
    bsp_spi_transmit(device->inst, data);
}
