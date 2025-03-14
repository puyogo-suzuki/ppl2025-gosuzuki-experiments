/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>

#include "esp_cpu.h"
#include "esp_attr.h"
#include "esp_macros.h"
#include "esp_rom_sys.h"
#include "esp_rom_uart.h"
#include "rom/cache.h"

#include "riscv/rv_utils.h"
#include "riscv/rvruntime-frames.h"

#include "hal/apm_hal.h"

#include "esp_tee.h"

#define RV_FUNC_STK_SZ    (32)

#define tee_panic_print(format, ...) esp_rom_printf(DRAM_STR(format), ##__VA_ARGS__)
FORCE_INLINE_ATTR void rv_utils_tee_intr_global_disable(void)
{
    RV_CLEAR_CSR(ustatus, USTATUS_UIE);
    RV_CLEAR_CSR(mstatus, MSTATUS_UIE);
    RV_CLEAR_CSR(mstatus, MSTATUS_MIE);
}
static void tee_panic_end(void)
{
    // Disable interrupts
    rv_utils_tee_intr_global_disable();

    // Disable the cache
    Cache_Disable_ICache();

    // Clear the interrupt controller configurations
    memset((void *)DR_REG_PLIC_MX_BASE, 0x00, (PLIC_MXINT_CLAIM_REG + 4 - DR_REG_PLIC_MX_BASE));
    memset((void *)DR_REG_PLIC_UX_BASE, 0x00, (PLIC_UXINT_CLAIM_REG + 4 - DR_REG_PLIC_UX_BASE));

    // Make sure all the panic handler output is sent from UART FIFO
    if (CONFIG_ESP_CONSOLE_UART_NUM >= 0) {
        esp_rom_output_tx_wait_idle(CONFIG_ESP_CONSOLE_UART_NUM);
    }

    // Generate system reset
    esp_rom_software_reset_system();
}

void abort(void);

extern void panic_print_isrcause(const void *, int);
extern void panic_print_exccause(const void *, int);
extern void panic_print_registers(const void *, int);
extern void panic_print_backtrace(const void *, int);
static void panic_handler(void *frame, bool pseudo_exccause)
{
    int fault_core = esp_cpu_get_core_id();
    tee_panic_print("\n=================================================\n");
    tee_panic_print("Secure exception occurred on Core %d\n", fault_core);
    if (pseudo_exccause) {
        panic_print_isrcause((const void *)frame, fault_core);
    } else {
        panic_print_exccause((const void *)frame, fault_core);
    }
    tee_panic_print("=================================================\n");

    panic_print_registers((const void *)frame, fault_core);
    tee_panic_print("\n");
    panic_print_backtrace((const void *)frame, 100);
    tee_panic_print("\n");
    tee_panic_print("Rebooting...\r\n\n");

    tee_panic_end();
    ESP_INFINITE_LOOP();
}

#define ENABLE_HINT 1

