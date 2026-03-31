/**
 * @file pcie.c
 * @brief GTX PCIe Driver Implementation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "drivers/pcie.h"
#include "drivers/timer.h"

// 해당 pcie.c 코드는 PCIe 프로토콜 스택 중 가장 하위 계층인 물리 계층(Physical Layer)의 초기화와 데이터 링크 계층 (Data Link Layer)의 활성화를 제어하며
// 상위 트랜잭션 게층이 동작할 수 있도록 주소 쳬계를 잡아주는 역할을 함.

// 1. 물리 계층 (physical Layer): 하드웨어의 전기적 연결, 차동 신호, 속도(Gen), 레인(Lane) 수를 직접 결정하는 부분.
// 관련 코드
// gtx_pcie_set_num_lanes: 물리적인 선(Lane)의 개수를 설정.
// gtx_pcie_set_target_speed: 데이터 전송 속도(Gen1 ~ Gen5)를 결정.
// gtx_pcie_is_link_up: LTSSM(상태 머신)이 L0(정상 작동) 상태인지 확인.
// 하드웨어 동작: 실제 구리선에서 전기 신호가 오가고, 클럭이 동기화되는 물리적인 준비 단계

// 2. 데이터 링크 계층 (Data Link Layer): 패킷 전송의 신뢰성을 확보하고 링크를 유지하는 단계
// 관련 코드
// gtx_pcie_start_link 내의 GTX_PORT_LINK_DLL_EN: 데이터 링크 계층(DLL)을 활성화.
// gtx_pcie_get_link_status 내의 status -> dll_active: DLL이 활성화되어 패킷을 주고 받을 준비가 됐는지 확인
// 하드웨어 동작: 패킷이 깨졌을 때 다시 보내는 Ack/Nak 메커니즘과 흐름 제어(Flow Control)가 이 계층에서 하드웨어적으로 처리.

// 3. 트랜잭션 계층 (Transaction Layer): CPU가 메모리 읽기/쓰기 명령을 내렸을 때 이를 TLP(Transaction Layer Packet)로 변환하기 위한 주소 지도를 그리는 단계.
// 관련 코드
// gtx_pcie_configure_atu: PCIe 내부의 주소 변환 장치(iATU)를 설정.
// gtx_pcie_setup_inbound_bar: 호스트(PC)가 내 메모리에 접근할 수 있는 창구(BAR)를 염.
// 하드웨어 동작: CPU 주소를 PCIe 버스 주소로, 혹은 그 반대로 하드웨어가 실시간으로 번역할 수 있게 세팅.

// 4. 설정 공간(Configuration Space): PCIe 스펙에서 정의한 표준 레지스터를 건드려 하드웨어의 기능을 탐색하는 부분.
// 관련 코드
// pcie_find_capability: 하드웨어가 지원하는 특수 기능의 위치를 찾음.
// gtx_pcie_setup_msi: 메시지 기반 인터럽트(MSI)를 설정.

/*=============================================================================
 * Private Data
 *============================================================================*/

static struct {
    uintptr_t dbi_base;
    uintptr_t cfg_base;
    gtx_pcie_config_t config;
    uint16_t pcie_cap_offset;
    bool initialized;
} g_pcie = {
    .dbi_base = GTX_PCIE_DBI_BASE,
    .cfg_base = GTX_PCIE_CFG_BASE,
    .pcie_cap_offset = 0,
    .initialized = false,
};

/*=============================================================================
 * MMIO Access Helpers
 *============================================================================*/

