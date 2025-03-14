# ppl2025-gosuzuki-experiments
An experiment artifact for a PPL 2025 poster (C3) "組込みデバイスのメモリ保護ユニットを活用したメモリ管理手法に向けた事前評価".

```
Go Suzuki, Takuo Watanabe, Sosuke Moriguchi. 組込みデバイスのメモリ保護ユニットを活用したメモリ管理手法に向けた事前評価, 日本ソフトウェア科学会第27回プログラミングおよびプログラミング言語ワークショップ(PPL 2025), Mar. 2025. 
```

# Structure
**result.ods** is a result data of this experiment.

# Compilation
Build and Run with the standard ESP-IDF compilation command.
```sh
> idf.py flash
```

Please uncomment related measurement do_bench call sites for each group measurements.
## Group 0
This includes:
 * main SRAM only
 * LP SRAM only
 * table if barrier
 * table if barrier with propagation
 * table if barrier(if is inlined)
 * table if barrier(if is inlined) with propagation
 * tree if barrier
 * tree if barrier with propagation
 * tree if barrier(if is inlined)
 * tree if barrier(if is inlined) with propagation
 * table memory protection unit
 * table memory protection unit with propagation (if)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 0.  

## Group 1
This includes:
 * tree memory protection unit
 * tree memory protection unit with propagation (if)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 1.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 0.  


## Group 2
This includes:
 * table memory protection unit with propagation (hint)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 1.  

## Group 3
This includes:
 * tree memory protection unit with propagation (hint)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 1.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 1.  

## Group 4
This includes:
 * (Already copied) table if barrier
 * (Already copied) table if barrier with propagation
 * (Already copied) table if barrier(if is inlined)
 * (Already copied) table if barrier(if is inlined) with propagation
 * (Already copied) table memory protection unit
 * (Already copied) table memory protection unit with propagation (if)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 1.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 0.  

## Group 5
This includes:
 * (Already copied) table memory protection unit with propagation (hint)

`NO_COPY_ON_READ` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 1.  
`IS_TREE` in [components/lpsram_protection/buffering.c](components/lpsram_protection/buffering.c) is 0.  
`ENABLE_HINT` in [components/lpsram_protection/panic_handler.c](components/lpsram_protection/panic_handler.c) is 1.  


# Environment
## ESP32-C6
We used ESP32-C6-devkitC-1 N8, v1.3.

