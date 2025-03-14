#include "esp_cpu.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_tee.h"
void this_is_for_alive(void);
void * hoge;

esp_err_t _ss_show_protection_info(void)
{
    hoge = this_is_for_alive;
    if (esp_cpu_get_curr_privilege_level() != ESP_CPU_S_MODE) {
        esp_rom_printf("Operation executing from illegal privilege level!\n");
        return ESP_ERR_INVALID_STATE;
    }

    esp_rom_printf("TEE: Hello\n");
    long ms = RV_READ_CSR(mstatus);
    esp_rom_printf("mstatus = %lx\n", ms);
    esp_rom_printf("dumping protection\n");
    esp_rom_printf("PMPADDR0 = %lx\n", RV_READ_CSR(CSR_PMPADDR0) << 2);
    esp_rom_printf("PMPADDR1 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 1) << 2);
    esp_rom_printf("PMPADDR2 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 2) << 2);
    esp_rom_printf("PMPADDR3 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 3) << 2);
    esp_rom_printf("PMPADDR4 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 4) << 2);
    esp_rom_printf("PMPADDR5 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 5) << 2);
    esp_rom_printf("PMPADDR6 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 6) << 2);
    esp_rom_printf("PMPADDR7 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 7) << 2);
    esp_rom_printf("PMPADDR8 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 8) << 2);
    esp_rom_printf("PMPADDR9 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 9) << 2);
    esp_rom_printf("PMPADDR10 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 10) << 2);
    esp_rom_printf("PMPADDR11 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 11) << 2);
    esp_rom_printf("PMPADDR12 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 12) << 2);
    esp_rom_printf("PMPADDR13 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 13) << 2);
    esp_rom_printf("PMPADDR14 = %lx\n", RV_READ_CSR(CSR_PMPADDR0 + 14) << 2);
    esp_rom_printf("PMPCFG0..3: %ld\n", RV_READ_CSR(CSR_PMPCFG0));
    esp_rom_printf("PMPCFG4..7: %ld\n", RV_READ_CSR(CSR_PMPCFG0 + 1));
    esp_rom_printf("PMPCFG8..11: %ld\n", RV_READ_CSR(CSR_PMPCFG0 + 2));
    esp_rom_printf("PMPCFG12..15: %ld\n", RV_READ_CSR(CSR_PMPCFG0 + 3));
    return ESP_OK;
}

esp_err_t _ss_protect_lpsram(int protect_enable) {
    PMP_ENTRY_CFG_RESET(9);
    PMP_ENTRY_CFG_SET(9, PMP_NAPOT | (protect_enable ?  0 : (PMP_R | PMP_W | PMP_X)));
    return ESP_OK;
}

void reset_buffering(int st, void * buf_ulp, void * buf_begin, int * buf_i);
esp_err_t _ss_reset_buffering(int st, void * buf_ulp, void * buf_begin, int * buf_i) {
    // esp_rom_printf("buf_ulp = %lx, buf_main = %lx\n", (long)buf_ulp, (long)buf_begin);
    reset_buffering(st, buf_ulp, buf_begin, buf_i);
    return ESP_OK;
}