static inline void pcie_write32(uintptr_t addr, uint32_t value) {
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t pcie_read32(uintptr_t addr) {
    return *(volatile uint32_t *)addr;
}

static inline void pcie_write16(uintptr_t addr, uint16_t value) {
    *(volatile uint16_t *)addr = value;
}

static inline uint16_t pcie_read16(uintptr_t addr) {
    return *(volatile uint16_t *)addr;
}

static inline void pcie_write8(uintptr_t addr, uint8_t value) {
    *(volatile uint8_t *)addr = value;
}

static inline uint8_t pcie_read8(uintptr_t addr) {
    return *(volatile uint8_t *)addr;
}

/*=============================================================================
 * DBI Register Access
 *============================================================================*/

uint32_t gtx_pcie_read_dbi(uint32_t offset) {
    return pcie_read32(g_pcie.dbi_base + offset);
}

void gtx_pcie_write_dbi(uint32_t offset, uint32_t value) {
    pcie_write32(g_pcie.dbi_base + offset, value);
}

uint16_t gtx_pcie_read_dbi16(uint32_t offset) {
    return pcie_read16(g_pcie.dbi_base + offset);
}

void gtx_pcie_write_dbi16(uint32_t offset, uint16_t value) {
    pcie_write16(g_pcie.dbi_base + offset, value);
}

uint8_t gtx_pcie_read_dbi8(uint32_t offset) {
    return pcie_read8(g_pcie.dbi_base + offset);
}

void gtx_pcie_write_dbi8(uint32_t offset, uint8_t value) {
    pcie_write8(g_pcie.dbi_base + offset, value);
}

/*=============================================================================
 * Private Helper Functions
 *============================================================================*/


static void pcie_set_bits(uint32_t offset, uint32_t bits) {
    uint32_t val = gtx_pcie_read_dbi(offset);
    val |= bits;
    gtx_pcie_write_dbi(offset, val);
}

static void pcie_clear_bits(uint32_t offset, uint32_t bits) {
    uint32_t val = gtx_pcie_read_dbi(offset);
    val &= ~bits;
    gtx_pcie_write_dbi(offset, val);
}

static void pcie_modify(uint32_t offset, uint32_t mask, uint32_t value) {
    uint32_t val = gtx_pcie_read_dbi(offset);
    val = (val & ~mask) | (value & mask);
    gtx_pcie_write_dbi(offset, val);
}

static void pcie_enable_dbi_ro_wr(void) {
    pcie_set_bits(GTX_DW_MISC_CTRL_1, GTX_DBI_RO_WR_EN);
}

static void pcie_disable_dbi_ro_wr(void) {
    pcie_clear_bits(GTX_DW_MISC_CTRL_1, GTX_DBI_RO_WR_EN);
}

static uint16_t pcie_find_capability(uint8_t cap_id) {
    uint16_t status = gtx_pcie_read_dbi16(GTX_PCI_STATUS);
    if (!(status & GTX_PCI_STATUS_CAP_LIST)) {
        return 0;
    }

    uint8_t pos = gtx_pcie_read_dbi8(GTX_PCI_CAP_PTR) & 0xFC;
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

static void pcie_find_pcie_capability(void) {
    g_pcie.pcie_cap_offset = pcie_find_capability(GTX_PCI_CAP_ID_PCIE);
}

static void pcie_configure_as_endpoint(void) {
    pcie_enable_dbi_ro_wr();

    /* Enable memory and bus master */
    uint16_t cmd = gtx_pcie_read_dbi16(GTX_PCI_COMMAND);
    cmd |= GTX_PCI_CMD_MEM_EN | GTX_PCI_CMD_BUS_MASTER;
    gtx_pcie_write_dbi16(GTX_PCI_COMMAND, cmd);

    pcie_disable_dbi_ro_wr();
}

static void pcie_configure_as_rc(void) {
    pcie_enable_dbi_ro_wr();

    /* Set class to PCI-PCI bridge */
    uint32_t class_reg = gtx_pcie_read_dbi(GTX_PCI_REVISION_ID);
    class_reg = (class_reg & 0xFF) | 0x06040000;
    gtx_pcie_write_dbi(GTX_PCI_REVISION_ID, class_reg);
    gtx_pcie_write_dbi8(GTX_PCI_HEADER_TYPE, 0x01);

    pcie_disable_dbi_ro_wr();
}

static void pcie_enable_fast_link(void) {
    pcie_set_bits(GTX_DW_PORT_LINK_CTRL, GTX_PORT_LINK_FAST_MODE);
}

static gtx_pcie_status_t pcie_wait_for_link(uint32_t timeout_us) {
    uint64_t start = gtx_timer_get_us();

    while ((gtx_timer_get_us() - start) < timeout_us) {
        if (gtx_pcie_is_link_up()) {
            return GTX_PCIE_OK;
        }
        gtx_delay_us(100);
    }

    return GTX_PCIE_TIMEOUT;
}

static uintptr_t pcie_get_atu_region_base(gtx_atu_direction_t dir, uint8_t region) {
    if (dir == GTX_ATU_OUTBOUND) {
        return g_pcie.dbi_base + GTX_IATU_OUTBOUND_BASE + region * GTX_IATU_STRIDE;
    } else {
        return g_pcie.dbi_base + GTX_IATU_INBOUND_BASE + region * GTX_IATU_STRIDE;
    }
}

static gtx_pcie_status_t pcie_wait_atu_enabled(gtx_atu_direction_t dir, uint8_t region) {
    uintptr_t base = pcie_get_atu_region_base(dir, region);

    for (int i = 0; i < 1000; ++i) {
        uint32_t ctrl2 = pcie_read32(base + GTX_IATU_CTRL2);
        if (ctrl2 & GTX_IATU_CTRL2_ENABLE) {
            return GTX_PCIE_OK;
        }
        gtx_delay_us(1);
    }

    return GTX_PCIE_TIMEOUT;
}

/*=============================================================================
 * Public Functions - Configuration
 *============================================================================*/

gtx_pcie_config_t gtx_pcie_default_config(void) {
    gtx_pcie_config_t config = {
        .mode = GTX_PCIE_MODE_ENDPOINT,
        .num_lanes = GTX_PCIE_X4,
        .max_speed = GTX_PCIE_GEN4_16_0GT,
        .fast_link_mode = false,
    };
    return config;
}

gtx_pcie_status_t gtx_pcie_init(const gtx_pcie_config_t *config) {
    if (config == NULL) {
        g_pcie.config = gtx_pcie_default_config();
    } else {
        g_pcie.config = *config;
    }

    /* Find PCIe capability */
    pcie_find_pcie_capability();

    /* Enable DBI write */
    pcie_enable_dbi_ro_wr();

    /* Configure lanes */
    gtx_pcie_set_num_lanes(g_pcie.config.num_lanes);

    /* Configure speed */
    gtx_pcie_set_target_speed(g_pcie.config.max_speed);

    /* Configure mode */
    if (g_pcie.config.mode == GTX_PCIE_MODE_ROOT_COMPLEX) {
        pcie_configure_as_rc();
    } else {
        pcie_configure_as_endpoint();
    }

    /* Fast link mode */
    if (g_pcie.config.fast_link_mode) {
        pcie_enable_fast_link();
    }

    /* Disable DBI write */
    pcie_disable_dbi_ro_wr();

    g_pcie.initialized = true;
    return GTX_PCIE_OK;
}

gtx_pcie_status_t gtx_pcie_start_link(void) {
    if (!g_pcie.initialized) {
        return GTX_PCIE_NOT_INITIALIZED;
    }

    /* Enable DLL link */
    pcie_set_bits(GTX_DW_PORT_LINK_CTRL, GTX_PORT_LINK_DLL_EN);

    /* Wait for link with 5 second timeout */
    return pcie_wait_for_link(5000000);
}

gtx_pcie_status_t gtx_pcie_init_and_start(const gtx_pcie_config_t *config) {
    gtx_pcie_status_t status = gtx_pcie_init(config);
    if (status != GTX_PCIE_OK) {
        return status;
    }
    return gtx_pcie_start_link();
}

bool gtx_pcie_is_initialized(void) {
    return g_pcie.initialized;
}

/*=============================================================================
 * Public Functions - Link Status
 *============================================================================*/

bool gtx_pcie_is_link_up(void) {
    uint32_t debug0 = gtx_pcie_read_dbi(GTX_DW_DEBUG_0);
    uint8_t ltssm = debug0 & 0x3F;
    return ltssm == 0x11;  /* L0 state */
}

gtx_pcie_speed_t gtx_pcie_get_current_speed(void) {
    if (g_pcie.pcie_cap_offset == 0) {
        return GTX_PCIE_GEN1_2_5GT;
    }
    uint16_t status = gtx_pcie_read_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_STATUS);
    return (gtx_pcie_speed_t)(status & 0xF);
}

gtx_pcie_width_t gtx_pcie_get_current_width(void) {
    if (g_pcie.pcie_cap_offset == 0) {
        return GTX_PCIE_X1;
    }
    uint16_t status = gtx_pcie_read_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_STATUS);
    return (gtx_pcie_width_t)((status >> 4) & 0x3F);
}