struct decode_inst_ret_t {
    long base_reg_num;
    void * imm;
#if ENABLE_HINT
    int is_compressed;
#endif
};
struct decode_inst_ret_t decode_inst(uint32_t instVal) {
    long rs1 = 0, imm = 0;
#if ENABLE_HINT
    int is_compressed = 1;
#endif
    switch(instVal & 0x7F) {
    case 3: // Load
        rs1 = (instVal >> 15) & 0x1F;
        imm = (instVal >> 20);
        break;
    case 0x23: // Store
        rs1 = (instVal >> 15) & 0x1F;
        imm = ((instVal >> 25) << 5) + ((instVal >> 7) & 0x1F);
        break;
    default:
        if((instVal & 3) == 0) {
            if((instVal & 0xE000) == 0) goto fail; // c.addi4spn or illegal
            long imm2 = (instVal >> 6) & 1;
            long imm6 = (instVal >> 5) & 1;
            long imm543 = (instVal >> 10) & 7;
            // or.i is not available on compressed inst.
            rs1 = 8 + ((instVal >> 7) & 7);
            imm = (imm2 << 2) + (imm6 << 6) + (imm543 << 3);
#if ENABLE_HINT
            is_compressed = 1;
#endif
        }
        break;
    }
    fail:
    return (struct decode_inst_ret_t){.base_reg_num = rs1, .imm = (void *)imm
#if ENABLE_HINT
        , .is_compressed = is_compressed
#endif
    };
}
void * copy_buffer(void * from);
void tee_panic_from_exc(void *frame)
{
    RvExcFrame * excframe = (RvExcFrame *)frame;
    // esp_rom_printf("HI mcause=%ld, a0=%lx!\n", excframe->mcause, excframe->a0);
    if(excframe->mcause == 5 /* Load access fault */ || excframe->mcause == 7 /* Store access fault */)
    {
        uint32_t inst = *((uint32_t *)(excframe->mepc));
        struct decode_inst_ret_t decoded = decode_inst(inst);
        if(decoded.base_reg_num == 0) goto fail; // It's absolute address, certainly.
        void * computed = (void *)(((long *)frame)[decoded.base_reg_num] + decoded.imm);
        void * cb = copy_buffer(computed);
        // esp_rom_printf("copied=%lx, computed=%lx, decoded.base_reg_num=%ld, decoded.imm=%lx, exframe->mepc=%lx!\n", cb, computed, decoded.base_reg_num, decoded.imm, excframe->mepc );
        if(cb == NULL) goto fail;
        long rs1 = (long)cb - (long)decoded.imm;
        ((long *)frame)[decoded.base_reg_num] = rs1;
#if ENABLE_HINT
        {
            uint32_t hintinst = *((uint32_t *)((long)excframe->mepc + (decoded.is_compressed ? 2 : 4)));
            if((hintinst & 0x7FFF) == 0x2013) {
                uint32_t orig = (hintinst >> 15) & 0x1F;
                *(((long **)frame)[orig]) = (long)rs1;
            }
        }
#endif
        return;
    }
    fail:
    // esp_rom_printf("panic_handler!\n");
    panic_handler(frame, false);
}

void tee_panic_from_isr(void *frame){ panic_handler(frame, true); }
extern void _imalive(void);
void this_is_for_alive(void){ tee_panic_print("ALIVE!"); _imalive();}

void load_store_error(void *frame)
{
    RvExcFrame * excframe = (RvExcFrame *)frame;
    // esp_rom_printf("HI mcause=%ld, a0=%lx!\n", excframe->mcause, excframe->a0);
    uint32_t inst = *((uint32_t *)(excframe->mepc));
    struct decode_inst_ret_t decoded = decode_inst(inst);
    if(decoded.base_reg_num == 0) goto fail; // It's absolute address, certainly.
    void * computed = (void *)(((long *)frame)[decoded.base_reg_num] + decoded.imm);
    void * cb = copy_buffer(computed);
    // esp_rom_printf("copied=%lx, computed=%lx, decoded.base_reg_num=%ld, decoded.imm=%lx, exframe->mepc=%lx!\n", cb, computed, decoded.base_reg_num, decoded.imm, excframe->mepc );
    if(cb == NULL) goto fail;
    long rs1 = (long)cb - (long)decoded.imm;
    ((long *)frame)[decoded.base_reg_num] = rs1;
#if ENABLE_HINT
    {
        uint32_t hintinst = *((uint32_t *)((long)excframe->mepc + (decoded.is_compressed ? 2 : 4)));
        if((hintinst & 0x7FFF) == 0x2013) {
            uint32_t orig = (hintinst >> 15) & 0x1F;
            *(((long **)frame)[orig]) = (long)rs1;
        }
    }
#endif
    return;
    fail:
    // esp_rom_printf("panic_handler!\n");
    panic_handler(frame, false);
}