## ESP-IDF
We used [ESP-IDF SDK](https://github.com/espressif/esp-idf) with a commit hash `1160a86ba0b87b0ea68ad6b91eb10fe9c34ad692`;
We modified as below:
```
diff --git a/components/esp_driver_uart/src/uart_wakeup.c b/components/esp_driver_uart/src/uart_wakeup.c
index deb8e2b89c..555ff49640 100644
--- a/components/esp_driver_uart/src/uart_wakeup.c
+++ b/components/esp_driver_uart/src/uart_wakeup.c
@@ -10,7 +10,7 @@
  * This file is shared between the HP and LP cores.
  * Updates to this file should be made carefully and should not include FreeRTOS APIs or other IDF-specific functionalities, such as the interrupt allocator.
  */
-
+#if 0
 #include "driver/uart_wakeup.h"
 #include "hal/uart_hal.h"
 
@@ -89,3 +89,5 @@ esp_err_t uart_wakeup_setup(uart_port_t uart_num, const uart_wakeup_cfg_t *cfg)
 
     return ESP_ERR_INVALID_ARG;
 }
+
+#endif
diff --git a/components/esp_hw_support/esp_clk.c b/components/esp_hw_support/esp_clk.c
index 2ebd827631..73e104d437 100644
--- a/components/esp_hw_support/esp_clk.c
+++ b/components/esp_hw_support/esp_clk.c
@@ -52,7 +52,6 @@ static uint32_t calc_checksum(void)
 {
     uint32_t checksum = 0;
     uint32_t *data = (uint32_t*) &s_rtc_timer_retain_mem;
-
     for (uint32_t i = 0; i < (sizeof(retain_mem_t) - sizeof(s_rtc_timer_retain_mem.checksum)) / 4; i++) {
         checksum = ((checksum << 5) - checksum) ^ data[i];
     }
diff --git a/components/esp_system/port/panic_handler.c b/components/esp_system/port/panic_handler.c
index 4570ae00e5..6dddce2f6d 100644
--- a/components/esp_system/port/panic_handler.c
+++ b/components/esp_system/port/panic_handler.c
@@ -240,7 +240,7 @@ static void IRAM_ATTR panic_enable_cache(void)
 }
 #endif
 
-void IRAM_ATTR panicHandler(void *frame)
+void __attribute__((weak)) IRAM_ATTR panicHandler(void *frame)
 {
 #if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
     panic_enable_cache();
@@ -252,7 +252,7 @@ void IRAM_ATTR panicHandler(void *frame)
     panic_handler(frame, true);
 }
 
-void IRAM_ATTR xt_unhandled_exception(void *frame)
+void __attribute__((weak)) IRAM_ATTR xt_unhandled_exception(void *frame)
 {
 #if !CONFIG_APP_BUILD_TYPE_PURE_RAM_APP
     panic_enable_cache();
diff --git a/components/esp_tee/subproject/main/arch/riscv/esp_tee_vectors.S b/components/esp_tee/subproject/main/arch/riscv/esp_tee_vectors.S
index aded4c17a5..605a153efe 100644
--- a/components/esp_tee/subproject/main/arch/riscv/esp_tee_vectors.S
+++ b/components/esp_tee/subproject/main/arch/riscv/esp_tee_vectors.S
@@ -172,6 +172,7 @@ _s_sp:
     /* Exception handler. */
     .global _panic_handler
     .type _panic_handler, @function
+    .weak _panic_handler
 _panic_handler:
     /* Backup t0 on the stack before using it */
     addi    sp, sp, -16
diff --git a/components/esp_tee/subproject/main/common/panic/esp_tee_panic.c b/components/esp_tee/subproject/main/common/panic/esp_tee_panic.c
index 1aea490a07..4e1cfcdb6e 100644
--- a/components/esp_tee/subproject/main/common/panic/esp_tee_panic.c
+++ b/components/esp_tee/subproject/main/common/panic/esp_tee_panic.c
@@ -90,12 +90,12 @@ static void panic_handler(void *frame, bool pseudo_exccause)
     ESP_INFINITE_LOOP();
 }
 
-void tee_panic_from_exc(void *frame)
+__attribute__((weak)) void tee_panic_from_exc(void *frame)
 {
     panic_handler(frame, false);
 }
 
-void tee_panic_from_isr(void *frame)
+__attribute__((weak)) void tee_panic_from_isr(void *frame)
 {
     panic_handler(frame, true);
 }
diff --git a/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_apm_prot_cfg.c b/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_apm_prot_cfg.c
index 3c027c0693..b52312b655 100644
--- a/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_apm_prot_cfg.c
+++ b/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_apm_prot_cfg.c
@@ -310,7 +310,7 @@ void esp_tee_configure_apm_protection(void)
     /* LP APM0 interrupt configuration. */
     esp_tee_apm_int_enable(&lp_apm0_sec_mode_data);
     ESP_LOGD(TAG, "[REE0] LP_APM0 configured");
-
+#if 0
     /* LP APM TEE configuration. */
     lp_apm_sec_mode_data_tee.regn_count = sizeof(lp_apm_pms_data_tee) / sizeof(apm_ctrl_region_config_data_t);
     apm_hal_apm_ctrl_master_sec_mode_config(&lp_apm_sec_mode_data_tee);
@@ -322,7 +322,7 @@ void esp_tee_configure_apm_protection(void)
     /* LP APM interrupt configuration. */
     esp_tee_apm_int_enable(&lp_apm_sec_mode_data);
     ESP_LOGD(TAG, "[REE0] LP_APM configured");
-
+#endif
     /* HP APM TEE configuration. */
     hp_apm_sec_mode_data_tee.regn_count = sizeof(hp_apm_pms_data_tee) / sizeof(apm_ctrl_region_config_data_t);
     apm_hal_apm_ctrl_master_sec_mode_config(&hp_apm_sec_mode_data_tee);
diff --git a/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_pmp_pma_prot_cfg.c b/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_pmp_pma_prot_cfg.c
index af6cf5e20b..cb59f36f82 100644
--- a/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_pmp_pma_prot_cfg.c
+++ b/components/esp_tee/subproject/main/soc/esp32c6/esp_tee_pmp_pma_prot_cfg.c
@@ -86,11 +86,11 @@ void esp_tee_configure_region_protection(void)
      * 4) Entries are grouped in order with some static asserts to try and verify everything is
      * correct.
      */
-    const unsigned NONE    = PMP_L;
-    const unsigned R       = PMP_L | PMP_R;
-    const unsigned RW      = PMP_L | PMP_R | PMP_W;
-    const unsigned RX      = PMP_L | PMP_R | PMP_X;
-    const unsigned RWX     = PMP_L | PMP_R | PMP_W | PMP_X;
+    const unsigned NONE    = 0; // PMP_L;
+    const unsigned R       = PMP_R; // PMP_L | PMP_R;
+    const unsigned RW      = PMP_R | PMP_W; // PMP_L | PMP_R | PMP_W;
+    const unsigned RX      = PMP_R | PMP_X; // PMP_L | PMP_R | PMP_X;
+    const unsigned RWX     = PMP_R | PMP_W | PMP_X; // PMP_L | PMP_R | PMP_W | PMP_X;
 
     //
     // Configure all the invalid address regions using PMA
diff --git a/components/ulp/CMakeLists.txt b/components/ulp/CMakeLists.txt
index eea6ea5126..00f61fe1b8 100644
--- a/components/ulp/CMakeLists.txt
+++ b/components/ulp/CMakeLists.txt
@@ -56,9 +56,9 @@ if(CONFIG_ULP_COPROC_TYPE_LP_CORE)
         "lp_core/shared/ulp_lp_core_critical_section_shared.c")
 
     if(CONFIG_SOC_ULP_LP_UART_SUPPORTED)
-        list(APPEND srcs
-        "lp_core/lp_core_uart.c"
-        "lp_core/shared/ulp_lp_core_lp_uart_shared.c")
+#        list(APPEND srcs
+#        "lp_core/lp_core_uart.c"
+#        "lp_core/shared/ulp_lp_core_lp_uart_shared.c")
     endif()
 
     if(CONFIG_SOC_LP_I2C_SUPPORTED)
diff --git a/components/ulp/lp_core/lp_core/vector.S b/components/ulp/lp_core/lp_core/vector.S
index b62cbc9ec3..d7f6bfb661 100644
--- a/components/ulp/lp_core/lp_core/vector.S
+++ b/components/ulp/lp_core/lp_core/vector.S
@@ -96,6 +96,7 @@
 /* _panic_handler: handle all exception */
 	.section	.text.handlers,"ax"
 	.global _panic_handler
+        .weak _panic_handler
 	.type _panic_handler, @function
 _panic_handler:
     save_general_regs RV_STK_FRMSZ
@@ -129,6 +130,7 @@ _end:
 /* interrupt_handler: handle all interrupt */
     .section	.text.handlers,"ax"
     .global _interrupt_handler
+    .weak _interrupt_handler
     .type _interrupt_handler, @function
 _interrupt_handler:
 	/* save registers & mepc to stack */
diff --git a/components/ulp/lp_core/shared/ulp_lp_core_lp_uart_shared.c b/components/ulp/lp_core/shared/ulp_lp_core_lp_uart_shared.c
index 57faac9a7d..67ebf894f2 100644
--- a/components/ulp/lp_core/shared/ulp_lp_core_lp_uart_shared.c
+++ b/components/ulp/lp_core/shared/ulp_lp_core_lp_uart_shared.c
@@ -3,6 +3,7 @@
  *
  * SPDX-License-Identifier: Apache-2.0
  */
+#if 0
 #include "ulp_lp_core_lp_uart_shared.h"
 #include "hal/uart_hal.h"
 
@@ -22,3 +23,4 @@ void lp_core_uart_clear_buf(void)
     uart_ll_rxfifo_rst(hal.dev);
     uart_ll_txfifo_rst(hal.dev);
 }
+#endif
```


