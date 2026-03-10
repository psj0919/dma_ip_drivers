/**
 * @file pcie_irq.h
 * @brief GTX PCIe IRQ Handler Integration
 *        Unified interrupt handling for PCIe communication
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This module integrates:
 * - ELBI Doorbell (Host -> Device interrupts)
 * - MSI (Device -> Host interrupts)
 * - Message queue processing
 *
 * ## Usage Example
 *
 * ```c
 * #include "pcie_irq.h"
 *
 * // Define handlers
 * void on_cmd_ready(void *ctx) {
 *     // Process command from Host
 *     gtx_pcie_irq_send_msi(GTX_MSI_VEC_CMD_ACK);
 * }
 *
 * void on_dma_done(void *ctx) {
 *     // DMA transfer complete, start processing
 * }
 *
 * int main(void) {
 *     // Initialize PCIe and IRQ
 *     gtx_pcie_irq_init();
 *
 *     // Register handlers
 *     gtx_pcie_irq_register(GTX_PCIE_IRQ_CMD_READY, on_cmd_ready, NULL);
 *     gtx_pcie_irq_register(GTX_PCIE_IRQ_DMA_DONE, on_dma_done, NULL);
 *
 *     // Enable interrupts
 *     gtx_pcie_irq_enable_all();
 *
 *     // Main loop
 *     while (1) {
 *         __asm__ volatile ("wfi");  // Wait for interrupt
 *     }
 * }
 * ```
 */

#ifndef GTX_PCIE_IRQ_H
#define GTX_PCIE_IRQ_H

#include <stdint.h>
#include <stdbool.h>

/* MSI vector macros (GTX_PCIE_MSI_*) */
#include "drivers/pcie-elbi-regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * IRQ Source Definitions (Host -> Device via ELBI)
 *============================================================================*/

typedef enum {
    GTX_PCIE_IRQ_CMD_READY       = 0,   /* New command from Host */
    GTX_PCIE_IRQ_DMA_DONE        = 1,   /* Host DMA complete */
    GTX_PCIE_IRQ_CONFIG_UPDATE   = 2,   /* Configuration changed */
    GTX_PCIE_IRQ_ABORT           = 3,   /* Abort request */
    GTX_PCIE_IRQ_SYNC            = 4,   /* Sync/barrier */
    GTX_PCIE_IRQ_MSG_AVAILABLE   = 5,   /* Message in queue */
    GTX_PCIE_IRQ_DEBUG           = 6,   /* Debug request */
    GTX_PCIE_IRQ_RESERVED_7      = 7,
    /* 8-31: Application defined */
    GTX_PCIE_IRQ_MAX             = 32
} gtx_pcie_irq_source_t;

/*=============================================================================
 * MSI Vector Definitions (Device -> Host)
 *
 * Note: Use macros from pcie-elbi-regs.hpp for vector numbers:
 *   GTX_PCIE_MSI_CMD_ACK, GTX_PCIE_MSI_INFER_DONE, etc.
 *============================================================================*/

typedef uint32_t gtx_pcie_msi_vector_t;
#define GTX_PCIE_MSI_MAX  32

/*=============================================================================
 * Types
 *============================================================================*/

/**
 * @brief IRQ handler callback
 * @param context User-provided context
 */
typedef void (*gtx_pcie_irq_handler_t)(void *context);

/*=============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize PCIe IRQ subsystem
 *
 * Initializes:
 * - PCIe controller (if not already initialized)
 * - ELBI doorbell
 * - MSI subsystem
 * - RISC-V external interrupt
 *
 * @return 0 on success, negative on error
 */
int gtx_pcie_irq_init(void);

/**
 * @brief Check if IRQ subsystem is initialized
 * @return true if initialized
 */
bool gtx_pcie_irq_is_initialized(void);

/*=============================================================================
 * IRQ Handler Registration (Host -> Device)
 *============================================================================*/

/**
 * @brief Register handler for specific IRQ source
 * @param source IRQ source number
 * @param handler Handler function
 * @param context User context passed to handler
 * @return 0 on success, -1 on error
 */
int gtx_pcie_irq_register(gtx_pcie_irq_source_t source,
                               gtx_pcie_irq_handler_t handler,
                               void *context);

/**
 * @brief Unregister handler for IRQ source
 * @param source IRQ source number
 */
void gtx_pcie_irq_unregister(gtx_pcie_irq_source_t source);

/*=============================================================================
 * IRQ Enable/Disable
 *============================================================================*/

/**
 * @brief Enable specific IRQ source
 * @param source IRQ source number
 */
void gtx_pcie_irq_enable(gtx_pcie_irq_source_t source);

/**
 * @brief Disable specific IRQ source
 * @param source IRQ source number
 */
void gtx_pcie_irq_disable(gtx_pcie_irq_source_t source);

/**
 * @brief Enable all registered IRQ sources
 */
void gtx_pcie_irq_enable_all(void);

/**
 * @brief Disable all IRQ sources
 */
void gtx_pcie_irq_disable_all(void);

/*=============================================================================
 * IRQ Handling
 *============================================================================*/

/**
 * @brief Handle pending PCIe interrupts
 *
 * Called from RISC-V external interrupt handler.
 * Processes all pending ELBI doorbell interrupts.
 *
 * @return Number of interrupts handled
 */
uint32_t gtx_pcie_irq_handle(void);

/**
 * @brief Poll for and handle interrupts (non-blocking)
 * @return Number of interrupts handled
 */
uint32_t gtx_pcie_irq_poll(void);

/*=============================================================================
 * MSI Send Functions (Device -> Host)
 *============================================================================*/

/**
 * @brief Send MSI to Host
 * @param vector MSI vector number
 * @return 0 on success, negative on error
 */
int gtx_pcie_irq_send_msi(gtx_pcie_msi_vector_t vector);

/**
 * @brief Send multiple MSIs to Host
 * @param vectors Bitmask of vectors
 * @return 0 on success, negative on error
 */
int gtx_pcie_irq_send_msi_multiple(uint32_t vectors);

/*=============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Notify Host that inference is complete
 */
static inline void gtx_pcie_notify_infer_done(void) {
    gtx_pcie_irq_send_msi(GTX_PCIE_MSI_INFER_DONE);
}

/**
 * @brief Notify Host of an error
 */
static inline void gtx_pcie_notify_error(void) {
    gtx_pcie_irq_send_msi(GTX_PCIE_MSI_ERROR);
}

/**
 * @brief Notify Host that command is acknowledged
 */
static inline void gtx_pcie_notify_cmd_ack(void) {
    gtx_pcie_irq_send_msi(GTX_PCIE_MSI_CMD_ACK);
}

/**
 * @brief Notify Host that DMA is complete
 */
static inline void gtx_pcie_notify_dma_complete(void) {
    gtx_pcie_irq_send_msi(GTX_PCIE_MSI_DMA_COMPLETE);
}

/*=============================================================================
 * RISC-V Interrupt Setup
 *============================================================================*/

/**
 * @brief Enable RISC-V machine external interrupt
 *
 * Sets up mie and mstatus CSRs for external interrupt handling.
 */
void gtx_pcie_irq_enable_riscv_external(void);

/**
 * @brief Disable RISC-V machine external interrupt
 */
void gtx_pcie_irq_disable_riscv_external(void);

/*=============================================================================
 * Debug
 *============================================================================*/

/**
 * @brief Print IRQ status
 */
void gtx_pcie_irq_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_IRQ_H */
