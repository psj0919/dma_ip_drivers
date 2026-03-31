#include <stdio.h>
#include <unistd.h>
#include "pcie-device.h"

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