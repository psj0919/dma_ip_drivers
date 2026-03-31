#ifndef DMA_WRITE_READ_H
#define DMA_WRITE_READ_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

typedef enum {
    PCIE_DEV_H2C = 0,
    PCIE_DEV_C2H,
    PCIE_DEV_USER,
    PCIE_DEV_CONTROL,
} pcie_dev_type_t;

const char *get_pcie_device(pcie_dev_type_t type);

int dma_to_device(const char *device,
                  uint64_t address,
                  uint64_t aperture,
                  uint64_t size,
                  uint64_t offset,
                  uint64_t count,
                  const char *infname,
                  const char *ofname);

int dma_from_device(const char *device,
                    uint64_t address,
                    uint64_t aperture,
                    uint64_t size,
                    uint64_t offset,
                    uint64_t count,
                    const char *ofname);

int dma_write(const char *file,
              uint64_t address,
              uint64_t aperture,
              uint64_t size,
              uint64_t offset,
              uint64_t count,
              const char *ofname,
              pcie_dev_type_t type);

int dma_read(const char *file,
             uint64_t address,
             uint64_t aperture,
             uint64_t size,
             uint64_t offset,
             uint64_t count,
             const char *ofname,
             pcie_dev_type_t type);

int bar_write_user(uint64_t address, uint32_t value);
int bar_read_user(uint64_t address, uint32_t *value);

extern int verbose;
extern int eop_flush;

ssize_t read_to_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
ssize_t write_from_buffer(const char *fname, int fd, char *buffer, uint64_t size, uint64_t base);
void timespec_sub(struct timespec *t1, struct timespec *t2);

#endif