gtx_pcie_speed_t gtx_pcie_get_max_speed(void) {
    if (g_pcie.pcie_cap_offset == 0) {
        return GTX_PCIE_GEN1_2_5GT;
    }
    uint32_t cap = gtx_pcie_read_dbi(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CAP);
    return (gtx_pcie_speed_t)(cap & 0xF);
}

gtx_pcie_width_t gtx_pcie_get_max_width(void) {
    if (g_pcie.pcie_cap_offset == 0) {
        return GTX_PCIE_X1;
    }
    uint32_t cap = gtx_pcie_read_dbi(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CAP);
    return (gtx_pcie_width_t)((cap >> 4) & 0x3F);
}

void gtx_pcie_get_link_status(gtx_pcie_link_status_t *status) {
    if (status == NULL) return;

    status->current_speed = gtx_pcie_get_current_speed();
    status->current_width = gtx_pcie_get_current_width();
    status->max_speed = gtx_pcie_get_max_speed();
    status->max_width = gtx_pcie_get_max_width();
    status->link_up = gtx_pcie_is_link_up();

    if (g_pcie.pcie_cap_offset != 0) {
        uint16_t link_stat = gtx_pcie_read_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_STATUS);
        status->dll_active = (link_stat & GTX_LINK_STATUS_DLL) != 0;
    } else {
        status->dll_active = false;
    }
}

