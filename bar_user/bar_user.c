#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/mman.h>

#include "pcie-device.h"
#include "bar_user.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define ltohl(x)       (x)
#define ltohs(x)       (x)
#define htoll(x)       (x)
#define htols(x)       (x)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define ltohl(x)     __bswap_32(x)
#define ltohs(x)     __bswap_16(x)
#define htoll(x)     __bswap_32(x)
#define htols(x)     __bswap_16(x)
#endif

int bar_user_access(int rw, char access_width, uint64_t address, uint32_t *value)
{
    const char *device;
    int fd;
    long page_size;
    uint64_t target_aligned;
    uint64_t page_offset;
    size_t map_size;
    char *map_base;
    void *map;
    uint32_t read_result;
    uint32_t writeval;

    if (value == NULL) {
        fprintf(stderr, "bar_user_access: value pointer is NULL\n");
        return -1;
    }

    access_width = (char)tolower((unsigned char)access_width);

    printf("access width: ");
    if (access_width == 'b') {
        printf("byte (8-bits)\n");
    } else if (access_width == 'h') {
        printf("half word (16-bits)\n");
    } else if (access_width == 'w') {
        printf("word (32-bits)\n");
    } else {
        printf("default to word (32-bits)\n");
        access_width = 'w';
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
    map_size = (size_t)(page_offset + 4);

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

    map = (void *)(map_base + page_offset);

    if (rw == BAR_USER_READ) {
        switch (access_width) {
        case 'b':
            read_result = *((volatile uint8_t *)map);
            printf("Read 8-bits value at address 0x%lx (%p): 0x%02x\n",
                   address, map, (unsigned int)read_result);
            *value = read_result;
            break;

        case 'h':
            read_result = *((volatile uint16_t *)map);
            read_result = ltohs(read_result);
            printf("Read 16-bit value at address 0x%lx (%p): 0x%04x\n",
                   address, map, (unsigned int)read_result);
            *value = read_result;
            break;

        case 'w':
            read_result = *((volatile uint32_t *)map);
            read_result = ltohl(read_result);
            printf("Read 32-bit value at address 0x%lx (%p): 0x%08x\n",
                   address, map, (unsigned int)read_result);
            *value = read_result;
            break;

        default:
            fprintf(stderr, "Illegal data type '%c'\n", access_width);
            munmap(map_base, map_size);
            close(fd);
            return -1;
        }
    } else if (rw == BAR_USER_WRITE) {
        writeval = *value;

        switch (access_width) {
        case 'b':
            printf("Write 8-bits value 0x%02x to 0x%lx (%p)\n",
                   (unsigned int)writeval, address, map);
            *((volatile uint8_t *)map) = (uint8_t)writeval;
            break;

        case 'h':
            printf("Write 16-bits value 0x%04x to 0x%lx (%p)\n",
                   (unsigned int)writeval, address, map);
            writeval = htols(writeval);
            *((volatile uint16_t *)map) = (uint16_t)writeval;
            break;

        case 'w':
            printf("Write 32-bits value 0x%08x to 0x%lx (%p)\n",
                   (unsigned int)writeval, address, map);
            writeval = htoll(writeval);
            *((volatile uint32_t *)map) = (uint32_t)writeval;
            break;

        default:
            fprintf(stderr, "Illegal data type '%c'\n", access_width);
            munmap(map_base, map_size);
            close(fd);
            return -1;
        }
    } else {
        fprintf(stderr, "invalid rw mode: %d\n", rw);
        munmap(map_base, map_size);
        close(fd);
        return -1;
    }

    if (munmap(map_base, map_size) != 0) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}