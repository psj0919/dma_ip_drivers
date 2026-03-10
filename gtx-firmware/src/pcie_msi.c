/**
 * @file pcie_msi.c
 * @brief GTX PCIe MSI/MSI-X Driver Implementation
 *        Based on Synopsys DesignWare PCIe Controller (pcie-designware-ep.c)
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "drivers/pcie-msi.h"
#include "drivers/pcie.h"
#include "drivers/sc_print.h"
/*=============================================================================
 * Private Data
 *============================================================================*/

static struct {
    uint16_t msi_cap_offset;        /* MSI capability offset in config space */
    uint16_t msix_cap_offset;       /* MSI-X capability offset */
    uint16_t pcie_cap_offset;       /* PCIe capability offset */
    uintptr_t msix_table_base;      /* MSI-X table virtual address */
    uint8_t  msi_atu_region;        /* iATU region for MSI mapping */
    bool     initialized;
} g_msi = {
    .msi_cap_offset = 0,
    .msix_cap_offset = 0,
    .pcie_cap_offset = 0,
    .msix_table_base = 0,
    .msi_atu_region = 7,            /* Use last iATU region for MSI */
    .initialized = false,
};

/*=============================================================================
 * Private Helper Functions
 *============================================================================*/

static uint16_t msi_find_capability(uint8_t cap_id)
{
    uint16_t status = gtx_pcie_read_dbi16(0x06);  /* PCI_STATUS */
    if (!(status & (1U << 4))) {  /* CAP_LIST bit */
        return 0;
    }

    uint8_t pos = gtx_pcie_read_dbi8(0x34) & 0xFC;  /* CAP_PTR */
    unsigned count = 0;

    while (pos && count < 48) {
        uint8_t id = gtx_pcie_read_dbi8(pos);
        if (id == cap_id) {
            return pos;
        }
        pos = gtx_pcie_read_dbi8(pos + 1) & 0xFC;
        ++count;
    }
    return 0;
}

/*=============================================================================
 * Initialization Functions
 *============================================================================*/

gtx_msi_status_t gtx_msi_init(void)
{
    /* Find MSI capability (cap_id = 0x05) */
    g_msi.msi_cap_offset = msi_find_capability(0x05);

    /* Find MSI-X capability (cap_id = 0x11) */
    g_msi.msix_cap_offset = msi_find_capability(0x11);

    /* Find PCIe capability (cap_id = 0x10) */
    g_msi.pcie_cap_offset = msi_find_capability(0x10);

    g_msi.initialized = true;

    return GTX_MSI_OK;
}

bool gtx_msi_is_initialized(void)
{
    return g_msi.initialized;
}

/*=============================================================================
 * MSI Configuration Functions
 *============================================================================*/

gtx_msi_status_t gtx_msi_get_config(gtx_msi_config_t *config)
{
    if (config == NULL) {
        return GTX_MSI_ERROR;
    }

    if (g_msi.msi_cap_offset == 0) {
        config->enabled = false;
        config->num_vectors = 0;
        return GTX_MSI_NO_CAPABILITY;
    }

    uint16_t cap = g_msi.msi_cap_offset;
    uint16_t flags = gtx_pcie_read_dbi16(cap + GTX_MSI_CAP_FLAGS);

    config->enabled = (flags & GTX_MSI_FLAG_ENABLE) != 0;
    config->is_64bit = (flags & GTX_MSI_FLAG_64BIT) != 0;

    /* Multi-Message Enable (bits 6:4) determines number of vectors */
    uint8_t mme = (flags >> GTX_MSI_FLAG_QSIZE_SHIFT) & 0x7;
    config->num_vectors = 1 << mme;  /* 2^mme vectors */

    /* Read address */
    uint32_t addr_lo = gtx_pcie_read_dbi(cap + GTX_MSI_CAP_ADDR_LO);
    if (config->is_64bit) {
        uint32_t addr_hi = gtx_pcie_read_dbi(cap + GTX_MSI_CAP_ADDR_HI);
        config->addr = ((uint64_t)addr_hi << 32) | addr_lo;
        config->data = gtx_pcie_read_dbi16(cap + GTX_MSI_CAP_DATA_64);
    } else {
        config->addr = addr_lo;
        config->data = gtx_pcie_read_dbi16(cap + GTX_MSI_CAP_DATA_32);
    }

    return GTX_MSI_OK;
}

bool gtx_msi_is_enabled(void)
{
    if (g_msi.msi_cap_offset == 0) {
        return false;
    }
    uint16_t flags = gtx_pcie_read_dbi16(g_msi.msi_cap_offset + GTX_MSI_CAP_FLAGS);
    return (flags & GTX_MSI_FLAG_ENABLE) != 0;
}

