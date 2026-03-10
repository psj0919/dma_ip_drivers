/**
 * @file pcie-elbi-regs.h
 * @brief GTX PCIe ELBI Register Definitions (C version)
 *
 * ELBI (External Local Bus Interface) provides Host-to-Device interrupt
 * mechanism via doorbell registers.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GTX_PCIE_ELBI_REGS_H
#define GTX_PCIE_ELBI_REGS_H

#include <stdint.h>
#include "pcie-regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * ELBI Register Offsets
 *============================================================================*/

/* BAR0 ELBI registers (offset from SLAVE_BASE when accessed locally) */
#define GTX_ELBI_BAR0_INT_GEN     0x0E40U  /* Host writes here to trigger doorbell */

/* PCIE_CFG interrupt registers (offset from CFG_BASE) */
#define GTX_ELBI_CFG_BASE         GTX_PCIE_CFG_BASE
#define GTX_ELBI_CFG_INT_FLAG     0x0330U  /* Raw status, write 1 to clear */
#define GTX_ELBI_CFG_INT_MASK     0x033CU  /* Set bit to mask interrupt */
#define GTX_ELBI_CFG_INT_STATUS   0x0340U  /* FLAG & ~MASK (read-only) */

/* PCIE_DBI MSI generation register */
#define GTX_ELBI_DBI_MSI_GEN      0x200D70U  /* Write to generate MSI */

/*=============================================================================
 * Constants
 *============================================================================*/

#define GTX_ELBI_NUM_INT_SOURCES  32U  /* Host -> Device interrupt sources */
#define GTX_ELBI_NUM_MSI_VECTORS  32U  /* Device -> Host MSI vectors */

/*=============================================================================
 * Doorbell Source Definitions (Host -> Device)
 *============================================================================*/

#define GTX_DOORBELL_CMD_READY         0U   /* Command ready in message queue */
#define GTX_DOORBELL_DMA_DONE          1U   /* DMA transfer complete */
#define GTX_DOORBELL_CONFIG_UPDATE     2U   /* Configuration updated */
#define GTX_DOORBELL_ABORT             3U   /* Abort current operation */
#define GTX_DOORBELL_SYNC              4U   /* Synchronization request */
#define GTX_DOORBELL_MSG_AVAILABLE     5U   /* Message available */
/* 6-31: Reserved / User-defined */

/*=============================================================================
 * MSI Vector Definitions (Device -> Host)
 *============================================================================*/

#define GTX_PCIE_MSI_CMD_ACK       0U   /* Command acknowledged */
#define GTX_PCIE_MSI_INFER_DONE    1U   /* Inference complete */
#define GTX_PCIE_MSI_ERROR         2U   /* Error occurred */
#define GTX_PCIE_MSI_DMA_COMPLETE  3U   /* DMA transfer complete */
#define GTX_PCIE_MSI_MSG_SENT      4U   /* Message sent */
#define GTX_PCIE_MSI_STATUS_CHANGE 5U   /* Device status changed */
#define GTX_PCIE_MSI_HEARTBEAT     6U   /* Periodic heartbeat */
#define GTX_PCIE_MSI_ABORT_ACK     7U   /* Abort acknowledged */
/* 8-31: Reserved / User-defined */

/*=============================================================================
 * Helper Macros
 *============================================================================*/

/** Get bit mask for a doorbell source */
#define GTX_DOORBELL_MASK(source)    (1U << (source))

/** Get bit mask for an MSI vector */
#define GTX_MSI_VECTOR_MASK(vector)  (1U << (vector))

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_ELBI_REGS_H */
