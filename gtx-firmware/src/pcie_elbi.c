/**
 * @file pcie_elbi.c
 * @brief GTX PCIe ELBI Driver Implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "drivers/pcie-elbi.h"
#include "drivers/pcie-elbi-regs.h"
#include "drivers/pcie.h"
#include "drivers/sc_print.h"
/*=============================================================================
 * Private Data
 *============================================================================*/

static struct {
    uintptr_t dbi_base;
    uintptr_t cfg_base;
    gtx_elbi_handler_t handler;
    void *user_data;
    bool initialized;
} g_elbi = {
    .dbi_base = GTX_PCIE_DBI_BASE,
    .cfg_base = GTX_ELBI_CFG_BASE,
    .handler = NULL,
    .user_data = NULL,
    .initialized = false,
};

/*=============================================================================
 * MMIO Access Helpers
 *============================================================================*/

static inline void elbi_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t elbi_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

/*=============================================================================
 * Initialization Functions
 *============================================================================*/

void gtx_elbi_init(void) {
    /* Mask all interrupts initially */
    gtx_elbi_mask_all();

    /* Clear any pending interrupts */
    gtx_elbi_clear_all();

    g_elbi.initialized = true;
}

bool gtx_elbi_is_initialized(void) {
    return g_elbi.initialized;
}

/*=============================================================================
 * Handler Registration Functions
 *============================================================================*/

void gtx_elbi_register_handler(gtx_elbi_handler_t handler, void *user_data) {
    g_elbi.handler = handler;
    g_elbi.user_data = user_data;
}

void gtx_elbi_unregister_handler(void) {
    g_elbi.handler = NULL;
    g_elbi.user_data = NULL;
}

/*=============================================================================
 * Interrupt Masking Functions
 *============================================================================*/

void gtx_elbi_mask_all(void) {
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK, 0xFFFFFFFF);
}

void gtx_elbi_unmask_all(void) {
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK, 0x00000000);
}

void gtx_elbi_mask(uint32_t source) {
    if (source >= GTX_ELBI_NUM_INT_SOURCES) return;

    uint32_t mask = elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK);
    mask |= (1U << source);
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK, mask);
}

void gtx_elbi_unmask(uint32_t source) {
    if (source >= GTX_ELBI_NUM_INT_SOURCES) return;

    uint32_t mask = elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK);
    mask &= ~(1U << source);
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK, mask);
}

void gtx_elbi_set_mask(uint32_t mask) {
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK, mask);
}

uint32_t gtx_elbi_get_mask(void) {
    return elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_MASK);
}

/*=============================================================================
 * Interrupt Status Functions
 *============================================================================*/

uint32_t gtx_elbi_get_flag(void) {
    return elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_FLAG);
}

uint32_t gtx_elbi_get_status(void) {
    return elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_STATUS);
}

bool gtx_elbi_is_pending(uint32_t source) {
    if (source >= GTX_ELBI_NUM_INT_SOURCES) return false;
    return (gtx_elbi_get_status() & (1U << source)) != 0;
}

/*=============================================================================
 * Interrupt Clearing Functions
 *============================================================================*/

void gtx_elbi_clear(uint32_t source) {
    if (source >= GTX_ELBI_NUM_INT_SOURCES) return;
    /* Write 1 to clear */
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_FLAG, 1U << source);
}

void gtx_elbi_clear_multiple(uint32_t sources) {
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_FLAG, sources);
}

void gtx_elbi_clear_all(void) {
    elbi_write32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_FLAG, 0xFFFFFFFF);
}

/*=============================================================================
 * Interrupt Handling Functions
 *============================================================================*/

uint32_t gtx_elbi_handle_interrupt(void) {
    uint32_t status = gtx_elbi_get_status();
    if (status == 0) return 0;

    uint32_t handled = 0;

    /* Process each pending interrupt (LSB first = highest priority) */
    while (status != 0) {
        /* Find lowest set bit (highest priority interrupt) */
        uint32_t source = __builtin_ctz(status);

        /* Call handler if registered */
        if (g_elbi.handler != NULL) {
            g_elbi.handler(source, g_elbi.user_data);
        }

        /* Clear this interrupt */
        gtx_elbi_clear(source);

        /* Remove from status */
        status &= ~(1U << source);
        ++handled;
    }

    return handled;
}

/*=============================================================================
 * MSI Generation Functions
 *============================================================================*/

void gtx_elbi_send_msi(uint32_t vector) {
    if (vector >= GTX_ELBI_NUM_MSI_VECTORS) return;
    elbi_write32(g_elbi.dbi_base + GTX_ELBI_DBI_MSI_GEN, 1U << vector);
}

void gtx_elbi_send_msi_multiple(uint32_t vectors) {
    elbi_write32(g_elbi.dbi_base + GTX_ELBI_DBI_MSI_GEN, vectors);
}

/*=============================================================================
 * Debug Functions
 *============================================================================*/

/* Forward declaration for printf (if available) */
extern int printf(const char *format, ...);

void gtx_elbi_print_status(void) {
    uint32_t flag = gtx_elbi_get_flag();
    uint32_t mask = gtx_elbi_get_mask();
    uint32_t status = gtx_elbi_get_status();

    printf("ELBI Interrupt Status:\n");
    printf("  Flag (raw):    0x%08x\n", flag);
    printf("  Mask:          0x%08x\n", mask);
    printf("  Status (eff):  0x%08x\n", status);
}
