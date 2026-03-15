# PCIe 정리
- 1. 기본 역할 설명 
- 2. 실제 호출 흐름

# 정리 방식 
- 함수명
    - 역할:
    - 호출 흐름:
    - 최종 동작:
    - 비고:

- firmware 쪽 pcie는 device 내부에서 PCIe controller를 직접 제어하는 코드

## Link
- PCIe 연결 상태 자체.
- 즉 host와 device 사이의 PCIe 물리/논리 연결이 정상적으로 올라왔는지를 의미.
- *** PCIe 통신선이 실제로 연결되어 동작 가능한 상태인가”를 다루는 개념 ***

## ATU(Address Translation Unit)
- CPU가 보는 로컬 주소
- PCIe 버스에서 사용하는 주소가 다를 수 있어서, 이 둘을 연결해주는 설정이 필요합니다.
- *** PCIe read/write가 어디로 가야 하는지 주소 길을 열어주는 기능 ***

## Capability
- PCIe 장치가 지원하는 기능 정보 블록.
- *** PCIe 장치가 어떤 기능을 지원하는지 적혀 있는 표준 정보 영역 ***


## pcie.c
### DBI read/write 계열
- gtx_pcie_read_dbi / gtx_pcie_read_dbi16 / gtx_pcie_read_dbi8
    - PCIe DBI 영역에서 32 / 16 / 8 비트 레지스터를 읽음
    - gtx_pcie_read_dbi / gtx_pcie_read_dbi16 / gtx_pcie_read_dbi8 (offset)
        → pcie_read32 / pcie_read16 / pcie_read8 (g_pcie.dbi_base + offset)
        → *(volatile uint32_t *)addr
    - g_pcie.dbi_base + offset 주소의 MMIO 32비트 읽기.
- DBI는 payload 데이터가 아니라 PCIe 컨트롤러 내부 레지스터 접근에 사용됨.

- gtx_pcie_write_dbi / gtx_pcie_write_dbi16 / gtx_pcie_write_dbi8
    - PCIe DBI 영역에서 32 / 16 / 8 비트 레지스터를 기록.
    - gtx_pcie_write_dbi / gtx_pcie_write_dbi16 / gtx_pcie_write_dbi8 (offset)
        → pcie_write32 / pcie_write16 / pcie_write8 (g_pcie.dbi_base + offset)
        → *(volatile uint32_t *)addr
- g_pcie.dbi_base + offset 주소의 MMIO 32비트 쓰기.

### DBI 비트 제어 helper
- pcie_set_bits(uint32_t offset, uint32_t bits)
    - 특정 DBI 레지스터의 비트를 set
    - pcie_set_bits(offset, bits)
    -  → gtx_pcie_read_dbi(offset)
    -  → pcie_read32(...)
    -  → 읽은 값에 |= bits
    -  → gtx_pcie_write_dbi(offset, val)
    -  → pcie_write32(...)
    - read-modify-write

- pcie_clear_bits(uint32_t offset, uint32_t bits)
    - 특정 비트를 clear
    - pcie_clear_bits(offset, bits)
    - → gtx_pcie_read_dbi(offset)
    - → pcie_read32(...)
    - → 읽은 값에 &= ~bits
    - → gtx_pcie_write_dbi(offset, val)
    - → pcie_write32(...)
    - 

- pcie_modify(uint32_t offset, uint32_t mask, uint32_t value)
    - 특정 mask 영역만 수정
    - pcie_modify(offset, mask, value)
    - → gtx_pcie_read_dbi(offset)
    - → pcie_read32(...)
    - → (val & ~mask) | (value & mask)
    - → gtx_pcie_write_dbi(offset, val)
    - → pcie_write32(...)

- pcie_enable_dbi_ro_wr(void)
    - DBI read-only 영역까지 write 가능하게 열어줌
    - pcie_enable_dbi_ro_wr()
    - → pcie_set_bits(GTX_DW_MISC_CTRL_1, GTX_DBI_RO_WR_EN)
    - → gtx_pcie_read_dbi()
    - → gtx_pcie_write_dbi()
- DBI 제어 레지스터의 write-enable bit set

