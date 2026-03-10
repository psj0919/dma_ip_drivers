/**
 * @file pcie_irq.c
 * @brief GTX PCIe IRQ Handler Implementation (C version)
 *
 * Uses riscv-csr.h for CSR access and riscv-interrupts.h for constants.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "drivers/pcie-irq.h"
#include "drivers/pcie.h"
#include "drivers/pcie-elbi.h"
#include "drivers/pcie-msi.h"

/* RISC-V CSR and interrupt definitions */
#include "riscv/riscv-csr.h"
#include "riscv/riscv-interrupts.h"

#include "drivers/sc_print.h"
/*=============================================================================
 * Private Data Types
 *============================================================================*/

typedef struct {
    gtx_pcie_irq_handler_t handler;
    void *context;
} irq_handler_entry_t;

typedef struct {
    irq_handler_entry_t handlers[GTX_PCIE_IRQ_MAX];
    uint32_t enabled_mask;
    bool initialized;
} pcie_irq_state_t;

/*=============================================================================
 * Private Data
 *============================================================================*/

static pcie_irq_state_t g_pcie_irq = {
    .handlers = {{NULL, NULL}},
    .enabled_mask = 0,
    .initialized = false,
};

/*=============================================================================
 * ELBI Handler Callback
 *============================================================================*/

static void pcie_irq_elbi_callback(uint32_t source, void *user_data)
{
    (void)user_data;

    if (source >= GTX_PCIE_IRQ_MAX) {
        return;
    }

    irq_handler_entry_t *entry = &g_pcie_irq.handlers[source];
    if (entry->handler != NULL) {
        entry->handler(entry->context);
    }
}

/*=============================================================================
 * Initialization
 *============================================================================*/

int gtx_pcie_irq_init(void)
{
    /* Initialize handlers array */
    for (int i = 0; i < GTX_PCIE_IRQ_MAX; i++) {
        g_pcie_irq.handlers[i].handler = NULL;
        g_pcie_irq.handlers[i].context = NULL;
    }
    g_pcie_irq.enabled_mask = 0;

    /* Initialize PCIe if not already done */
    if (!gtx_pcie_is_initialized()) {
        gtx_pcie_status_t status = gtx_pcie_init(NULL);
        if (status != GTX_PCIE_OK) {
            return -1;
        }
    }

    /* Initialize ELBI */
    gtx_elbi_init();
    gtx_elbi_register_handler(pcie_irq_elbi_callback, NULL);

    /* Initialize MSI */
    gtx_msi_init();

    g_pcie_irq.initialized = true;
    return 0;
}

bool gtx_pcie_irq_is_initialized(void)
{
    return g_pcie_irq.initialized;
}

/*=============================================================================
 * Handler Registration
 *============================================================================*/

int gtx_pcie_irq_register(gtx_pcie_irq_source_t source,
                               gtx_pcie_irq_handler_t handler,
                               void *context)
{
    if (source >= GTX_PCIE_IRQ_MAX) {
        return -1;
    }

    g_pcie_irq.handlers[source].handler = handler;
    g_pcie_irq.handlers[source].context = context;

    return 0;
}

void gtx_pcie_irq_unregister(gtx_pcie_irq_source_t source)
{
    if (source >= GTX_PCIE_IRQ_MAX) {
        return;
    }

    g_pcie_irq.handlers[source].handler = NULL;
    g_pcie_irq.handlers[source].context = NULL;

    /* Also disable the IRQ */
    gtx_pcie_irq_disable(source);
}

/*=============================================================================
 * IRQ Enable/Disable
 *============================================================================*/

void gtx_pcie_irq_enable(gtx_pcie_irq_source_t source)
{
    if (source >= GTX_PCIE_IRQ_MAX) {
        return;
    }

    g_pcie_irq.enabled_mask |= (1U << source);
    gtx_elbi_unmask(source);
}

void gtx_pcie_irq_disable(gtx_pcie_irq_source_t source)
{
    if (source >= GTX_PCIE_IRQ_MAX) {
        return;
    }

    g_pcie_irq.enabled_mask &= ~(1U << source);
    gtx_elbi_mask(source);
}

void gtx_pcie_irq_enable_all(void)
{
    /* Enable all sources that have registered handlers */
    for (int i = 0; i < GTX_PCIE_IRQ_MAX; i++) {
        if (g_pcie_irq.handlers[i].handler != NULL) {
            gtx_pcie_irq_enable((gtx_pcie_irq_source_t)i);
        }
    }
}

void gtx_pcie_irq_disable_all(void)
{
    g_pcie_irq.enabled_mask = 0;
    gtx_elbi_mask_all();
}

/*=============================================================================
 * IRQ Handling
 *============================================================================*/

uint32_t gtx_pcie_irq_handle(void)
{
    return gtx_elbi_handle_interrupt();
}

uint32_t gtx_pcie_irq_poll(void)
{
    /* Check for pending interrupts without waiting */
    uint32_t status = gtx_elbi_get_status();
    if (status == 0) {
        return 0;
    }

    return gtx_pcie_irq_handle();
}

/*=============================================================================
 * MSI Send Functions
 *============================================================================*/

int gtx_pcie_irq_send_msi(gtx_pcie_msi_vector_t vector)
{
    if (vector >= GTX_PCIE_MSI_MAX) {
        return -1;
    }

    gtx_msi_status_t status = gtx_msi_send_dbi((uint8_t)vector);
    return (status == GTX_MSI_OK) ? 0 : -1;
}

int gtx_pcie_irq_send_msi_multiple(uint32_t vectors)
{
    gtx_msi_status_t status = gtx_msi_send_dbi_multiple(vectors);
    return (status == GTX_MSI_OK) ? 0 : -1;
}

/*=============================================================================
 * RISC-V Interrupt Setup
 *============================================================================*/

void gtx_pcie_irq_enable_riscv_external(void)
{
    /* Enable machine external interrupt in mie CSR */
    csr_set_bits_mie(MIE_MEI_BIT_MASK);

    /* Enable global machine interrupt in mstatus CSR */
    csr_set_bits_mstatus(MSTATUS_MIE_BIT_MASK);
}

void gtx_pcie_irq_disable_riscv_external(void)
{
    /* Disable machine external interrupt in mie CSR */
    csr_clr_bits_mie(MIE_MEI_BIT_MASK);
}

/*=============================================================================
 * Debug
 *============================================================================*/

extern int printf(const char *format, ...) __attribute__((weak));

void gtx_pcie_irq_print_status(void)
{
    if (!printf) return;

    printf("PCIe IRQ Status:\n");
    printf("  Initialized: %s\n", g_pcie_irq.initialized ? "yes" : "no");
    printf("  Enabled mask: 0x%08x\n", (unsigned int)g_pcie_irq.enabled_mask);
    printf("  Registered handlers:\n");

    for (int i = 0; i < GTX_PCIE_IRQ_MAX; i++) {
        if (g_pcie_irq.handlers[i].handler != NULL) {
            printf("    [%2d] %s\n", i,
                   (g_pcie_irq.enabled_mask & (1U << i)) ? "enabled" : "disabled");
        }
    }

    /* Print ELBI status */
    printf("\n");
    gtx_elbi_print_status();

    /* Print MSI config */
    printf("\n");
    gtx_msi_print_config();
}
