#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "xdma/cdev_sgdma.h"
#include "dma_write_read.h"

#define Mode 1

extern int verbose;
extern int eop_flush;

extern ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern ssize_t write_from_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern void timespec_sub(struct timespec *t1, struct timespec *t2);

const char *get_pcie_device(pcie_dev_type_t type)
{
    static char device_path[128];
    int dev_idx, ch_idx;

    switch (type) {
    case PCIE_DEV_H2C:
        for (dev_idx = 0; dev_idx < 8; dev_idx++) {
            for (ch_idx = 0; ch_idx < 8; ch_idx++) {
                snprintf(device_path, sizeof(device_path),
                         "/dev/xdma%d_h2c_%d", dev_idx, ch_idx);
                if (access(device_path, F_OK) == 0) {
                    return device_path;
                }
            }
        }
        break;

    case PCIE_DEV_C2H:
        for (dev_idx = 0; dev_idx < 8; dev_idx++) {
            for (ch_idx = 0; ch_idx < 8; ch_idx++) {
                snprintf(device_path, sizeof(device_path),
                         "/dev/xdma%d_c2h_%d", dev_idx, ch_idx);
                if (access(device_path, F_OK) == 0) {
                    return device_path;
                }
            }
        }
        break;

    case PCIE_DEV_USER:
        for (dev_idx = 0; dev_idx < 8; dev_idx++) {
            snprintf(device_path, sizeof(device_path),
                     "/dev/xdma%d_user", dev_idx);
            if (access(device_path, F_OK) == 0) {
                return device_path;
            }
        }
        break;

    default:
        break;
    }

    return NULL;
}

static int dma_to_device_test_dma(char *devname, uint64_t addr, uint64_t aperture,
                                  uint64_t size, uint64_t offset, uint64_t count,
                                  char *infname, char *ofname)
{
    uint64_t i;
    ssize_t rc = 0;
    size_t bytes_done = 0;
    size_t out_offset = 0;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_start, ts_end;
    int infile_fd = -1;
    int outfile_fd = -1;
    int fpga_fd = open(devname, O_RDWR);
    long total_time = 0;
    float result;
    float avg_time = 0;
    int underflow = 0;

    if (fpga_fd < 0) {
        fprintf(stderr, "unable to open device %s, %d.\n", devname, fpga_fd);
        perror("open device");
        return -EINVAL;
    }

    if (infname) {
        infile_fd = open(infname, O_RDONLY);
        if (infile_fd < 0) {
            fprintf(stderr, "unable to open input file %s, %d.\n", infname, infile_fd);
            perror("open input file");
            rc = -EINVAL;
            goto out;
        }
    }

    if (ofname) {
        outfile_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (outfile_fd < 0) {
            fprintf(stderr, "unable to open output file %s, %d.\n", ofname, outfile_fd);
            perror("open output file");
            rc = -EINVAL;
            goto out;
        }
    }

    posix_memalign((void **)&allocated, 4096, size + 4096);
    if (!allocated) {
        fprintf(stderr, "OOM %lu.\n", size + 4096);
        rc = -ENOMEM;
        goto out;
    }

    buffer = allocated + offset;
    if (verbose) {
        fprintf(stdout, "host buffer 0x%lx = %p\n", size + 4096, buffer);
    }

    if (infile_fd >= 0) {
        rc = read_to_buffer(infname, infile_fd, buffer, size, 0);
        if (rc < 0 || (uint64_t)rc < size) {
            goto out;
        }
    }

    for (i = 0; i < count; i++) {
        rc = clock_gettime(CLOCK_MONOTONIC, &ts_start);

        if (aperture) {
            struct xdma_aperture_ioctl io;

            io.buffer = (unsigned long)buffer;
            io.len = size;
            io.ep_addr = addr;
            io.aperture = aperture;
            io.done = 0UL;

            rc = ioctl(fpga_fd, IOCTL_XDMA_APERTURE_W, &io);
            if (rc < 0 || io.error) {
                fprintf(stdout, "#%lu: aperture W ioctl failed %ld,%u.\n",
                        i, (long)rc, io.error);
                goto out;
            }

            bytes_done = io.done;
        } else {
            rc = write_from_buffer(devname, fpga_fd, buffer, size, addr);
            if (rc < 0) {
                goto out;
            }
            bytes_done = rc;
        }

        rc = clock_gettime(CLOCK_MONOTONIC, &ts_end);

        if (bytes_done < size) {
            printf("#%lu: underflow %zu/%lu.\n", i, bytes_done, size);
            underflow = 1;
        }

        timespec_sub(&ts_end, &ts_start);
        {
            const long NS_PER_SEC = 1000000000L;
            total_time += ts_end.tv_sec * NS_PER_SEC + ts_end.tv_nsec;
        }

        if (verbose) {
            fprintf(stdout,
                    "#%lu: CLOCK_MONOTONIC %ld.%09ld sec. write %lu bytes\n",
                    i, ts_end.tv_sec, ts_end.tv_nsec, size);
        }

        if (outfile_fd >= 0) {
            rc = write_from_buffer(ofname, outfile_fd, buffer, bytes_done, out_offset);
            if (rc < 0 || (size_t)rc < bytes_done) {
                goto out;
            }
            out_offset += bytes_done;
        }
    }

    if (!underflow) {
        avg_time = (float)total_time / (float)count;
        result = ((float)size) * 1000.0f / avg_time;

        if (verbose) {
            printf("** Avg time device %s, total time %ld nsec, avg_time = %f, size = %lu, BW = %f\n",
                   devname, total_time, avg_time, size, result);
        }
        printf("%s ** Average BW = %lu, %f\n", devname, size, result);
    }

out:
    close(fpga_fd);
    if (infile_fd >= 0) {
        close(infile_fd);
    }
    if (outfile_fd >= 0) {
        close(outfile_fd);
    }
    free(allocated);

    if (rc < 0) {
        return (int)rc;
    }

    return underflow ? -EIO : 0;
}

