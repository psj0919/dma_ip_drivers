/**
 * @file pcie_msi.h
 * @brief GTX PCIe MSI/MSI-X Driver Header
 *        Based on Synopsys DesignWare PCIe Controller analysis
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ## MSI Transmission Mechanisms (Device -> Host)
 *
 * This driver provides two methods for sending MSI/MSI-X interrupts:
 *
 * ### Method 1: Standard MSI (via iATU mapping)
 * - Read MSI address/data from Host-configured MSI Capability
 * - Map Host MSI address using outbound iATU
 * - Write MSI data to mapped address -> generates Memory Write TLP
 * - Used by: dw_pcie_ep_raise_msi_irq() in Linux
 *
 * ### Method 2: DBI Direct Write (Vendor extension)
 * - Write to DBI offset 0x20_0d70 (MSI_GEN register)
 * - Controller automatically generates MSI TLP
 * - Simpler but requires DW PCIe IP support
 *
 * ### Method 3: MSI-X Doorbell (DW PCIe specific)
 * - Write to DBI offset 0x948 (MSIX_DOORBELL register)
 * - Format: (func_no << 24) | (interrupt_num - 1)
 * - Automatically generates MSI-X TLP
 */

#ifndef GTX_PCIE_MSI_H
#define GTX_PCIE_MSI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Register Definitions (DBI offsets)
 *============================================================================*/

/* MSI Capability structure offsets (relative to MSI cap base) */
#define GTX_MSI_CAP_FLAGS           0x02
#define GTX_MSI_CAP_ADDR_LO         0x04
#define GTX_MSI_CAP_ADDR_HI         0x08
#define GTX_MSI_CAP_DATA_32         0x08  /* If 32-bit capable */
#define GTX_MSI_CAP_DATA_64         0x0C  /* If 64-bit capable */

/* MSI flags */
#define GTX_MSI_FLAG_ENABLE         (1U << 0)
#define GTX_MSI_FLAG_64BIT          (1U << 7)
#define GTX_MSI_FLAG_QMASK_SHIFT    1
#define GTX_MSI_FLAG_QMASK          (0x7 << 1)  /* MMC: Multi-Message Capable */
#define GTX_MSI_FLAG_QSIZE_SHIFT    4
#define GTX_MSI_FLAG_QSIZE          (0x7 << 4)  /* MME: Multi-Message Enable */

/* MSI-X Capability structure offsets */
#define GTX_MSIX_CAP_FLAGS          0x02
#define GTX_MSIX_CAP_TABLE          0x04
#define GTX_MSIX_CAP_PBA            0x08

/* MSI-X flags */
#define GTX_MSIX_FLAG_ENABLE        (1U << 15)
#define GTX_MSIX_FLAG_MASK          (1U << 14)
#define GTX_MSIX_TABLE_SIZE_MASK    0x7FF

/* DBI MSI generation register (vendor extension) */
#define GTX_DBI_MSI_GEN             0x200D70

/* DBI MSI-X Doorbell register */
#define GTX_DBI_MSIX_DOORBELL       0x948
#define GTX_MSIX_DOORBELL_PF_SHIFT  24

/* MSI-X Table entry structure (in BAR memory) */
#define GTX_MSIX_ENTRY_SIZE         16
#define GTX_MSIX_ENTRY_ADDR_LO      0x00
#define GTX_MSIX_ENTRY_ADDR_HI      0x04
#define GTX_MSIX_ENTRY_DATA         0x08
#define GTX_MSIX_ENTRY_CTRL         0x0C
#define GTX_MSIX_ENTRY_CTRL_MASK    (1U << 0)

/*=============================================================================
 * Constants
 *============================================================================*/

#define GTX_MSI_MAX_VECTORS         32
#define GTX_MSIX_MAX_VECTORS        2048

/* Pre-defined MSI vector assignments - matches GTX_PCIE_MSI_* in pcie-elbi-regs.h */
#define GTX_MSI_VEC_CMD_ACK         0
#define GTX_MSI_VEC_INFER_DONE      1
#define GTX_MSI_VEC_ERROR           2
#define GTX_MSI_VEC_DMA_COMPLETE    3
#define GTX_MSI_VEC_MSG_SENT        4
#define GTX_MSI_VEC_STATUS_CHANGE   5
#define GTX_MSI_VEC_HEARTBEAT       6
#define GTX_MSI_VEC_ABORT_ACK       7

/*=============================================================================
 * Status Codes
 *============================================================================*/

typedef enum {
    GTX_MSI_OK              =  0,
    GTX_MSI_ERROR           = -1,
    GTX_MSI_NOT_ENABLED     = -2,
    GTX_MSI_INVALID_VECTOR  = -3,
    GTX_MSI_NO_CAPABILITY   = -4,
    GTX_MSI_ATU_ERROR       = -5,
} gtx_msi_status_t;

/*=============================================================================
 * Structures
 *============================================================================*/

/**
 * @brief MSI configuration (read from Host-programmed capability)
 */
typedef struct {
    uint64_t addr;              /* MSI target address */
    uint16_t data;              /* MSI data value */
    uint8_t  num_vectors;       /* Number of enabled vectors (1,2,4,8,16,32) */
    bool     enabled;           /* MSI enabled by Host */
    bool     is_64bit;          /* 64-bit address capable */
} gtx_msi_config_t;

/**
 * @brief MSI-X table entry
 */
typedef struct {
    uint64_t addr;              /* Message address */
    uint32_t data;              /* Message data */
    bool     masked;            /* Vector is masked */
} gtx_msix_entry_t;

