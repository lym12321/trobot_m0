//
// Created by fish on 2026/5/19.
//

#include "bsp/spi.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static StaticSemaphore_t spi_bus_mutex_buffer;
static SemaphoreHandle_t spi_bus_mutex = NULL;
static uint8_t spi_selected_count = 0;

static void bsp_spi_bus_mutex_init() {
    if (spi_bus_mutex != NULL) {
        return;
    }

    taskENTER_CRITICAL();
    if (spi_bus_mutex == NULL) {
        spi_bus_mutex = xSemaphoreCreateRecursiveMutexStatic(&spi_bus_mutex_buffer);
        BSP_ASSERT(spi_bus_mutex != NULL);
    }
    taskEXIT_CRITICAL();
}

void bsp_spi_bus_lock() {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    bsp_spi_bus_mutex_init();
    BSP_ASSERT(xSemaphoreTakeRecursive(spi_bus_mutex, portMAX_DELAY) == pdTRUE);
}

void bsp_spi_bus_unlock() {
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    bsp_spi_bus_mutex_init();
    BSP_ASSERT(xSemaphoreGiveRecursive(spi_bus_mutex) == pdTRUE);
}

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
    bsp_spi_bus_lock();
    spi_selected_count++;
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_clearPins(device->cs_port, device->cs_pin);
}

void bsp_spi_device_deselect(const bsp_spi_device_t *device) {
    while (DL_SPI_isBusy(device->inst)) {
    }
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_setPins(device->cs_port, device->cs_pin);
    if (spi_selected_count > 0) {
        spi_selected_count--;
        bsp_spi_bus_unlock();
    }
}

uint8_t bsp_spi_device_transfer(const bsp_spi_device_t *device, uint8_t data) {
    return bsp_spi_transfer(device->inst, data);
}

void bsp_spi_device_transmit(const bsp_spi_device_t *device, uint8_t data) {
    bsp_spi_transmit(device->inst, data);
}