- pcie_disable_dbi_ro_wr(void)
    - 위에서 열어준 DBI write 권한을 다시 닫음
    - pcie_disable_dbi_ro_wr()
    - → pcie_clear_bits(GTX_DW_MISC_CTRL_1, GTX_DBI_RO_WR_EN)
    - → gtx_pcie_read_dbi()
    - → gtx_pcie_write_dbi()

### Capability 탐색 흐름
- PCIe capability가 어느 offset에 있는지 찾는 과정

- pcie_find_capability(uint8_t cap_id)
    - config space capability list를 따라가면서 원하는 capability offset 탐색
    - pcie_find_capability(cap_id)
    - → gtx_pcie_read_dbi16(GTX_PCI_STATUS)
    - → capability list 존재 여부 확인
    - → gtx_pcie_read_dbi8(GTX_PCI_CAP_PTR)
    - → while loop로 capability chain 순회
    - → 각 위치에서 gtx_pcie_read_dbi8(pos)로 cap id 읽기
    - → gtx_pcie_read_dbi8(pos + 1)로 next pointer 읽기
- DBI config space를 순회하여 capability offset 반환

- pcie_find_pcie_capability(void)
    - PCIe capability offset 저장
    - pcie_find_pcie_capability()
    - → pcie_find_capability(GTX_PCI_CAP_ID_PCIE)
    - → 결과를 g_pcie.pcie_cap_offset에 저장

- gtx_pcie_find_msi_cap(void)
    - MSI capability offset 찾기
    - gtx_pcie_find_msi_cap()
    - → pcie_find_capability(GTX_PCI_CAP_ID_MSI)

### 모드 설정 흐름
- pcie_configure_as_endpoint(void)
    - PCIe 컨트롤러를 Endpoint 모드로 설정
    - pcie_configure_as_endpoint()
    - → pcie_enable_dbi_ro_wr()
    - → gtx_pcie_read_dbi16(GTX_PCI_COMMAND)
    - → cmd |= MEM_EN | BUS_MASTER
    - → gtx_pcie_write_dbi16(GTX_PCI_COMMAND, cmd)
    - → pcie_disable_dbi_ro_wr()
- command register에서 memory enable, bus master enable 설정

- pcie_configure_as_rc(void)
    - PCIe 컨트롤러를 Root Complex 모드로 설정
    - pcie_configure_as_rc()
    - → pcie_enable_dbi_ro_wr()
    - → gtx_pcie_read_dbi(GTX_PCI_REVISION_ID)
    - → class code 수정
    - → gtx_pcie_write_dbi(GTX_PCI_REVISION_ID, class_reg)
    - → gtx_pcie_write_dbi8(GTX_PCI_HEADER_TYPE, 0x01)
    - → pcie_disable_dbi_ro_wr()

### PCIe 초기화 전체 흐름
- gtx_pcie_init(const gtx_pcie_config_t *config)
    - PCIe 기본 설정 초기화
    - gtx_pcie_init(config)
    - → config 저장 (NULL이면 gtx_pcie_default_config())
    - → pcie_find_pcie_capability()
    - → pcie_enable_dbi_ro_wr()
    - → gtx_pcie_set_num_lanes(...)
    - → 내부에서 pcie_modify(...)
    - → 내부에서 gtx_pcie_read_dbi() / gtx_pcie_write_dbi()
    - → gtx_pcie_set_target_speed(...)
    - → 내부에서 gtx_pcie_read_dbi16() / gtx_pcie_write_dbi16()
    - → mode에 따라
      - pcie_configure_as_rc() 또는 pcie_configure_as_endpoint()
    - → fast link면 pcie_enable_fast_link()
    - → 내부에서 pcie_set_bits(...)
    - → pcie_disable_dbi_ro_wr()
    - → g_pcie.initialized = true
- lane/speed/mode/fast link까지 DBI 레지스터 기반으로 설정 완료