static int dma_from_device_test_dma(char *devname, uint64_t addr, uint64_t aperture,
                                    uint64_t size, uint64_t offset, uint64_t count,
                                    char *ofname)
{
    ssize_t rc = 0;
    size_t out_offset = 0;
    size_t bytes_done = 0;
    uint64_t i;
    char *buffer = NULL;
    char *allocated = NULL;
    struct timespec ts_start, ts_end;
    int out_fd = -1;
    int fpga_fd;
    long total_time = 0;
    float result;
    float avg_time = 0;
    int underflow = 0;

    if (eop_flush) {
        fpga_fd = open(devname, O_RDWR | O_TRUNC);
    } else {
        fpga_fd = open(devname, O_RDWR);
    }

    if (fpga_fd < 0) {
        fprintf(stderr, "unable to open device %s, %d.\n", devname, fpga_fd);
        perror("open device");
        return -EINVAL;
    }

    if (ofname) {
        out_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (out_fd < 0) {
            fprintf(stderr, "unable to open output file %s, %d.\n", ofname, out_fd);
            perror("open output file");
            rc = -EINVAL;
            goto out;
        }
    }

    posix_memalign((void **)&allocated, 4096, size + 4096);
    if (!allocated) {
        fprintf(stderr, "OOM %lu.\n", size + 4096);
        rc = -ENOMEM;
        goto out;
    }

    buffer = allocated + offset;
    if (verbose) {
        fprintf(stdout, "host buffer 0x%lx, %p.\n", size + 4096, buffer);
    }

    for (i = 0; i < count; i++) {
        rc = clock_gettime(CLOCK_MONOTONIC, &ts_start);

        if (aperture) {
            struct xdma_aperture_ioctl io;

            io.buffer = (unsigned long)buffer;
            io.len = size;
            io.ep_addr = addr;
            io.aperture = aperture;
            io.done = 0UL;

            rc = ioctl(fpga_fd, IOCTL_XDMA_APERTURE_R, &io);
            if (rc < 0 || io.error) {
                fprintf(stderr, "#%lu: aperture R failed %ld,%u.\n",
                        i, (long)rc, io.error);
                goto out;
            }

            bytes_done = io.done;
        } else {
            rc = read_to_buffer(devname, fpga_fd, buffer, size, addr);
            if (rc < 0) {
                goto out;
            }
            bytes_done = rc;
        }

        clock_gettime(CLOCK_MONOTONIC, &ts_end);

        if (bytes_done < size) {
            fprintf(stderr, "#%lu: underflow %zu/%lu.\n", i, bytes_done, size);
            underflow = 1;
        }

        timespec_sub(&ts_end, &ts_start);
        {
            const long NS_PER_SEC = 1000000000L;
            total_time += ts_end.tv_sec * NS_PER_SEC + ts_end.tv_nsec;
        }

        if (verbose) {
            fprintf(stdout,
                    "#%lu: CLOCK_MONOTONIC %ld.%09ld sec. read %zu/%lu bytes\n",
                    i, ts_end.tv_sec, ts_end.tv_nsec, bytes_done, size);
        }

        if (out_fd >= 0) {
            rc = write_from_buffer(ofname, out_fd, buffer, bytes_done, out_offset);
            if (rc < 0 || (size_t)rc < bytes_done) {
                goto out;
            }
            out_offset += bytes_done;
        }
    }

    if (!underflow) {
        avg_time = (float)total_time / (float)count;
        result = ((float)size) * 1000.0f / avg_time;

        if (verbose) {
            printf("** Avg time device %s, total time %ld nsec, avg_time = %f, size = %lu, BW = %f\n",
                   devname, total_time, avg_time, size, result);
        }
        printf("%s ** Average BW = %lu, %f\n", devname, size, result);
        rc = 0;
    } else if (eop_flush) {
        rc = 0;
    } else {
        rc = -EIO;
    }

out:
    close(fpga_fd);
    if (out_fd >= 0) {
        close(out_fd);
    }
    free(allocated);

    return (int)rc;
}

