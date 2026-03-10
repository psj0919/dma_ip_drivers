/**
 * @file pcie-regs.h
 * @brief GTX PCIe Register Definitions (Synopsys DesignWare C417-0, Gen4)
 *
 * Pure C header for PCIe registers.
 * All register addresses and bit definitions are defined here.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GTX_PCIE_REGS_H
#define GTX_PCIE_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * PCIe Base Addresses
 *============================================================================*/

#define GTX_PCIE_DBI_BASE       0x40000000UL    /* DBI registers */
#define GTX_PCIE_DBI_SIZE       0x00800000UL    /* 8MB */
#define GTX_PCIE_PHY_BASE       0x40800000UL    /* PHY registers */
#define GTX_PCIE_PHY_SIZE       0x00200000UL    /* 2MB */
#define GTX_PCIE_CFG_BASE       0x40A00000UL    /* Configuration space */
#define GTX_PCIE_CFG_SIZE       0x00010000UL    /* 64KB */
#define GTX_PCIE_SLAVE_BASE     0x60000000UL    /* PCIe slave (host access) */
#define GTX_PCIE_SLAVE_SIZE     0x10000000UL    /* 256MB */

/*=============================================================================
 * PCI Configuration Space Registers
 *============================================================================*/

#define GTX_PCI_VENDOR_ID       0x00U
#define GTX_PCI_DEVICE_ID       0x02U
#define GTX_PCI_COMMAND         0x04U
#define GTX_PCI_STATUS          0x06U
#define GTX_PCI_REVISION_ID     0x08U
#define GTX_PCI_CLASS_PROG      0x09U
#define GTX_PCI_CLASS_DEVICE    0x0AU
#define GTX_PCI_CACHE_LINE      0x0CU
#define GTX_PCI_LATENCY         0x0DU
#define GTX_PCI_HEADER_TYPE     0x0EU
#define GTX_PCI_BIST            0x0FU
#define GTX_PCI_BAR0            0x10U
#define GTX_PCI_BAR1            0x14U
#define GTX_PCI_BAR2            0x18U
#define GTX_PCI_BAR3            0x1CU
#define GTX_PCI_BAR4            0x20U
#define GTX_PCI_BAR5            0x24U
#define GTX_PCI_CAP_PTR         0x34U
#define GTX_PCI_INT_LINE        0x3CU
#define GTX_PCI_INT_PIN         0x3DU

/* Command register bits */
#define GTX_PCI_CMD_IO_EN       (1U << 0)
#define GTX_PCI_CMD_MEM_EN      (1U << 1)
#define GTX_PCI_CMD_BUS_MASTER  (1U << 2)
#define GTX_PCI_CMD_INTX_DIS    (1U << 10)

/* Status register bits */
#define GTX_PCI_STATUS_CAP_LIST (1U << 4)

/* Capability IDs */
#define GTX_PCI_CAP_ID_PM       0x01U
#define GTX_PCI_CAP_ID_MSI      0x05U
#define GTX_PCI_CAP_ID_PCIE     0x10U
#define GTX_PCI_CAP_ID_MSIX     0x11U

/*=============================================================================
 * PCIe Capability Registers (offsets from cap base)
 *============================================================================*/

#define GTX_PCIE_CAP_LINK_CAP    0x0CU
#define GTX_PCIE_CAP_LINK_CTRL   0x10U
#define GTX_PCIE_CAP_LINK_STATUS 0x12U
#define GTX_PCIE_CAP_LINK_CTRL2  0x30U

/* Link Control bits */
#define GTX_LINK_CTRL_RETRAIN   (1U << 5)

/* Link Status bits */
#define GTX_LINK_STATUS_DLL     (1U << 13)

/*=============================================================================
 * MSI Capability Registers
 *============================================================================*/

#define GTX_MSI_FLAGS           0x02U
#define GTX_MSI_ADDR_LO         0x04U
#define GTX_MSI_ADDR_HI         0x08U
#define GTX_MSI_DATA_32         0x08U
#define GTX_MSI_DATA_64         0x0CU
#define GTX_MSI_FLAGS_ENABLE    (1U << 0)
#define GTX_MSI_FLAGS_64BIT     (1U << 7)

/*=============================================================================
 * DesignWare Port Logic Registers
 *============================================================================*/

#define GTX_DW_PORT_LINK_CTRL   0x710U
#define GTX_DW_LINK_WIDTH_SPEED 0x80CU
#define GTX_DW_MISC_CTRL_1      0x8BCU
#define GTX_DW_DEBUG_0          0x728U
#define GTX_DW_DEBUG_1          0x72CU
#define GTX_DBI_RO_WR_EN        (1U << 0)

/* PORT_LINK_CTRL bits */
#define GTX_PORT_LINK_DLL_EN    (1U << 5)
#define GTX_PORT_LINK_FAST_MODE (1U << 7)
#define GTX_PORT_LINK_MODE_MASK (0x3FU << 16)
#define GTX_PORT_LINK_MODE_1L   (0x01U << 16)
#define GTX_PORT_LINK_MODE_2L   (0x03U << 16)
#define GTX_PORT_LINK_MODE_4L   (0x07U << 16)
#define GTX_PORT_LINK_MODE_8L   (0x0FU << 16)
#define GTX_PORT_LINK_MODE_16L  (0x1FU << 16)

/* LINK_WIDTH_SPEED bits */
#define GTX_LINK_SPEED_CHANGE   (1U << 17)

/*=============================================================================
 * iATU Registers
 *============================================================================*/

#define GTX_IATU_STRIDE         0x200U
#define GTX_IATU_OUTBOUND_BASE  0x300000U
#define GTX_IATU_INBOUND_BASE   (0x300000U + 8U * 0x200U)
#define GTX_IATU_CTRL1          0x00U
#define GTX_IATU_CTRL2          0x04U
#define GTX_IATU_LWR_BASE       0x08U
#define GTX_IATU_UPPER_BASE     0x0CU
#define GTX_IATU_LIMIT          0x10U
#define GTX_IATU_LWR_TARGET     0x14U
#define GTX_IATU_UPPER_TARGET   0x18U
#define GTX_IATU_UPPER_LIMIT    0x20U
#define GTX_IATU_CTRL2_ENABLE   (1U << 31)
#define GTX_IATU_CTRL2_BAR_MODE (1U << 30)
#define GTX_IATU_CTRL2_CFG_SHIFT (1U << 28)

/* iATU types */
#define GTX_IATU_TYPE_MEM       0x0U
#define GTX_IATU_TYPE_IO        0x2U
#define GTX_IATU_TYPE_CFG0      0x4U
#define GTX_IATU_TYPE_CFG1      0x5U

/*=============================================================================
 * Constants
 *============================================================================*/

#define GTX_PCIE_MAX_ATU_REGIONS 8U
#define GTX_PCIE_MAX_BARS        6U

/*=============================================================================
 * Enumerations
 *============================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_REGS_H */
