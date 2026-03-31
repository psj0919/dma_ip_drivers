목적: chip의 bar0에 해당하는 dma_write, read를 먼저 application이 쓸 수 있게 function을 구성해야 한다.


main 안에 있던 내용을 function으로 재구성 


read/write 하는 function을 모두 한 개 파일에 넣는다.


pcie.c


void dma_write() { 
int cmd_opt;
	char *device = DEVICE_NAME_DEFAULT;
	uint64_t address = 0;
	uint64_t aperture = 0;
	uint64_t size = SIZE_DEFAULT;
	uint64_t offset = 0;
	uint64_t count = COUNT_DEFAULT;
	char *ofname = NULL;

	while ((cmd_opt = getopt_long(argc, argv, "vhec:f:d:a:k:s:o:", long_opts,
			    NULL)) != -1) {
		switch (cmd_opt) {
		case 0:
			/* long option */
			break;
		case 'd':
			/* device node name */
			device = strdup(optarg);
			break;
		case 'a':
			/* RAM address on the AXI bus in bytes */
			address = getopt_integer(optarg);
			break;
		case 'k':
			/* memory aperture windows size */
			aperture = getopt_integer(optarg);
			break;
		case 's':
			/* RAM size in bytes */
			size = getopt_integer(optarg);
			break;
		case 'o':
			offset = getopt_integer(optarg) & 4095;
			break;
			/* count */
		case 'c':
			count = getopt_integer(optarg);
			break;
			/* count */
		case 'f':
			ofname = strdup(optarg);
			break;
			/* print usage help and exit */
		case 'v':
			verbose = 1;
			break;
		case 'e':
			eop_flush = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
			break;
		}
	}
	if (verbose)
	fprintf(stdout,
		"dev %s, addr 0x%lx, aperture 0x%lx, size 0x%lx, offset 0x%lx, "
		"count %lu\n",
		device, address, aperture, size, offset, count);

	return test_dma(device, address, aperture, size, offset, count, ofname);
}


-> deivce 정보가 필요하고, kernerl function like ioctl ,..etc

어떻게 불러들일까?.. 

기존 xilinx xdma linux driver 동작
./dma_to_device -d /dev/xdma0_h2c -s 10 -a 0x4_0000_0000 -f data.hex
data.hex: 10bytes file

main() {
    pci_device = get_pcie_device();
    address = 0x4000000;
    data = fopen();
    size = ???
    dma_write(&pci_device, size , address, data);
}