- gtx_pcie_start_link(void)
    - PCIe link를 실제로 올리고 link up까지 대기
    - gtx_pcie_start_link()
    - → initialized 확인
    - → pcie_set_bits(GTX_DW_PORT_LINK_CTRL, GTX_PORT_LINK_DLL_EN)
    - → 내부에서 gtx_pcie_read_dbi() / gtx_pcie_write_dbi()
    - → pcie_wait_for_link(5000000)
    - → loop에서 gtx_pcie_is_link_up() 반복 호출
    - → gtx_pcie_read_dbi(GTX_DW_DEBUG_0)
    - → LTSSM이 L0인지 확인
- DLL enable 후 link up polling

- gtx_pcie_init_and_start(const gtx_pcie_config_t *config)
    - 초기화 + 링크 시작을 한 번에 수행
    - gtx_pcie_init_and_start(config)
    - → gtx_pcie_init(config)
    - → 성공 시 gtx_pcie_start_link()

### 링크 상태 확인 흐름
- gtx_pcie_is_link_up(void)
    - LTSSM 상태를 읽어서 link up 여부 확인
    - gtx_pcie_is_link_up()
    - → gtx_pcie_read_dbi(GTX_DW_DEBUG_0)
    - → pcie_read32(...)
    - → 하위 6비트 LTSSM 추출
    - → 0x11(L0)인지 비교

- gtx_pcie_get_current_speed(void)
    - 현재 협상된 링크 속도 읽기
    - gtx_pcie_get_current_speed()
    - → g_pcie.pcie_cap_offset 확인
    - → gtx_pcie_read_dbi16(pcie_cap_offset + GTX_PCIE_CAP_LINK_STATUS)
    - → LINK_STATUS 하위 비트 해석

- gtx_pcie_get_current_width(void)
    - 현재 lane width 읽기
    - gtx_pcie_get_current_width()
    - → gtx_pcie_read_dbi16(... LINK_STATUS)
    - → width field 추출

- gtx_pcie_get_link_status(gtx_pcie_link_status_t *status)
    - speed/width/link up/DLL active를 한 번에 수집
    - gtx_pcie_get_link_status(status)
    - → gtx_pcie_get_current_speed()
    - → gtx_pcie_get_current_width()
    - → gtx_pcie_get_max_speed()
    - → gtx_pcie_get_max_width()
    - → gtx_pcie_is_link_up()
    - → 필요 시 gtx_pcie_read_dbi16(... LINK_STATUS)로 dll_active 계산
    
### 링크 재설정 / 속도 변경 흐름
- gtx_pcie_set_num_lanes(gtx_pcie_width_t width)
    - lane 개수 설정
    - gtx_pcie_set_num_lanes(width)
    - → width에 따라 mode 값 결정
    - → pcie_modify(GTX_DW_PORT_LINK_CTRL, GTX_PORT_LINK_MODE_MASK, mode)
    - → 내부에서 gtx_pcie_read_dbi()
    - → gtx_pcie_write_dbi()

- gtx_pcie_set_target_speed(gtx_pcie_speed_t speed)
    - 목표 링크 속도 설정
    - gtx_pcie_set_target_speed(speed)
    - → gtx_pcie_read_dbi16(pcie_cap_offset + LINK_CTRL2)
    - → speed field 수정
    - → gtx_pcie_write_dbi16(... LINK_CTRL2, ctrl2)

- gtx_pcie_retrain_link(void)
    - 링크 retrain 요청
    - gtx_pcie_retrain_link()
    - → gtx_pcie_read_dbi16(... LINK_CTRL)
    - → retrain bit set
    - → gtx_pcie_write_dbi16(... LINK_CTRL, ctrl)
    - → pcie_wait_for_link(1000000)
    - → 내부에서 반복적으로 gtx_pcie_is_link_up()

- gtx_pcie_change_speed(gtx_pcie_speed_t target)
    - 목표 속도를 바꾸고 실제 변경 완료까지 확인
    - gtx_pcie_change_speed(target)
    - → gtx_pcie_set_target_speed(target)
    - → pcie_set_bits(GTX_DW_LINK_WIDTH_SPEED, GTX_LINK_SPEED_CHANGE)
    - → loop에서 gtx_pcie_get_current_speed() 반복 확인