/**
 * @brief MSI-X configuration
 */
typedef struct {
    uintptr_t table_base;       /* MSI-X table base address */
    uintptr_t pba_base;         /* PBA base address */
    uint16_t  table_size;       /* Number of entries (N+1) */
    uint8_t   table_bar;        /* BAR containing table */
    uint32_t  table_offset;     /* Offset within BAR */
    bool      enabled;          /* MSI-X enabled by Host */
    bool      function_mask;    /* Global function mask */
} gtx_msix_config_t;

/*=============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Initialize MSI subsystem
 * @return Status code
 *
 * Finds MSI/MSI-X capabilities and prepares for interrupt generation.
 */
gtx_msi_status_t gtx_msi_init(void);

/**
 * @brief Check if MSI subsystem is initialized
 * @return true if initialized
 */
bool gtx_msi_is_initialized(void);

/*=============================================================================
 * MSI Configuration Functions
 *============================================================================*/

/**
 * @brief Get current MSI configuration
 * @param config Pointer to config structure to fill
 * @return Status code
 *
 * Reads Host-programmed MSI address and data from MSI Capability.
 */
gtx_msi_status_t gtx_msi_get_config(gtx_msi_config_t *config);

/**
 * @brief Check if MSI is enabled by Host
 * @return true if MSI is enabled
 */
bool gtx_msi_is_enabled(void);

/**
 * @brief Get number of enabled MSI vectors
 * @return Number of vectors (0 if not enabled)
 */
uint8_t gtx_msi_get_num_vectors(void);

/*=============================================================================
 * MSI Send Functions
 *============================================================================*/

/**
 * @brief Send MSI using standard method (iATU mapping)
 * @param vector MSI vector number (0 to num_vectors-1)
 * @return Status code
 *
 * This method:
 * 1. Reads Host-configured MSI address from capability
 * 2. Maps the address using outbound iATU
 * 3. Writes MSI data | vector to generate Memory Write TLP
 * 4. Unmaps the iATU region
 *
 * This is the standard PCIe method used by Linux dw_pcie_ep_raise_msi_irq().
 */
gtx_msi_status_t gtx_msi_send(uint8_t vector);

/**
 * @brief Send MSI using DBI direct write (vendor extension)
 * @param vector MSI vector number (0-31)
 * @return Status code
 *
 * Writes to DBI + 0x20_0d70 to generate MSI.
 * This is a DW PCIe specific method, simpler than standard MSI.
 * The controller must be configured to support this mode.
 */
gtx_msi_status_t gtx_msi_send_dbi(uint8_t vector);

/**
 * @brief Send MSI using DBI (bitmask version)
 * @param vectors Bitmask of vectors to send
 * @return Status code
 */
gtx_msi_status_t gtx_msi_send_dbi_multiple(uint32_t vectors);

/*=============================================================================
 * MSI-X Configuration Functions
 *============================================================================*/

/**
 * @brief Get MSI-X configuration
 * @param config Pointer to config structure to fill
 * @return Status code
 */
gtx_msi_status_t gtx_msix_get_config(gtx_msix_config_t *config);

/**
 * @brief Check if MSI-X is enabled by Host
 * @return true if MSI-X is enabled
 */
bool gtx_msix_is_enabled(void);

/**
 * @brief Read MSI-X table entry
 * @param vector Vector number
 * @param entry Pointer to entry structure to fill
 * @return Status code
 */
gtx_msi_status_t gtx_msix_read_entry(uint16_t vector,
                                              gtx_msix_entry_t *entry);

/*=============================================================================
 * MSI-X Send Functions
 *============================================================================*/

/**
 * @brief Send MSI-X using standard method
 * @param vector MSI-X vector number
 * @return Status code
 *
 * Reads address/data from MSI-X table and generates Memory Write TLP.
 */
gtx_msi_status_t gtx_msix_send(uint16_t vector);

/**
 * @brief Send MSI-X using Doorbell method (DW PCIe specific)
 * @param vector MSI-X vector number
 * @return Status code
 *
 * Writes to DBI + 0x948 (MSIX_DOORBELL register).
 * Format: (func_no << 24) | (vector)
 */
gtx_msi_status_t gtx_msix_send_doorbell(uint16_t vector);

/*=============================================================================
 * Convenience Functions
 *============================================================================*/

/**
 * @brief Notify Host: Inference complete
 */
static inline void gtx_msi_notify_infer_done(void) {
    gtx_msi_send_dbi(GTX_MSI_VEC_INFER_DONE);
}

/**
 * @brief Notify Host: DMA complete
 */
static inline void gtx_msi_notify_dma_complete(void) {
    gtx_msi_send_dbi(GTX_MSI_VEC_DMA_COMPLETE);
}

/**
 * @brief Notify Host: Error occurred
 */
static inline void gtx_msi_notify_error(void) {
    gtx_msi_send_dbi(GTX_MSI_VEC_ERROR);
}

/**
 * @brief Notify Host: Command acknowledged
 */
static inline void gtx_msi_notify_cmd_ack(void) {
    gtx_msi_send_dbi(GTX_MSI_VEC_CMD_ACK);
}

/*=============================================================================
 * Debug Functions
 *============================================================================*/

/**
 * @brief Print MSI configuration
 */
void gtx_msi_print_config(void);

/**
 * @brief Print MSI-X configuration
 */
void gtx_msix_print_config(void);

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_MSI_H */
