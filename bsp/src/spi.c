//
// Created by fish on 2026/5/19.
//

#include "bsp/spi.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stddef.h>

#define SPI_BUS_COUNT (sizeof(spi_buses) / sizeof(spi_buses[0]))

typedef struct {
    SPI_Regs *inst;
    StaticSemaphore_t mutex_buffer;
    SemaphoreHandle_t mutex;
    const bsp_spi_device_t *selected;
    uint32_t select_depth;
} spi_bus_t;

static spi_bus_t spi_buses[] = {
#ifdef SPI0_BASE
    { .inst = (SPI_Regs *)SPI0_BASE },
#endif
#ifdef SPI1_BASE
    { .inst = (SPI_Regs *)SPI1_BASE },
#endif
};

static uint8_t spi_scheduler_running(void) {
    return xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED;
}

static spi_bus_t *spi_bus_get(SPI_Regs *inst) {
    for (uint32_t i = 0; i < SPI_BUS_COUNT; i++) {
        if (spi_buses[i].inst == inst) {
            return &spi_buses[i];
        }
    }

    BSP_ASSERT(false);
    return &spi_buses[0];
}

static SemaphoreHandle_t spi_bus_mutex_get(spi_bus_t *bus) {
    taskENTER_CRITICAL();
    if (bus->mutex == NULL) {
        bus->mutex = xSemaphoreCreateRecursiveMutexStatic(&bus->mutex_buffer);
        BSP_ASSERT(bus->mutex != NULL);
    }
    taskEXIT_CRITICAL();

    return bus->mutex;
}

static void spi_bus_lock_if_running(spi_bus_t *bus) {
    if (!spi_scheduler_running()) {
        return;
    }

    BSP_ASSERT(xSemaphoreTakeRecursive(spi_bus_mutex_get(bus), portMAX_DELAY) == pdTRUE);
}

static void spi_bus_unlock_if_running(spi_bus_t *bus) {
    if (!spi_scheduler_running()) {
        return;
    }

    BSP_ASSERT(xSemaphoreGiveRecursive(spi_bus_mutex_get(bus)) == pdTRUE);
}

static void spi_wait_idle(SPI_Regs *inst) {
    while (DL_SPI_isBusy(inst)) {
    }
}

static uint8_t spi_is_same_device(const bsp_spi_device_t *a, const bsp_spi_device_t *b) {
    return a->inst == b->inst && a->cs_port == b->cs_port && a->cs_pin == b->cs_pin;
}

static void spi_device_assert_cs(const bsp_spi_device_t *device) {
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_clearPins(device->cs_port, device->cs_pin);
}

static void spi_device_release_cs(const bsp_spi_device_t *device) {
    spi_wait_idle(device->inst);
    bsp_spi_flush_rx(device->inst);
    DL_GPIO_setPins(device->cs_port, device->cs_pin);
}

void bsp_spi_lock(SPI_Regs *inst) {
    spi_bus_lock_if_running(spi_bus_get(inst));
}

void bsp_spi_unlock(SPI_Regs *inst) {
    spi_bus_unlock_if_running(spi_bus_get(inst));
}

void bsp_spi_flush_rx(SPI_Regs *inst) {
    while (!DL_SPI_isRXFIFOEmpty(inst)) {
        (void)DL_SPI_receiveData8(inst);
    }
}

uint8_t bsp_spi_transfer(SPI_Regs *inst, uint8_t data) {
    DL_SPI_transmitDataBlocking8(inst, data);
    return DL_SPI_receiveData8(inst);
}

void bsp_spi_transmit(SPI_Regs *inst, uint8_t data) {
    DL_SPI_transmitDataBlocking8(inst, data);
    if (DL_SPI_isRXFIFOFull(inst)) {
        bsp_spi_flush_rx(inst);
    }
}

void bsp_spi_write(SPI_Regs *inst, const uint8_t *data, uint32_t len) {
    if (data == NULL) {
        return;
    }

    bsp_spi_transfer_buf(inst, data, NULL, len, 0xff);
}

void bsp_spi_read(SPI_Regs *inst, uint8_t *data, uint32_t len, uint8_t dummy) {
    if (data == NULL) {
        return;
    }

    bsp_spi_transfer_buf(inst, NULL, data, len, dummy);
}

void bsp_spi_transfer_buf(SPI_Regs *inst, const uint8_t *tx, uint8_t *rx, uint32_t len, uint8_t dummy) {
    while (len--) {
        uint8_t out = (tx == NULL) ? dummy : *tx++;
        uint8_t in = bsp_spi_transfer(inst, out);
        if (rx != NULL) {
            *rx++ = in;
        }
    }
}

void bsp_spi_device_select(const bsp_spi_device_t *device) {
    spi_bus_t *bus = spi_bus_get(device->inst);
    spi_bus_lock_if_running(bus);

    BSP_ASSERT(bus->selected == NULL || spi_is_same_device(bus->selected, device));
    if (bus->select_depth == 0) {
        bus->selected = device;
        spi_device_assert_cs(device);
    }
    bus->select_depth++;
}

void bsp_spi_device_deselect(const bsp_spi_device_t *device) {
    spi_bus_t *bus = spi_bus_get(device->inst);

    if (bus->select_depth == 0) {
        spi_device_release_cs(device);
        return;
    }

    BSP_ASSERT(bus->selected != NULL && spi_is_same_device(bus->selected, device));
    bus->select_depth--;
    if (bus->select_depth == 0) {
        spi_device_release_cs(device);
        bus->selected = NULL;
    }

    spi_bus_unlock_if_running(bus);
}

uint8_t bsp_spi_device_transfer(const bsp_spi_device_t *device, uint8_t data) {
    return bsp_spi_transfer(device->inst, data);
}

void bsp_spi_device_transmit(const bsp_spi_device_t *device, uint8_t data) {
    bsp_spi_transmit(device->inst, data);
}

void bsp_spi_device_write(const bsp_spi_device_t *device, const uint8_t *data, uint32_t len) {
    bsp_spi_write(device->inst, data, len);
}

void bsp_spi_device_read(const bsp_spi_device_t *device, uint8_t *data, uint32_t len, uint8_t dummy) {
    bsp_spi_read(device->inst, data, len, dummy);
}

void bsp_spi_device_transfer_buf(const bsp_spi_device_t *device, const uint8_t *tx, uint8_t *rx, uint32_t len, uint8_t dummy) {
    bsp_spi_transfer_buf(device->inst, tx, rx, len, dummy);
}
