#include <stdint.h>
#include <stdio.h>

#include "dma_write/dma_write.h"
#include "dma_read/dma_read.h"
#include "bar_user/bar_user.h"

int main(void)
{
    int ret;
    uint32_t value;

    /* H2C write test */
    
    ret = dma_write_run("./data.hex", 0x50000000, 0, 10, 0, 1, NULL);
    printf("dma_write_run ret = %d\n", ret);
    

    /* C2H read test */
    
    ret = dma_read_run("./out.bin", 0x50000000, 0, 10, 0, 1);
    printf("dma_read_run ret = %d\n", ret);


    /* USER write test */
    value = 0x00000001;
    ret = bar_user_access(BAR_USER_WRITE, 'w', 0x0, &value);
    printf("bar_user_access(write) ret = %d\n", ret);

    /* USER read test */
    value = 0;
    ret = bar_user_access(BAR_USER_READ, 'w', 0x0, &value);
    printf("bar_user_access(read) ret = %d, value = 0x%08x\n", ret, value);

    return 0;
}