uint8_t gtx_pcie_get_ltssm_state(void) {
    return gtx_pcie_read_dbi(GTX_DW_DEBUG_0) & 0x3F;
}

const char *gtx_pcie_get_ltssm_name(void) {
    switch (gtx_pcie_get_ltssm_state()) {
        case 0x00: return "Detect.Quiet";
        case 0x01: return "Detect.Active";
        case 0x02: return "Polling.Active";
        case 0x11: return "L0";
        case 0x12: return "L0s";
        case 0x14: return "L1.Idle";
        case 0x17: return "Disabled";
        case 0x1B: return "Hot Reset";
        default:   return "Unknown";
    }
}

/*=============================================================================
 * Public Functions - Link Configuration
 *============================================================================*/

void gtx_pcie_set_num_lanes(gtx_pcie_width_t width) {
    uint32_t mode = 0;
    switch (width) {
        case GTX_PCIE_X1:  mode = GTX_PORT_LINK_MODE_1L;  break;
        case GTX_PCIE_X2:  mode = GTX_PORT_LINK_MODE_2L;  break;
        case GTX_PCIE_X4:  mode = GTX_PORT_LINK_MODE_4L;  break;
        case GTX_PCIE_X8:  mode = GTX_PORT_LINK_MODE_8L;  break;
        case GTX_PCIE_X16: mode = GTX_PORT_LINK_MODE_16L; break;
    }
    pcie_modify(GTX_DW_PORT_LINK_CTRL, GTX_PORT_LINK_MODE_MASK, mode);
}

