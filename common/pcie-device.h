#ifndef PCIE_DEVICE_H
#define PCIE_DEVICE_H

typedef enum {
    PCIE_DEV_H2C = 0,
    PCIE_DEV_C2H,
    PCIE_DEV_USER
} pcie_dev_type_t;

const char *get_pcie_device(pcie_dev_type_t type);

#endif