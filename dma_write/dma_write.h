#ifndef DMA_WRITE_H
#define DMA_WRITE_H

#include <stdint.h>
#include "pcie_device.h"

int dma_write_run(const char *file,
                  uint64_t address,
                  uint64_t aperture,
                  uint64_t size,
                  uint64_t offset,
                  uint64_t count,
                  const char *ofname);

#endif