void gtx_pcie_set_target_speed(gtx_pcie_speed_t speed) {
    if (g_pcie.pcie_cap_offset == 0) return;

    /* Link Control 2 register */
    uint16_t ctrl2 = gtx_pcie_read_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CTRL2);
    ctrl2 = (ctrl2 & ~0xF) | (uint8_t)speed;
    gtx_pcie_write_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CTRL2, ctrl2);
}

gtx_pcie_status_t gtx_pcie_retrain_link(void) {
    if (g_pcie.pcie_cap_offset == 0) {
        return GTX_PCIE_ERROR;
    }

    uint16_t ctrl = gtx_pcie_read_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CTRL);
    ctrl |= GTX_LINK_CTRL_RETRAIN;
    gtx_pcie_write_dbi16(g_pcie.pcie_cap_offset + GTX_PCIE_CAP_LINK_CTRL, ctrl);

    return pcie_wait_for_link(1000000);  /* 1s timeout */
}

gtx_pcie_status_t gtx_pcie_change_speed(gtx_pcie_speed_t target) {
    gtx_pcie_set_target_speed(target);

    /* Enable directed speed change */
    pcie_set_bits(GTX_DW_LINK_WIDTH_SPEED, GTX_LINK_SPEED_CHANGE);

    /* Wait for speed change */
    for (int i = 0; i < 100; ++i) {
        gtx_delay_ms(10);
        if (gtx_pcie_get_current_speed() == target) {
            return GTX_PCIE_OK;
        }
    }

    return GTX_PCIE_TIMEOUT;
}

/*=============================================================================
 * Public Functions - ATU Configuration
 *============================================================================*/

gtx_pcie_status_t gtx_pcie_configure_atu(const gtx_atu_region_t *region) {
    if (region == NULL || region->region >= GTX_PCIE_MAX_ATU_REGIONS) {
        return GTX_PCIE_INVALID_PARAM;
    }

    uintptr_t base = pcie_get_atu_region_base(region->direction, region->region);

    /* Disable region first */
    pcie_write32(base + GTX_IATU_CTRL2, 0);

    /* Set base address */
    pcie_write32(base + GTX_IATU_LWR_BASE, (uint32_t)region->cpu_addr);
    pcie_write32(base + GTX_IATU_UPPER_BASE, (uint32_t)(region->cpu_addr >> 32));

    /* Set limit */
    uint64_t limit = region->cpu_addr + region->size - 1;
    pcie_write32(base + GTX_IATU_LIMIT, (uint32_t)limit);
    pcie_write32(base + GTX_IATU_UPPER_LIMIT, (uint32_t)(limit >> 32));

    /* Set target */
    pcie_write32(base + GTX_IATU_LWR_TARGET, (uint32_t)region->pci_addr);
    pcie_write32(base + GTX_IATU_UPPER_TARGET, (uint32_t)(region->pci_addr >> 32));

    /* Set control 1 */
    uint32_t ctrl1 = (uint32_t)region->type;
    pcie_write32(base + GTX_IATU_CTRL1, ctrl1);

    /* Set control 2 and enable */
    uint32_t ctrl2 = GTX_IATU_CTRL2_ENABLE;
    if (region->bar_match_mode) {
        ctrl2 |= GTX_IATU_CTRL2_BAR_MODE | (region->bar_num << 8);
    }
    if (region->type == GTX_ATU_TYPE_CFG0 || region->type == GTX_ATU_TYPE_CFG1) {
        ctrl2 |= GTX_IATU_CTRL2_CFG_SHIFT;
    }
    pcie_write32(base + GTX_IATU_CTRL2, ctrl2);

    /* Wait for enable */
    return pcie_wait_atu_enabled(region->direction, region->region);
}

