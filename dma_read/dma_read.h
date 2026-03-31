#ifndef DMA_READ_H
#define DMA_READ_H

#include <stdint.h>

int dma_read_run(const char *ofname,
                 uint64_t address,
                 uint64_t aperture,
                 uint64_t size,
                 uint64_t offset,
                 uint64_t count);

#endif