### ATU 설정 흐름
- gtx_pcie_configure_atu(const gtx_atu_region_t *region)
    - iATU region 하나를 설정
    - gtx_pcie_configure_atu(region)
    - → pcie_get_atu_region_base(direction, region)
    - → pcie_write32(base + CTRL2, 0) 로 disable
    - → pcie_write32(base + LWR_BASE, ...)
    - → pcie_write32(base + UPPER_BASE, ...)
    - → pcie_write32(base + LIMIT, ...)
    - → pcie_write32(base + UPPER_LIMIT, ...)
    - → pcie_write32(base + LWR_TARGET, ...)
    - → pcie_write32(base + UPPER_TARGET, ...)
    - → pcie_write32(base + CTRL1, type)
    - → pcie_write32(base + CTRL2, ctrl2 | ENABLE)
    - → pcie_wait_atu_enabled(direction, region)
    - → loop에서 pcie_read32(base + GTX_IATU_CTRL2) 확인
- ATU region enable 완료까지 polling

- gtx_pcie_setup_outbound_mem(region, cpu_addr, pci_addr, size)
    - outbound memory ATU 설정
    - gtx_pcie_setup_outbound_mem(...)
    - → gtx_atu_region_t atu 구조체 생성
    - → gtx_pcie_configure_atu(&atu)

- gtx_pcie_setup_inbound_bar(region, bar_num, cpu_addr, size)
    - inbound BAR 매핑 설정
    - gtx_pcie_setup_inbound_bar(...)
    - → gtx_atu_region_t atu 구조체 생성
    - → gtx_pcie_configure_atu(&atu)

- gtx_pcie_disable_atu(direction, region)
    - 특정 iATU region 비활성화
    - gtx_pcie_disable_atu(direction, region)
    - → pcie_get_atu_region_base(direction, region)
    - → pcie_write32(base + GTX_IATU_CTRL2, 0)

### MSI 설정 흐름
- gtx_pcie_setup_msi(uint64_t addr, uint16_t data)
    - MSI capability에 host MSI address/data를 기록하고 enable
    - gtx_pcie_setup_msi(addr, data)
    - → gtx_pcie_find_msi_cap()
    - → 내부에서 pcie_find_capability(MSI)
    - → gtx_pcie_read_dbi16(cap + GTX_MSI_FLAGS)
    - → 64bit 여부 확인
    - → gtx_pcie_write_dbi(cap + GTX_MSI_ADDR_LO, ...)
    - → 필요 시 gtx_pcie_write_dbi(cap + GTX_MSI_ADDR_HI, ...)
    - → gtx_pcie_write_dbi16(... MSI_DATA, data)
    - → flags에 enable bit set
    - → gtx_pcie_write_dbi16(cap + GTX_MSI_FLAGS, flags)

## pcie_elib.c
### ELBI interrupt 흐름
- gtx_elbi_get_status(void)
    - ELBI interrupt status 읽기
    - gtx_elbi_get_status()
    - → elbi_read32(g_elbi.cfg_base + GTX_ELBI_CFG_INT_STATUS)
    - → *(volatile uint32_t *)addr
    
- gtx_elbi_mask(uint32_t source)
    - 특정 interrupt source mask
    - gtx_elbi_mask(source)
    - → elbi_read32(INT_MASK)
    - → 해당 비트 set
    - → elbi_write32(INT_MASK, mask)

- gtx_elbi_unmask(uint32_t source)
    - 특정 interrupt source unmask
    - gtx_elbi_unmask(source)
    - → elbi_read32(INT_MASK)
    - → 해당 비트 clear
    - → elbi_write32(INT_MASK, mask)

- gtx_elbi_clear(uint32_t source)
    - 특정 pending interrupt clear
    - gtx_elbi_clear(source)
    - → elbi_write32(INT_FLAG, 1U << source)

- gtx_elbi_handle_interrupt(void)
    - pending source를 순회하며 callback 실행 후 clear
    - gtx_elbi_handle_interrupt()
    - → gtx_elbi_get_status()
    - → while(status != 0)
    - → __builtin_ctz(status)로 source 추출
    - → 등록된 g_elbi.handler(source, user_data) 호출
    - → gtx_elbi_clear(source)
    - → 다음 source 처리

