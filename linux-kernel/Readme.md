# Xilinx/dma_ip_drivers github의 read/wrtie 분석

## 서론
- PCIe(Peripheral Component Interconnect Express)는 기존 병렬 버스 방식이었던 PCI와 PCI-X의 한계를 극복하고 대역폭과 확장성을 높이기 위해 개발된 고속 인터페이스 기술.
- 오늘날 거의 모든 PC, 서버, 산업용 컴퓨터에서 주변기기를 연결하는 핵심 표준으로 사용.

## PCIE 핵심 개념
- 직렬 및 점대점 연결 (Serial & Point-to-Point): 여러 기기가 하나의 버스를 공유하던 기존 PCI 방식과 달리, 두 기기가 1:1로 직접 연결되는 점대점(Point-to-Point) 직렬 통신 방식을 사용.
- 링크(Link)와 레인(Lane): 두 기기간의 물리적 연결 통로를 **링크**라고 하며, 링크는 하나 이상의 **레인**으로 구성.
- 각 레인은 송신(Transmit)과 수신(Recevice)를 위한 차동 신호(Differential Signal) 쌍으로 구성되며 필요에 따라 
