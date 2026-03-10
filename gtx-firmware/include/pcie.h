/**
 * @file pcie.h
 * @brief GTX PCIe Driver Header (C Implementation)
 *        Synopsys DesignWare PCIe Controller (C417-0, Gen4)
 *
 * Uses common register definitions from riscv/pcie-regs.h
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef GTX_PCIE_H
#define GTX_PCIE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Common PCIe register definitions */
#include "drivers/pcie-regs.h"
#include "drivers/pcie-elbi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Enumerations
 *============================================================================*/

typedef enum {
    GTX_PCIE_MODE_ENDPOINT = 0,
    GTX_PCIE_MODE_ROOT_COMPLEX = 1,
} gtx_pcie_mode_t;

typedef enum {
    GTX_PCIE_GEN1_2_5GT  = 1,
    GTX_PCIE_GEN2_5_0GT  = 2,
    GTX_PCIE_GEN3_8_0GT  = 3,
    GTX_PCIE_GEN4_16_0GT = 4,
    GTX_PCIE_GEN5_32_0GT = 5,
} gtx_pcie_speed_t;

typedef enum {
    GTX_PCIE_X1  = 1,
    GTX_PCIE_X2  = 2,
    GTX_PCIE_X4  = 4,
    GTX_PCIE_X8  = 8,
    GTX_PCIE_X16 = 16,
} gtx_pcie_width_t;

typedef enum {
    GTX_ATU_OUTBOUND = 0,
    GTX_ATU_INBOUND  = 1,
} gtx_atu_direction_t;

typedef enum {
    GTX_ATU_TYPE_MEM  = 0,
    GTX_ATU_TYPE_IO   = 2,
    GTX_ATU_TYPE_CFG0 = 4,
    GTX_ATU_TYPE_CFG1 = 5,
} gtx_atu_type_t;

/*=============================================================================
 * Status Codes
 *============================================================================*/

typedef enum {
    GTX_PCIE_OK              =  0,
    GTX_PCIE_ERROR           = -1,
    GTX_PCIE_TIMEOUT         = -2,
    GTX_PCIE_INVALID_PARAM   = -3,
    GTX_PCIE_NOT_INITIALIZED = -4,
} gtx_pcie_status_t;

/*=============================================================================
 * Structures
 *============================================================================*/

/**
 * @brief Link status information
 */
typedef struct {
    gtx_pcie_speed_t current_speed;
    gtx_pcie_width_t current_width;
    gtx_pcie_speed_t max_speed;
    gtx_pcie_width_t max_width;
    bool link_up;
    bool dll_active;
} gtx_pcie_link_status_t;

/**
 * @brief ATU region configuration
 */
typedef struct {
    gtx_atu_direction_t direction;
    uint8_t region;
    gtx_atu_type_t type;
    uint64_t cpu_addr;
    uint64_t pci_addr;
    uint64_t size;
    uint8_t bar_num;
    bool bar_match_mode;
} gtx_atu_region_t;

/**
 * @brief PCIe controller configuration
 */
typedef struct {
    gtx_pcie_mode_t mode;
    gtx_pcie_width_t num_lanes;
    gtx_pcie_speed_t max_speed;
    bool fast_link_mode;
} gtx_pcie_config_t;

/*=============================================================================
 * Initialization Functions
 *============================================================================*/

/**
 * @brief Get default PCIe configuration
 * @return Default configuration (Endpoint, x4, Gen4)
 */
gtx_pcie_config_t gtx_pcie_default_config(void);

/**
 * @brief Initialize PCIe controller
 * @param config Configuration parameters
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_init(const gtx_pcie_config_t *config);

/**
 * @brief Start PCIe link training
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_start_link(void);

/**
 * @brief Initialize and start PCIe link
 * @param config Configuration parameters (NULL for defaults)
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_init_and_start(const gtx_pcie_config_t *config);

/**
 * @brief Check if PCIe is initialized
 * @return true if initialized
 */
bool gtx_pcie_is_initialized(void);

/*=============================================================================
 * Link Status Functions
 *============================================================================*/

/**
 * @brief Check if link is up (in L0 state)
 * @return true if link is up
 */
bool gtx_pcie_is_link_up(void);

/**
 * @brief Get current link speed
 * @return Current link speed
 */
gtx_pcie_speed_t gtx_pcie_get_current_speed(void);

/**
 * @brief Get current link width
 * @return Current link width
 */
