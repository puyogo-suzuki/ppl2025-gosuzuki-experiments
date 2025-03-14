// #include "ulp_lp_core.h"
#include "ulp_lp_core_utils.h"
#include "ulp_lp_core_gpio.h"
char buf[8192];
int buf_idx = 0;

void main(void)
{
    buf[0] = 0;
    buf_idx = 0;
    int v = 0;
    while(1) {
        ulp_lp_core_delay_us(1000000);
        ulp_lp_core_gpio_set_level(4, v);
        v = !v;
    }
}