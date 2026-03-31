#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>

#include "xdma/cdev_sgdma.h"
#include "pcie-device.h"
#include "dma_write.h"

extern int verbose;
extern ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern ssize_t write_from_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
extern void timespec_sub(struct timespec *t1, struct timespec *t2);

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
        perror("open device");
        return -EINVAL;
    }

    if (infname) {
        infile_fd = open(infname, O_RDONLY);
        if (infile_fd < 0) {
            perror("open input file");
            rc = -EINVAL;
            goto out;
        }
    }

    if (ofname) {
        outfile_fd = open(ofname, O_RDWR | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (outfile_fd < 0) {
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
                rc = -EIO;
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
            underflow = 1;
        }

        timespec_sub(&ts_end, &ts_start);
        total_time += ts_end.tv_sec * 1000000000L + ts_end.tv_nsec;

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
        printf("%s ** Average BW = %lu, %f\n", devname, size, result);
    }

out:
    close(fpga_fd);
    if (infile_fd >= 0) close(infile_fd);
    if (outfile_fd >= 0) close(outfile_fd);
    free(allocated);

    if (rc < 0) return (int)rc;
    return underflow ? -EIO : 0;
}

int dma_write_run(const char *file,
                  uint64_t address,
                  uint64_t aperture,
                  uint64_t size,
                  uint64_t offset,
                  uint64_t count,
                  const char *ofname)
{
    const char *device = get_pcie_device(PCIE_DEV_H2C);
    if (device == NULL) {
        fprintf(stderr, "failed to find H2C device\n");
        return -1;
    }

    return dma_to_device_test_dma((char *)device, address, aperture, size, offset, count,
                                  (char *)file, (char *)ofname);
}