gtx_pcie_width_t gtx_pcie_get_current_width(void);

/**
 * @brief Get maximum link speed
 * @return Maximum link speed
 */
gtx_pcie_speed_t gtx_pcie_get_max_speed(void);

/**
 * @brief Get maximum link width
 * @return Maximum link width
 */
gtx_pcie_width_t gtx_pcie_get_max_width(void);

/**
 * @brief Get complete link status
 * @param status Pointer to status structure
 */
void gtx_pcie_get_link_status(gtx_pcie_link_status_t *status);

/**
 * @brief Get LTSSM state
 * @return LTSSM state value
 */
uint8_t gtx_pcie_get_ltssm_state(void);

/**
 * @brief Get LTSSM state name
 * @return State name string
 */
const char *gtx_pcie_get_ltssm_name(void);

/*=============================================================================
 * Link Configuration Functions
 *============================================================================*/

/**
 * @brief Set number of lanes
 * @param width Number of lanes
 */
void gtx_pcie_set_num_lanes(gtx_pcie_width_t width);

/**
 * @brief Set target link speed
 * @param speed Target speed
 */
void gtx_pcie_set_target_speed(gtx_pcie_speed_t speed);

/**
 * @brief Retrain link
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_retrain_link(void);

/**
 * @brief Change link speed
 * @param target Target speed
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_change_speed(gtx_pcie_speed_t target);

/*=============================================================================
 * ATU Configuration Functions
 *============================================================================*/

/**
 * @brief Configure ATU region
 * @param region ATU region configuration
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_configure_atu(const gtx_atu_region_t *region);

/**
 * @brief Setup outbound memory region
 * @param region Region number (0-7)
 * @param cpu_addr CPU address
 * @param pci_addr PCI address
 * @param size Region size
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_setup_outbound_mem(
    uint8_t region, uint64_t cpu_addr, uint64_t pci_addr, uint64_t size);

/**
 * @brief Setup inbound BAR region
 * @param region Region number (0-7)
 * @param bar_num BAR number (0-5)
 * @param cpu_addr CPU address
 * @param size Region size
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_setup_inbound_bar(
    uint8_t region, uint8_t bar_num, uint64_t cpu_addr, uint64_t size);

/**
 * @brief Disable ATU region
 * @param direction ATU direction
 * @param region Region number
 */
void gtx_pcie_disable_atu(gtx_atu_direction_t direction, uint8_t region);

/*=============================================================================
 * MSI Functions
 *============================================================================*/

/**
 * @brief Find MSI capability
 * @return Capability offset, or 0 if not found
 */
uint16_t gtx_pcie_find_msi_cap(void);

/**
 * @brief Setup MSI
 * @param addr MSI address
 * @param data MSI data value
 * @return Status code
 */
gtx_pcie_status_t gtx_pcie_setup_msi(uint64_t addr, uint16_t data);

/*=============================================================================
 * DBI Register Access Functions
 *============================================================================*/

/**
 * @brief Read DBI register
 * @param offset Register offset
 * @return Register value
 */
uint32_t gtx_pcie_read_dbi(uint32_t offset);

/**
 * @brief Write DBI register
 * @param offset Register offset
 * @param value Value to write
 */
void gtx_pcie_write_dbi(uint32_t offset, uint32_t value);

/**
 * @brief Read DBI register (16-bit)
 * @param offset Register offset
 * @return Register value
 */
uint16_t gtx_pcie_read_dbi16(uint32_t offset);

/**
 * @brief Write DBI register (16-bit)
 * @param offset Register offset
 * @param value Value to write
 */
void gtx_pcie_write_dbi16(uint32_t offset, uint16_t value);

/**
 * @brief Read DBI register (8-bit)
 * @param offset Register offset
 * @return Register value
 */
uint8_t gtx_pcie_read_dbi8(uint32_t offset);

/**
 * @brief Write DBI register (8-bit)
 * @param offset Register offset
 * @param value Value to write
 */
void gtx_pcie_write_dbi8(uint32_t offset, uint8_t value);

/*=============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get speed string
 * @param speed Link speed
 * @return Speed string
 */
const char *gtx_pcie_speed_to_string(gtx_pcie_speed_t speed);

/**
 * @brief Get width string
 * @param width Link width
 * @return Width string
 */
const char *gtx_pcie_width_to_string(gtx_pcie_width_t width);

#ifdef __cplusplus
}
#endif

#endif /* GTX_PCIE_H */