int bar_write_user(uint64_t address, uint32_t value)
{
    const char *device;
    int fd;
    long page_size;
    uint64_t target;
    uint64_t target_aligned;
    uint64_t page_offset;
    size_t map_size;
    char *map_base;
    volatile uint32_t *virt_addr;

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

    target = address;
    target_aligned = target & ~((uint64_t)page_size - 1ULL);
    page_offset = target - target_aligned;
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
                target, strerror(errno));
        close(fd);
        return -1;
    }

    virt_addr = (volatile uint32_t *)(map_base + page_offset);
    *virt_addr = value;

    printf("Write 32-bit value 0x%08x at USER address 0x%lx (%p), device=%s\n",
           value, target, (void *)virt_addr, device);

    if (munmap(map_base, map_size) != 0) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int bar_read_user(uint64_t address, uint32_t *value)
{
    const char *device;
    int fd;
    long page_size;
    uint64_t target;
    uint64_t target_aligned;
    uint64_t page_offset;
    size_t map_size;
    char *map_base;
    volatile uint32_t *virt_addr;

    if (value == NULL) {
        fprintf(stderr, "bar_read_user: value pointer is NULL\n");
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

    target = address;
    target_aligned = target & ~((uint64_t)page_size - 1ULL);
    page_offset = target - target_aligned;
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
                target, strerror(errno));
        close(fd);
        return -1;
    }

    virt_addr = (volatile uint32_t *)(map_base + page_offset);
    *value = *virt_addr;

    printf("Read 32-bit value 0x%08x at USER address 0x%lx (%p), device=%s\n",
           *value, target, (void *)virt_addr, device);

    if (munmap(map_base, map_size) != 0) {
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

int dma_to_device(const char *device,
                  uint64_t address,
                  uint64_t aperture,
                  uint64_t size,
                  uint64_t offset,
                  uint64_t count,
                  const char *infname,
                  const char *ofname)
{
    if (device == NULL) {
        return -1;
    }

    return dma_to_device_test_dma((char *)device, address, aperture, size, offset, count,
                                  (char *)infname, (char *)ofname);
}

int dma_from_device(const char *device,
                    uint64_t address,
                    uint64_t aperture,
                    uint64_t size,
                    uint64_t offset,
                    uint64_t count,
                    const char *ofname)
{
    if (device == NULL) {
        return -1;
    }

    return dma_from_device_test_dma((char *)device, address, aperture, size, offset, count,
                                    (char *)ofname);
}

int dma_write(const char *file,
              uint64_t address,
              uint64_t aperture,
              uint64_t size,
              uint64_t offset,
              uint64_t count,
              const char *ofname,
              pcie_dev_type_t type)
{
    const char *device;

    device = get_pcie_device(type);
    if (device == NULL) {
        fprintf(stderr, "failed to find device for type %d\n", type);
        return -1;
    }

    switch (type) {
    case PCIE_DEV_H2C:
        return dma_to_device(device, address, aperture, size, offset, count, file, ofname);

    default:
        fprintf(stderr, "dma_write only supports H2C/C2H\n");
        return -2;
    }
}

int dma_read(const char *file,
             uint64_t address,
             uint64_t aperture,
             uint64_t size,
             uint64_t offset,
             uint64_t count,
             const char *ofname,
             pcie_dev_type_t type)
{
    const char *device;
    (void)file;


    device = get_pcie_device(type);
    if (device == NULL) {
        fprintf(stderr, "failed to find device for type %d\n", type);
        return -1;
    }

    switch (type) 
    {
    case PCIE_DEV_C2H:
        return dma_from_device(device, address, aperture, size, offset, count, ofname);

    default:
        fprintf(stderr, "dma_read only supports C2H\n");
        return -2;
    }
}

int main(void)
{
    // pcie_dev_type_t type = PCIE_DEV_H2C;        // dma_write
    /*pcie_dev_type_t type = PCIE_DEV_C2H;*/    // dma_read
    pcie_dev_type_t type = PCIE_DEV_USER;   // User write/read
    

    const char *file = NULL;
    const char *ofname = NULL;
    uint64_t address = 0;
    uint64_t aperture = 0;

    uint64_t size = 0;
    uint64_t offset = 0;
    uint64_t count = 0;
    int ret = -1;

#if Mode == 1
    if (type == PCIE_DEV_H2C)
    {
        file = "./data.hex";
        address = 0x50000000;    // range: 0x5000_0000 ~ 0x7000_0000
        aperture = 0;
        size = 10;
        offset = 0;
        count = 1;
        ofname = NULL;

        ret = dma_write(file, address, aperture, size, offset, count, ofname, PCIE_DEV_H2C);
    }
    else if (type == PCIE_DEV_USER)
    {
        file = "./data.hex";
        address = 0x00000000;   // Fixed: 0x4000_0000
        uint32_t value = 0x00000001;

        ret = bar_write_user(address, value); 
        printf("%d", ret);
    }


    
#else
    if (type == PCIE_DEV_C2H)
    {
        file = NULL;
        address = 0x50000000;
        aperture = 0;
        size = 10;
        offset = 0;
        count = 1;
        ofname = "./out.bin";

        ret = dma_read(file, address, aperture, size, offset, count, ofname, PCIE_DEV_C2H);
    }
    else if (type == PCIE_DEV_USER)
    {
        address = 0x40000000;
        uint32_t value = 0x00000001;

        ret = bar_read_user(address, value);
    }

    
#endif

    if (ret != 0) {
        fprintf(stderr, "operation failed: ret=%d\n", ret);
        return 1;
    }

    return 0;
}