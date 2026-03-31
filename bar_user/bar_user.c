#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "../common/pcie_device.h"
#include "bar_user.h"

int bar_user_access(int rw, uint64_t address, uint32_t *value)
{
    const char *device;
    int fd;
    long page_size;
    uint64_t target_aligned;
    uint64_t page_offset;
    size_t map_size;
    char *map_base;
    volatile uint32_t *virt_addr;

    if (value == NULL) {
        fprintf(stderr, "bar_user_access: value pointer is NULL\n");
        return -1;
    }

    device = get_pcie_device(PCIE_DEV_USER);
    if (device == NULL) {
        fprintf(stderr, "USER device not found\n");
        return -1;
    }

    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        fprintf(stderr, "failed to get page size\n");
        return -1;
    }

    target_aligned = address & ~((uint64_t)page_size - 1ULL);
    page_offset = address - target_aligned;
    map_size = (size_t)(page_offset + sizeof(uint32_t));

    fd = open(device, O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "character device %s open failed: %s\n",
                device, strerror(errno));
        return -1;
    }

    map_base = (char *)mmap(NULL, map_size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, (off_t)target_aligned);
    if (map_base == MAP_FAILED) {
        fprintf(stderr, "Memory 0x%lx mapped failed: %s\n",
                address, strerror(errno));
        close(fd);
        return -1;
    }

    virt_addr = (volatile uint32_t *)(map_base + page_offset);

    if (rw == BAR_USER_WRITE) {
        *virt_addr = *value;
        printf("Write 32-bit value 0x%08x at USER address 0x%lx (%p), device=%s\n",
               *value, address, (void *)virt_addr, device);
    } else if (rw == BAR_USER_READ) {
        *value = *virt_addr;
        printf("Read 32-bit value 0x%08x at USER address 0x%lx (%p), device=%s\n",
               *value, address, (void *)virt_addr, device);
    } else {
        fprintf(stderr, "invalid rw mode: %d\n", rw);
        munmap(map_base, map_size);
        close(fd);
        return -1;
    }

    munmap(map_base, map_size);
    close(fd);
    return 0;
}