uint8_t gtx_msi_get_num_vectors(void)
{
    if (g_msi.msi_cap_offset == 0) {
        return 0;
    }
    uint16_t flags = gtx_pcie_read_dbi16(g_msi.msi_cap_offset + GTX_MSI_CAP_FLAGS);
    if (!(flags & GTX_MSI_FLAG_ENABLE)) {
        return 0;
    }
    uint8_t mme = (flags >> GTX_MSI_FLAG_QSIZE_SHIFT) & 0x7;
    return 1 << mme;
}

/*=============================================================================
 * MSI Send Functions
 *============================================================================*/

gtx_msi_status_t gtx_msi_send(uint8_t vector)
{
    gtx_msi_config_t config;
    gtx_msi_status_t status;

    status = gtx_msi_get_config(&config);
    if (status != GTX_MSI_OK) {
        return status;
    }

    if (!config.enabled) {
        return GTX_MSI_NOT_ENABLED;
    }

    if (vector >= config.num_vectors) {
        return GTX_MSI_INVALID_VECTOR;
    }

    /*
     * Standard MSI method (from Linux pcie-designware-ep.c):
     * 1. Map Host MSI address using outbound iATU
     * 2. Write MSI data to mapped address
     * 3. Unmap iATU region
     */

    /* Align address to 4KB for iATU */
    uint64_t aligned_addr = config.addr & ~0xFFFULL;
    uint32_t offset = config.addr & 0xFFF;

    /* Setup outbound ATU to map Host MSI address */
    gtx_pcie_status_t atu_status;
    atu_status = gtx_pcie_setup_outbound_mem(
        g_msi.msi_atu_region,
        GTX_PCIE_SLAVE_BASE,  /* Use slave region base */
        aligned_addr,
        0x1000                     /* 4KB region */
    );

    if (atu_status != GTX_PCIE_OK) {
        return GTX_MSI_ATU_ERROR;
    }

    /* Write MSI data (includes vector number) */
    volatile uint32_t *msi_addr = (volatile uint32_t *)(GTX_PCIE_SLAVE_BASE + offset);
    *msi_addr = config.data | vector;

    /* Memory barrier to ensure write completes */
    __asm__ volatile ("fence ow, ow" ::: "memory");

    /* Disable ATU region */
    gtx_pcie_disable_atu(GTX_ATU_OUTBOUND, g_msi.msi_atu_region);

    return GTX_MSI_OK;
}

gtx_msi_status_t gtx_msi_send_dbi(uint8_t vector)
{
    if (vector >= GTX_MSI_MAX_VECTORS) {
        return GTX_MSI_INVALID_VECTOR;
    }

    /*
     * DBI direct write method (DW PCIe vendor extension):
     * Write to DBI + 0x20_0d70 to generate MSI TLP.
     * The controller uses pre-configured MSI address/data.
     */
    gtx_pcie_write_dbi(GTX_DBI_MSI_GEN, 1U << vector);

    return GTX_MSI_OK;
}

gtx_msi_status_t gtx_msi_send_dbi_multiple(uint32_t vectors)
{
    gtx_pcie_write_dbi(GTX_DBI_MSI_GEN, vectors);
    return GTX_MSI_OK;
}

/*=============================================================================
 * MSI-X Configuration Functions
 *============================================================================*/

gtx_msi_status_t gtx_msix_get_config(gtx_msix_config_t *config)
{
    if (config == NULL) {
        return GTX_MSI_ERROR;
    }

    if (g_msi.msix_cap_offset == 0) {
        config->enabled = false;
        config->table_size = 0;
        return GTX_MSI_NO_CAPABILITY;
    }

    uint16_t cap = g_msi.msix_cap_offset;
    uint16_t flags = gtx_pcie_read_dbi16(cap + GTX_MSIX_CAP_FLAGS);

    config->enabled = (flags & GTX_MSIX_FLAG_ENABLE) != 0;
    config->function_mask = (flags & GTX_MSIX_FLAG_MASK) != 0;
    config->table_size = (flags & GTX_MSIX_TABLE_SIZE_MASK) + 1;

    /* Read table location */
    uint32_t table_reg = gtx_pcie_read_dbi(cap + GTX_MSIX_CAP_TABLE);
    config->table_bar = table_reg & 0x7;
    config->table_offset = table_reg & ~0x7;

    /* Read PBA location */
    uint32_t pba_reg = gtx_pcie_read_dbi(cap + GTX_MSIX_CAP_PBA);
    /* PBA BAR and offset are stored similarly */

    return GTX_MSI_OK;
}

bool gtx_msix_is_enabled(void)
{
    if (g_msi.msix_cap_offset == 0) {
        return false;
    }
    uint16_t flags = gtx_pcie_read_dbi16(g_msi.msix_cap_offset + GTX_MSIX_CAP_FLAGS);
    return (flags & GTX_MSIX_FLAG_ENABLE) != 0;
}

