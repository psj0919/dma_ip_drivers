/**
 * @file pcie_elbi.h
 * @brief GTX PCIe ELBI (External Local Bus Interface) Driver Header
 *        Device-side interrupt handling for Host-Device communication
 *
 * Uses common register definitions from riscv/pcie-elbi-regs.hpp
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ## PCIe Interrupt Architecture (Synopsys DesignWare)
 *
 * ### Host -> Device Interrupt (ELBI Doorbell)
 *
 * The ELBI (External Local Bus Interface) provides a vendor-specific
 * doorbell mechanism for Host-to-Device interrupts.
 *
 * ```
 * Host CPU                    PCIe Link           GTX NPU (Device)
 * --------                    ---------           -----------------
 * Write BAR0+0x0e40   -->   Memory Write TLP  -->   ELBI Register Latch
 * (set bit[0]=1)                                         |
 *                                                    OR gate -> IRQ line
 *                                                        |
 *                                                    RISC-V Core (trap)
 *                                                        |
 *                                                    Read status (0x01)
 *                                                        |
 *                                                    handler()
 *                                                        |
 *                                                    Write clear (0x01)
 * ```
 *
 * Register map:
 * - ELBI_DOORBELL_STATUS (0x0E40): Host writes here, Device reads pending bits
 * - ELBI_DOORBELL_MASK   (0x0E44): 1=masked (disabled), 0=enabled
 * - ELBI_DOORBELL_CLEAR  (0x0E48): Write 1 to clear corresponding bit
 *
 * ### Device -> Host Interrupt (PCIe MSI)
 *
 * See pcie_msi.h for MSI/MSI-X implementation details.
 * Quick method: Write to DBI + 0x20_0d70 (MSI_GEN register)
 *
 * ### Linux DW PCIe Reference
 *
 * This implementation is based on analysis of:
 * - pcie-designware.h: struct dw_pcie.elbi_base
 * - pcie-designware-ep.c: dw_pcie_ep_raise_msi_irq()
 *
 * Note: ELBI is NOT part of the standard Linux DW PCIe driver.
 * It's a vendor extension that must be implemented per-platform.
 *
 * ### PCIE_CFG Register Offsets (Base: 0x40A0_0000)
 * - 0x0330: Interrupt Flag (Raw Status) - Write 1 to clear
 * - 0x033c: Interrupt Mask - Set bits to mask interrupts
 * - 0x0340: Masked Interrupt Status - Read only
 */

#ifndef GTX_PCIE_ELBI_H
#define GTX_PCIE_ELBI_H

#include <stdint.h>
#include <stdbool.h>

/* Common ELBI register definitions */
#include "drivers/pcie-elbi-regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Types
 *============================================================================*/

/**
 * @brief Interrupt handler function pointer type
 * @param source Interrupt source number (0-31)
 * @param user_data User-provided context pointer
 */
typedef void (*gtx_elbi_handler_t)(uint32_t source, void *user_data);

/*=============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Initialize ELBI controller
 *
 * Masks all interrupts and clears any pending interrupts.
 */
void gtx_elbi_init(void);

/**
 * @brief Check if ELBI is initialized
 * @return true if initialized
 */
bool gtx_elbi_is_initialized(void);

/*=============================================================================
 * Interrupt Handler Registration
 *============================================================================*/

/**
 * @brief Register interrupt handler
 * @param handler Handler function called for each interrupt source
 * @param user_data Optional user data passed to handler
 */
void gtx_elbi_register_handler(gtx_elbi_handler_t handler, void *user_data);

/**
 * @brief Unregister interrupt handler
 */
void gtx_elbi_unregister_handler(void);

/*=============================================================================
 * Interrupt Masking Functions
 *============================================================================*/

/**
 * @brief Mask all interrupt sources
 */
void gtx_elbi_mask_all(void);

/**
 * @brief Unmask all interrupt sources
 */
void gtx_elbi_unmask_all(void);

/**
 * @brief Mask specific interrupt source
 * @param source Interrupt source number (0-31)
 */
void gtx_elbi_mask(uint32_t source);

/**
 * @brief Unmask specific interrupt source
 * @param source Interrupt source number (0-31)
 */
void gtx_elbi_unmask(uint32_t source);

/**
 * @brief Set interrupt mask register directly
 * @param mask Mask value (1 = masked, 0 = enabled)
 */
void gtx_elbi_set_mask(uint32_t mask);

/**
 * @brief Get current interrupt mask
 * @return Current mask value
 */
uint32_t gtx_elbi_get_mask(void);

/*=============================================================================
 * Interrupt Status Functions
 *============================================================================*/

/**
 * @brief Get raw interrupt flag (before masking)
 * @return Raw interrupt flag register value
 */
uint32_t gtx_elbi_get_flag(void);

/**
 * @brief Get masked interrupt status (pending interrupts)
 * @return Masked interrupt status (FLAG & ~MASK)
 */
uint32_t gtx_elbi_get_status(void);

/**
 * @brief Check if specific interrupt source is pending
 * @param source Interrupt source number (0-31)
 * @return true if interrupt is pending
 */
bool gtx_elbi_is_pending(uint32_t source);

/*=============================================================================
 * Interrupt Clearing Functions
 *============================================================================*/

/**
 * @brief Clear specific interrupt source
 * @param source Interrupt source number (0-31)
 */
void gtx_elbi_clear(uint32_t source);

/**
 * @brief Clear multiple interrupt sources
 * @param sources Bitmask of sources to clear
 */
void gtx_elbi_clear_multiple(uint32_t sources);

/**
 * @brief Clear all pending interrupts
 */
void gtx_elbi_clear_all(void);

/*=============================================================================
 * Interrupt Handling Functions
 *============================================================================*/

/**
 * @brief Handle pending interrupts
 *
 * This function should be called from the PCIe interrupt handler.
 * It reads the masked interrupt status, calls the registered handler
 * for each pending source, and clears the handled interrupts.
 *
 * @return Number of interrupts handled
 */
uint32_t gtx_elbi_handle_interrupt(void);

/*=============================================================================
 * MSI Generation Functions (Device -> Host)
 *============================================================================*/

/**
 * @brief Send MSI to host
 * @param vector MSI vector number (0-31)
 *
 * Writes to PCIE_DBI offset 0x20_0d70 to generate MSI.
 * LSB (vector 0) has highest priority.
 */
void gtx_elbi_send_msi(uint32_t vector);

/**
 * @brief Send multiple MSIs to host
 * @param vectors Bitmask of vectors to send
 */
void gtx_elbi_send_msi_multiple(uint32_t vectors);

/*=============================================================================
 * Debug Functions
 *============================================================================*/

/**
 * @brief Print interrupt status (for debugging)
 *
 * Prints flag, mask, and effective status registers.
 */
void gtx_elbi_print_status(void);

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_ELBI_H */