gtx_pcie_status_t gtx_pcie_setup_outbound_mem(
    uint8_t region, uint64_t cpu_addr, uint64_t pci_addr, uint64_t size)
{
    gtx_atu_region_t atu = {
        .direction = GTX_ATU_OUTBOUND,
        .region = region,
        .type = GTX_ATU_TYPE_MEM,
        .cpu_addr = cpu_addr,
        .pci_addr = pci_addr,
        .size = size,
        .bar_num = 0,
        .bar_match_mode = false,
    };
    return gtx_pcie_configure_atu(&atu);
}

gtx_pcie_status_t gtx_pcie_setup_inbound_bar(
    uint8_t region, uint8_t bar_num, uint64_t cpu_addr, uint64_t size)
{
    gtx_atu_region_t atu = {
        .direction = GTX_ATU_INBOUND,
        .region = region,
        .type = GTX_ATU_TYPE_MEM,
        .cpu_addr = cpu_addr,
        .pci_addr = 0,
        .size = size,
        .bar_num = bar_num,
        .bar_match_mode = true,
    };
    return gtx_pcie_configure_atu(&atu);
}

void gtx_pcie_disable_atu(gtx_atu_direction_t direction, uint8_t region) {
    uintptr_t base = pcie_get_atu_region_base(direction, region);
    pcie_write32(base + GTX_IATU_CTRL2, 0);
}

/*=============================================================================
 * Public Functions - MSI
 *============================================================================*/

uint16_t gtx_pcie_find_msi_cap(void) {
    return pcie_find_capability(GTX_PCI_CAP_ID_MSI);
}

gtx_pcie_status_t gtx_pcie_setup_msi(uint64_t addr, uint16_t data) {
    uint16_t cap = gtx_pcie_find_msi_cap();
    if (cap == 0) {
        return GTX_PCIE_ERROR;
    }

    uint16_t flags = gtx_pcie_read_dbi16(cap + GTX_MSI_FLAGS);
    bool is_64bit = (flags & GTX_MSI_FLAGS_64BIT) != 0;

    /* Set address */
    gtx_pcie_write_dbi(cap + GTX_MSI_ADDR_LO, (uint32_t)addr);
    if (is_64bit) {
        gtx_pcie_write_dbi(cap + GTX_MSI_ADDR_HI, (uint32_t)(addr >> 32));
        gtx_pcie_write_dbi16(cap + GTX_MSI_DATA_64, data);
    } else {
        gtx_pcie_write_dbi16(cap + GTX_MSI_DATA_32, data);
    }

    /* Enable */
    flags |= GTX_MSI_FLAGS_ENABLE;
    gtx_pcie_write_dbi16(cap + GTX_MSI_FLAGS, flags);

    return GTX_PCIE_OK;
}

/*=============================================================================
 * Public Functions - Utility
 *============================================================================*/

const char *gtx_pcie_speed_to_string(gtx_pcie_speed_t speed) {
    switch (speed) {
        case GTX_PCIE_GEN1_2_5GT:  return "2.5 GT/s (Gen1)";
        case GTX_PCIE_GEN2_5_0GT:  return "5.0 GT/s (Gen2)";
        case GTX_PCIE_GEN3_8_0GT:  return "8.0 GT/s (Gen3)";
        case GTX_PCIE_GEN4_16_0GT: return "16.0 GT/s (Gen4)";
        case GTX_PCIE_GEN5_32_0GT: return "32.0 GT/s (Gen5)";
        default: return "Unknown";
    }
}

const char *gtx_pcie_width_to_string(gtx_pcie_width_t width) {
    switch (width) {
        case GTX_PCIE_X1:  return "x1";
        case GTX_PCIE_X2:  return "x2";
        case GTX_PCIE_X4:  return "x4";
        case GTX_PCIE_X8:  return "x8";
        case GTX_PCIE_X16: return "x16";
        default: return "Unknown";
    }
}