## pcie_irq.c
- gtx_pcie_irq_init(void)
    - PCIe IRQ 계층 전체 초기화
    - gtx_pcie_irq_init()
    - → handler 배열 초기화
    - → gtx_pcie_is_initialized() 확인
    - → 필요 시 gtx_pcie_init(NULL)
    - → gtx_elbi_init()
    - → gtx_elbi_register_handler(pcie_irq_elbi_callback, NULL)
    - → gtx_msi_init()
    - → initialized=true

- pcie_irq_elbi_callback(uint32_t source, void *user_data)
    - ELBI에서 올라온 source를 실제 등록된 IRQ handler로 전달
    - pcie_irq_elbi_callback(source, user_data)
    - → g_pcie_irq.handlers[source] 조회
    - → handler 있으면 entry->handler(entry->context) 호출

- gtx_pcie_irq_handle(void)
    - PCIe IRQ 처리 진입점
    - gtx_pcie_irq_handle()
    - → gtx_elbi_handle_interrupt()
    - → 내부에서 각 source별 callback 실행

- gtx_pcie_irq_poll(void)
    - polling 방식으로 pending 확인 후 처리
    - gtx_pcie_irq_poll()
    - → gtx_elbi_get_status()
    - → status가 0이 아니면 gtx_pcie_irq_handle()

## pcie_msi.c
### MSI 전송 흐름
- *** 여기는 Xilinx XDMA와 비교할 때도 중요 ***
- gtx_msi_init(void)
    - MSI/MSI-X/PCIe capability offset 찾기
    - gtx_msi_init()
    - → msi_find_capability(0x05)
    - → msi_find_capability(0x11)
    - → msi_find_capability(0x10)
    - → 각 함수 내부에서 gtx_pcie_read_dbi8/16()로 capability chain 순회

- gtx_msi_get_config(gtx_msi_config_t *config)
    - 현재 MSI address/data/vector 개수 읽기
    - gtx_msi_get_config(config)
    - → gtx_pcie_read_dbi16(cap + FLAGS)
    - → gtx_pcie_read_dbi(cap + ADDR_LO)
    - → 필요 시 gtx_pcie_read_dbi(cap + ADDR_HI)
    - → gtx_pcie_read_dbi16(cap + DATA)

- gtx_msi_send(uint8_t vector)
    - 표준 MSI 방식으로 host MSI address에 write
    - gtx_msi_send(vector)
    - → gtx_msi_get_config(&config)
    - → MSI enabled / vector 유효성 확인
    - → gtx_pcie_setup_outbound_mem(...)
    - → 내부에서 gtx_pcie_configure_atu(...)
    - → 내부에서 여러 pcie_write32(...) 수행
    - → ATU enable polling
    - → volatile uint32_t *msi_addr = ...
    - → *msi_addr = config.data | vector
    - → fence
    - → gtx_pcie_disable_atu(...)
- outbound ATU를 통해 host MSI address에 memory write

- gtx_msi_send_dbi(uint8_t vector)
    - DBI MSI generator 레지스터를 써서 MSI 생성
    - gtx_msi_send_dbi(vector)
    - → gtx_pcie_write_dbi(GTX_DBI_MSI_GEN, 1U << vector)
    - → pcie_write32(g_pcie.dbi_base + GTX_DBI_MSI_GEN, ...)
- DBI vendor extension 레지스터 write로 MSI TLP 생성

- gtx_pcie_irq_send_msi(gtx_pcie_msi_vector_t vector)
    - IRQ 계층에서 MSI 전송 wrapper
    - gtx_pcie_irq_send_msi(vector)
    - → gtx_msi_send_dbi((uint8_t)vector)







## 계층도

[ pcie_irq.c ]
   - IRQ handler 등록/분배
   - ELBI, MSI를 묶어서 상위 인터페이스 제공
        │
        ▼
[ pcie_elbi.c ]      [ pcie_msi.c ]
   - interrupt         - MSI/MSI-X capability 확인
     status/mask       - MSI 전송
   - pending 처리      - DBI MSI generator 사용
        │                 │
        └───────┬─────────┘
                ▼
            [ pcie.c ]
         - DBI read/write
         - link 설정
         - capability 탐색
         - ATU 설정
                │
                ▼
      [ PCIe Controller Hardware ]
