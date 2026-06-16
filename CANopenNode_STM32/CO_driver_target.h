/*
 * Device and application specific definitions for CANopenNode.
 *
 * @file        CO_driver_target.h
 * @author      Hamed Jafarzadeh 	2022
 * 				Tilen Marjerle		2021
 * 				Janez Paternoster	2020
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CO_DRIVER_TARGET_H
#define CO_DRIVER_TARGET_H

/* This file contains device and application specific definitions.
 * It is included from CO_driver.h, which contains documentation
 * for common definitions below. */

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Determining the CANOpen Driver

#if defined(FDCAN) || defined(FDCAN1) || defined(FDCAN2) || defined(FDCAN3)
#define CO_STM32_FDCAN_Driver 1
#elif defined(CAN) || defined(CAN1) || defined(CAN2) || defined(CAN3)
#define CO_STM32_CAN_Driver 1
#else
#error This STM32 Do not support CAN or FDCAN
#endif

#undef CO_CONFIG_STORAGE_ENABLE // We don't need Storage option, implement based on your use case and remove this line from here

#ifdef CO_DRIVER_CUSTOM
#include "CO_driver_custom.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Stack configuration override default values.
 * For more information see file CO_config.h. */
/* NOTE: CO_CONFIG_HB_CONS (and other CO_CONFIG_* overrides) are defined
 * via compile flags in CMakeLists.txt — do not duplicate them here. */

/* Basic definitions. If big endian, CO_SWAP_xx macros must swap bytes. */
#define CO_LITTLE_ENDIAN
#define CO_SWAP_16(x) x
#define CO_SWAP_32(x) x
#define CO_SWAP_64(x) x

/* NULL is defined in stddef.h */
/* true and false are defined in stdbool.h */
/* int8_t to uint64_t are defined in stdint.h */
typedef uint_fast8_t bool_t;
typedef float float32_t;
typedef double float64_t;

/**
 * \brief           CAN RX message for platform
 *
 * This is platform specific one
 */
typedef struct {
    uint32_t ident;  /*!< Standard identifier */
    uint8_t dlc;     /*!< Data length */
    uint8_t data[8]; /*!< Received data */
} CO_CANrxMsg_t;

/* Access to received CAN message */
#define CO_CANrxMsg_readIdent(msg) ((uint16_t)(((CO_CANrxMsg_t*)(msg)))->ident)
#define CO_CANrxMsg_readDLC(msg)   ((uint8_t)(((CO_CANrxMsg_t*)(msg)))->dlc)
#define CO_CANrxMsg_readData(msg)  ((uint8_t*)(((CO_CANrxMsg_t*)(msg)))->data)

/* Received message object */
typedef struct {
    uint16_t ident;
    uint16_t mask;
    void* object;
    void (*CANrx_callback)(void* object, void* message);
} CO_CANrx_t;

/* Transmit message object */
typedef struct {
    uint32_t ident;
    uint8_t DLC;
    uint8_t data[8];
    volatile bool_t bufferFull;
    volatile bool_t syncFlag;
} CO_CANtx_t;

/* CAN module object */
typedef struct {
    void* CANptr;
    CO_CANrx_t* rxArray;
    uint16_t rxSize;
    CO_CANtx_t* txArray;
    uint16_t txSize;
    uint16_t CANerrorStatus;
    volatile bool_t CANnormal;
    volatile bool_t useCANrxFilters;
    volatile bool_t bufferInhibitFlag;
    volatile bool_t firstCANtxMessage;
    volatile uint16_t CANtxCount;
    uint32_t errOld;

    /* STM32 specific features */
    uint32_t primask_send; /* Primask register for interrupts for send operation */
    uint32_t primask_emcy; /* Primask register for interrupts for emergency operation */
    uint32_t primask_od;   /* Primask register for interrupts for send operation */

} CO_CANmodule_t;