gtx_msi_status_t gtx_msix_read_entry(uint16_t vector,
                                              gtx_msix_entry_t *entry)
{
    if (entry == NULL) {
        return GTX_MSI_ERROR;
    }

    gtx_msix_config_t config;
    gtx_msi_status_t status = gtx_msix_get_config(&config);
    if (status != GTX_MSI_OK) {
        return status;
    }

    if (vector >= config.table_size) {
        return GTX_MSI_INVALID_VECTOR;
    }

    /*
     * MSI-X table is in BAR memory.
     * For bare-metal, we need to know the BAR's local address.
     * This depends on how BARs are mapped in device memory.
     */
    if (g_msi.msix_table_base == 0) {
        /* Table base not configured */
        return GTX_MSI_ERROR;
    }

    uintptr_t entry_addr = g_msi.msix_table_base +
                           config.table_offset +
                           vector * GTX_MSIX_ENTRY_SIZE;

    volatile uint32_t *e = (volatile uint32_t *)entry_addr;

    entry->addr = ((uint64_t)e[1] << 32) | e[0];
    entry->data = e[2];
    entry->masked = (e[3] & GTX_MSIX_ENTRY_CTRL_MASK) != 0;

    return GTX_MSI_OK;
}

/*=============================================================================
 * MSI-X Send Functions
 *============================================================================*/

gtx_msi_status_t gtx_msix_send(uint16_t vector)
{
    gtx_msix_entry_t entry;
    gtx_msi_status_t status;

    status = gtx_msix_read_entry(vector, &entry);
    if (status != GTX_MSI_OK) {
        return status;
    }

    if (entry.masked) {
        return GTX_MSI_NOT_ENABLED;
    }

    /*
     * Standard MSI-X method:
     * Map entry.addr using iATU, write entry.data
     */
    uint64_t aligned_addr = entry.addr & ~0xFFFULL;
    uint32_t offset = entry.addr & 0xFFF;

    gtx_pcie_status_t atu_status;
    atu_status = gtx_pcie_setup_outbound_mem(
        g_msi.msi_atu_region,
        GTX_PCIE_SLAVE_BASE,
        aligned_addr,
        0x1000
    );

    if (atu_status != GTX_PCIE_OK) {
        return GTX_MSI_ATU_ERROR;
    }

    volatile uint32_t *msix_addr = (volatile uint32_t *)(GTX_PCIE_SLAVE_BASE + offset);
    *msix_addr = entry.data;

    __asm__ volatile ("fence ow, ow" ::: "memory");

    gtx_pcie_disable_atu(GTX_ATU_OUTBOUND, g_msi.msi_atu_region);

    return GTX_MSI_OK;
}

gtx_msi_status_t gtx_msix_send_doorbell(uint16_t vector)
{
    if (vector >= GTX_MSIX_MAX_VECTORS) {
        return GTX_MSI_INVALID_VECTOR;
    }

    /*
     * MSI-X Doorbell method (DW PCIe specific):
     * Write to DBI + 0x948 with format:
     *   bits[31:24] = function number
     *   bits[23:0]  = interrupt_num (vector)
     *
     * From pcie-designware-ep.c:dw_pcie_ep_raise_msix_irq_doorbell()
     */
    uint32_t msg_data = (0 << GTX_MSIX_DOORBELL_PF_SHIFT) | vector;
    gtx_pcie_write_dbi(GTX_DBI_MSIX_DOORBELL, msg_data);

    return GTX_MSI_OK;
}

/*=============================================================================
 * Debug Functions
 *============================================================================*/

/* Forward declaration */
extern int printf(const char *format, ...) __attribute__((weak));

void gtx_msi_print_config(void)
{
    if (!printf) return;

    gtx_msi_config_t config;
    gtx_msi_get_config(&config);

    printf("MSI Configuration:\n");
    printf("  Capability offset: 0x%03x\n", g_msi.msi_cap_offset);
    printf("  Enabled: %s\n", config.enabled ? "yes" : "no");
    printf("  64-bit: %s\n", config.is_64bit ? "yes" : "no");
    printf("  Num vectors: %u\n", config.num_vectors);
    printf("  Address: 0x%08x%08x\n",
           (uint32_t)(config.addr >> 32), (uint32_t)config.addr);
    printf("  Data: 0x%04x\n", config.data);
}

void gtx_msix_print_config(void)
{
    if (!printf) return;

    gtx_msix_config_t config;
    gtx_msix_get_config(&config);

    printf("MSI-X Configuration:\n");
    printf("  Capability offset: 0x%03x\n", g_msi.msix_cap_offset);
    printf("  Enabled: %s\n", config.enabled ? "yes" : "no");
    printf("  Function mask: %s\n", config.function_mask ? "yes" : "no");
    printf("  Table size: %u\n", config.table_size);
    printf("  Table BAR: %u\n", config.table_bar);
    printf("  Table offset: 0x%08x\n", config.table_offset);
}
