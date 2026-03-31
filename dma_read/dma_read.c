#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "xdma/cdev_sgdma.h"
#include "../common/pcie_device.h"
#include "dma_read.h"

extern int verbose;
extern int eop_flush;
extern ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern ssize_t write_from_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern void timespec_sub(struct timespec *t1, struct timespec *t2);

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

    fpga_fd = open(devname, eop_flush ? (O_RDWR | O_TRUNC) : O_RDWR);
    if (fpga_fd < 0) {
        perror("open device");
        return -EINVAL;
    }

    if (ofname) {
        out_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (out_fd < 0) {
            perror("open output file");
            rc = -EINVAL;
            goto out;
        }
    }

    posix_memalign((void **)&allocated, 4096, size + 4096);
    if (!allocated) {
        rc = -ENOMEM;
        goto out;
    }

    buffer = allocated + offset;

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
                rc = -EIO;
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
            underflow = 1;
        }

        timespec_sub(&ts_end, &ts_start);
        total_time += ts_end.tv_sec * 1000000000L + ts_end.tv_nsec;

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
        printf("%s ** Average BW = %lu, %f\n", devname, size, result);
        rc = 0;
    } else if (eop_flush) {
        rc = 0;
    } else {
        rc = -EIO;
    }

out:
    close(fpga_fd);
    if (out_fd >= 0) close(out_fd);
    free(allocated);
    return (int)rc;
}

int dma_read_run(const char *ofname,
                 uint64_t address,
                 uint64_t aperture,
                 uint64_t size,
                 uint64_t offset,
                 uint64_t count)
{
    const char *device = get_pcie_device(PCIE_DEV_C2H);
    if (device == NULL) {
        fprintf(stderr, "failed to find C2H device\n");
        return -1;
    }

    return dma_from_device_test_dma((char *)device, address, aperture, size, offset, count,
                                    (char *)ofname);
}