/* Data storage object for one entry */
typedef struct {
    void* addr;
    size_t len;
    uint8_t subIndexOD;
    uint8_t attr;
    /* Additional variables (target specific) */
    void* addrNV;
} CO_storage_entry_t;

/*
 * Critical-section locks: BASEPRI, NOT PRIMASK.
 *
 * These were originally __disable_irq()/__set_PRIMASK() (mask EVERYTHING). That
 * defeated the watchdog stall supervisor: TIM7 runs at NVIC priority 0 to catch
 * a scheduler-wide hang and record a pmTypeStall before the IWDG fires, but a
 * full PRIMASK disable masks even priority 0 — so any hang while one of these
 * locks was held produced a BLIND IWDG with no forensics (confirmed from bag
 * data 2026-06-16: genuine IWDG, zero post-mortem ever emitted).
 *
 * Raising BASEPRI to the FreeRTOS syscall ceiling instead masks every interrupt
 * at priority >= 5 (CAN RX/TX/SCE, UART, TIM3 — all the contexts that actually
 * touch the CAN module / OD, all at priority 5), so the mutual exclusion these
 * locks provide is fully preserved. Only priorities 0..4 still fire — i.e. the
 * TIM7 supervisor, which touches neither the CAN module nor the OD and so cannot
 * corrupt anything. The stored value is the previous BASEPRI (field names kept
 * for ABI/diff stability; they now hold BASEPRI, not PRIMASK). This is the same
 * mechanism FreeRTOS itself uses for taskENTER_CRITICAL on Cortex-M.
 *
 * CO_DRIVER_BASEPRI_MASK must equal FreeRTOSConfig.h's
 * configMAX_SYSCALL_INTERRUPT_PRIORITY: configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)
 * shifted into the upper configPRIO_BITS (4) bits of an 8-bit priority field.
 */
#define CO_DRIVER_BASEPRI_MASK ((uint32_t)(5U << (8U - 4U)))

#define CO_DRIVER_RAISE_BASEPRI(SAVE_FIELD)                                                                            \
    do {                                                                                                               \
        (SAVE_FIELD) = __get_BASEPRI();                                                                                \
        __set_BASEPRI(CO_DRIVER_BASEPRI_MASK);                                                                         \
        __DMB();                                                                                                       \
    } while (0)

/* (un)lock critical section in CO_CANsend() */
#define CO_LOCK_CAN_SEND(CAN_MODULE) CO_DRIVER_RAISE_BASEPRI((CAN_MODULE)->primask_send)
#define CO_UNLOCK_CAN_SEND(CAN_MODULE) __set_BASEPRI((CAN_MODULE)->primask_send)

/* (un)lock critical section in CO_errorReport() or CO_errorReset() */
#define CO_LOCK_EMCY(CAN_MODULE) CO_DRIVER_RAISE_BASEPRI((CAN_MODULE)->primask_emcy)
#define CO_UNLOCK_EMCY(CAN_MODULE) __set_BASEPRI((CAN_MODULE)->primask_emcy)

/* (un)lock critical section when accessing Object Dictionary */
#define CO_LOCK_OD(CAN_MODULE) CO_DRIVER_RAISE_BASEPRI((CAN_MODULE)->primask_od)
#define CO_UNLOCK_OD(CAN_MODULE) __set_BASEPRI((CAN_MODULE)->primask_od)

/* Synchronization between CAN receive and message processing threads. */
#define CO_MemoryBarrier()
#define CO_FLAG_READ(rxNew) ((rxNew) != NULL)
#define CO_FLAG_SET(rxNew)                                                                                             \
    do {                                                                                                               \
        CO_MemoryBarrier();                                                                                            \
        rxNew = (void*)1L;                                                                                             \
    } while (0)
#define CO_FLAG_CLEAR(rxNew)                                                                                           \
    do {                                                                                                               \
        CO_MemoryBarrier();                                                                                            \
        rxNew = NULL;                                                                                                  \
    } while (0)


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_DRIVER_TARGET_H */
