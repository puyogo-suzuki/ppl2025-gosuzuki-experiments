#include "esp_err.h"
esp_err_t show_protection_info(void);
esp_err_t protect_lpsram(int protect_enable);
esp_err_t reset_buffering(int state, void * buf_ulp, void * buf_begin, int * buf_i);