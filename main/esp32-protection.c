#include <stdio.h>
#include "esp_intr_alloc.h"
#include "riscv/csr.h"
#include "include/xorshift32.h"
#include "ulp_lp_core.h"
#include "driver/rtc_io.h"

#include "esp_tee.h"
#include "esp_timer.h"
#include "secure_service_num.h"

struct tree_t {
    struct tree_t * l;
    struct tree_t * r;
    long val;
    long val2;
};

struct tree_t buf[8192 / sizeof(struct tree_t)];
int buf_idx = 0;
struct tree_t * allocate_main(void) {
    if(buf_idx >= (sizeof(buf) / sizeof(buf[0])))
        return NULL;
    int i = buf_idx;
    buf_idx++;
    return &(buf[i]);
}

extern struct tree_t ulp_buf[8192 / sizeof(struct tree_t)];
extern int ulp_buf_idx;
struct tree_t * allocate_ulp(void) {
    if(ulp_buf_idx >= (sizeof(ulp_buf) / sizeof(ulp_buf[0])))
        return NULL;
    int i = ulp_buf_idx;
    ulp_buf_idx++;
    return &(ulp_buf[i]);
}

struct tree_t * append(struct tree_t ** _t, long v, struct tree_t * (allocator)(void)) {
    struct tree_t * t = *_t;
    while(t != NULL){
        if(t->val == v) return t; // not added.
        _t = t->val > v ? &(t->l) : &(t->r);
        t = *_t;
    }
    t = allocator();
    t->l = NULL; t->r = NULL;
    t->val = v;
    *_t = t;
    return t;
}

int64_t do_bench(uint32_t seed, int count, void (initializer)(void), void (initializer2)(void), struct tree_t * (allocator)(void), struct tree_t * (searcher)(struct tree_t *, long)) {
    struct xorshift32_state xs32 = {seed};
    struct tree_t * t = NULL;
    // long start_time, end_time;
    int64_t start_time, end_time;
    initializer();
    for(int i = 0; i < (8192 / sizeof(struct tree_t)); ++i) {
        uint32_t v = xorshift32(&xs32);
        append(&t, v, allocator);
    }
    initializer2();
    int hits = 0;
    start_time = esp_timer_get_time();
    // asm volatile ("csrw 0x7e0, 1");
    // asm volatile ("csrw 0x7e1, 1");
    // asm volatile ("csrr %0, 0x7e2" : "=r" (start_time));
    for(int i = 0; i < count; ++i) {
        uint32_t v = xorshift32(&xs32);
        if(searcher(t, (long)v)) hits++;
    }
    // asm volatile ("csrr %0, 0x7e2" : "=r" (end_time));
    // asm volatile ("csrw 0x7e1, 0");
    // asm volatile ("csrw 0x7e0, 0");
    end_time = esp_timer_get_time();
    int64_t ti = end_time - start_time;
    // printf("hits: %d\tcycles: %ld\n", hits, end_time - start_time);
#if 0 // LOG
    printf("hits: %d\ttime: %lld us\n", hits, ti);
#endif
    return ti;
}

struct tree_t * search(struct tree_t * t, long v) {
    for(;t != NULL && t->val != v; t = t->val > v ? t->l : t->r);
    return t;
}
struct tree_t * cache[8192 / sizeof(struct tree_t)];
#define IS_IN_MAIN(v) (((v) >= buf) && ((v) < &buf[sizeof(buf) / sizeof(struct tree_t)]))
#define read_barrier_inline(v) IS_IN_MAIN((v)) ? (v) : _read_barrier(v) 
static struct tree_t * _read_barrier(struct tree_t * v) {
    // if(IS_IN_MAIN(v)) return v;
    size_t idx = ((size_t)v - (size_t)ulp_buf) / sizeof(struct tree_t);
    struct tree_t * ret = cache[idx];
    if(ret) return ret;
    ret = allocate_main();
    cache[idx] = ret;
    ret->l = v->l;
    ret->r = v->r;
    ret->val = v->val;
    return ret;
}
static struct tree_t * read_barrier(struct tree_t * v) {
    if(IS_IN_MAIN(v)) return v;
    size_t idx = ((size_t)v - (size_t)ulp_buf) / sizeof(struct tree_t);
    struct tree_t * ret = cache[idx];
    if(ret) return ret;
    ret = allocate_main();
    cache[idx] = ret;
    ret->l = v->l;
    ret->r = v->r;
    ret->val = v->val;
    return ret;
}
#define SEARCH_CACHED(search_cached, read_barrier) struct tree_t * search_cached(struct tree_t * t, long v){ \
    while(t != NULL) { \
        struct tree_t * translated = read_barrier(t); \
        if(translated->val == v) return translated; \
        t = translated->val > v ? translated->l : translated->r; \
    } \
    return 0; \
}

#define SEARCH_CACHED_WITH_REWRITING(search_cached_with_rewriting, read_barrier) struct tree_t * search_cached_with_rewriting(struct tree_t * t, long v){ \
    struct tree_t ** orig = &t; \
    while(t != NULL) { \
        struct tree_t * translated = read_barrier(t); \
        if(t != translated) *orig = translated; \
        if(translated->val == v) return translated; \
        orig = translated->val > v ? &(translated->l) : &(translated->r); \
        t = *orig; \
    } \
    return 0; \
}

SEARCH_CACHED(search_cached, read_barrier)
SEARCH_CACHED_WITH_REWRITING(search_cached_with_rewriting, read_barrier)
SEARCH_CACHED(search_inline_cached, read_barrier_inline)
SEARCH_CACHED_WITH_REWRITING(search_inline_cached_with_rewriting, read_barrier_inline)

struct tree_t cache2[(8192 / sizeof(struct tree_t))];
struct tree_t * cache2_top;
int cache2_idx;
struct tree_t * allocate_cache_tree(void) {
    if(cache2_idx >= (sizeof(cache2) / sizeof(cache2[0])))
        return NULL;
    int i = cache2_idx;
    cache2_idx++;
    return &(cache2[i]);
}
#define read_barrier_tree_inline(v) IS_IN_MAIN((v)) ? (v) : _read_barrier_tree(v) 
#define read_barrier_tree_not_inline(v) read_barrier_tree(v) 
static struct tree_t * _read_barrier_tree(struct tree_t * v) {
    struct tree_t * translated = search(cache2_top, (long)v);
    if(translated) return (struct tree_t *)(translated->val2);
    translated = append(&cache2_top, (long)v, allocate_cache_tree);
    struct tree_t * ret = allocate_main();
    ret->l = v->l;
    ret->r = v->r;
    ret->val = v->val;
    translated->val2 = (long)ret;
    return ret;
}
static struct tree_t * read_barrier_tree(struct tree_t * v) {
    if(IS_IN_MAIN(v)) return v;
    struct tree_t * translated = search(cache2_top, (long)v);
    if(translated) return (struct tree_t *)(translated->val2);
    translated = append(&cache2_top, (long)v, allocate_cache_tree);
    struct tree_t * ret = allocate_main();
    ret->l = v->l;
    ret->r = v->r;
    ret->val = v->val;
    translated->val2 = (long)ret;
    return ret;
}
SEARCH_CACHED(search_cached_tree, read_barrier_tree)
SEARCH_CACHED_WITH_REWRITING(search_cached_tree_with_rewriting, read_barrier_tree)
SEARCH_CACHED(search_inline_cached_tree, read_barrier_tree_inline)
SEARCH_CACHED_WITH_REWRITING(search_inline_cached_tree_with_rewriting, read_barrier_tree_inline)

#define LRU_SIZE 32
struct tree_t * lru[LRU_SIZE * 2];
int lru_idx;
int hits = 0;
static struct tree_t * read_barrier_lru(struct tree_t * v) {
    if(IS_IN_MAIN(v)) return v;
    for(int i = 0; i < LRU_SIZE; ++i)
        if(lru[i*2] == v) {
            struct tree_t * swp = lru[i*2];
            lru[i*2] = lru[lru_idx];
            lru[lru_idx] = swp;
            swp = lru[i*2+1];
            lru[i*2+1] = lru[lru_idx+1];
            lru[lru_idx+1] = swp;
            lru_idx = (lru_idx+2) % (LRU_SIZE*2);
            return swp;
        }
    struct tree_t * translated = search(cache2_top, (long)v);
    if(translated) {
        struct tree_t * t = (struct tree_t *)(translated->val2);
        lru[lru_idx] = v;
        lru[lru_idx + 1] = t;
        lru_idx = (lru_idx+2) % (LRU_SIZE*2);
        return t;
    }
    translated = append(&cache2_top, (long)v, allocate_cache_tree);
    struct tree_t * ret = allocate_main();
    ret->l = v->l;
    ret->r = v->r;
    ret->val = v->val;
    translated->val2 = (long)ret;
    return ret;
}
SEARCH_CACHED(search_cached_lru, read_barrier_lru)
SEARCH_CACHED_WITH_REWRITING(search_cached_lru_with_rewriting, read_barrier_lru)

void init_main(void) {buf_idx = 0;}
void init_ulp(void) {init_main(); esp_tee_service_call(2, SS_PROTECT_LPSRAM, 0); ulp_buf_idx = 0;}
void init_cached(void) {init_ulp(); init_main(); for(int i = 0; i < 8192 / sizeof(struct tree_t); ++i) cache[i] = NULL;}
void init_cached_nocopy(void) {init_ulp(); init_main(); for(int i = 0; i < (8192 / sizeof(struct tree_t)); ++i) cache[i] = &buf[i];}
void init_cached_tree(void) {init_ulp(); init_main(); cache2_top = NULL; cache2_idx = 0;}
void init_cached_lru(void) {init_ulp(); init_main(); cache2_top = NULL; cache2_idx = 0; for(int i = 0; i < LRU_SIZE*2; ++i) lru[i] = NULL; lru_idx = 0; }

void init2_none(void) {}
void init2_cached_table(void) {esp_tee_service_call(2, SS_PROTECT_LPSRAM, 1);esp_tee_service_call(5, SS_RESET_BUFFERING, 0, ulp_buf, buf, &buf_idx);}
void init2_cached_tree(void) {esp_tee_service_call(2, SS_PROTECT_LPSRAM, 1);esp_tee_service_call(5, SS_RESET_BUFFERING, 1, ulp_buf, buf, &buf_idx);}
void init2_nocopy(void) {for(int i = 0; i < 8192 / sizeof(struct tree_t); ++i) buf[i] = ulp_buf[i];}
void init2_nocopy_mpu(void) {for(int i = 0; i < 8192 / sizeof(struct tree_t); ++i) buf[i] = ulp_buf[i]; esp_tee_service_call(2, SS_PROTECT_LPSRAM, 1); esp_tee_service_call(5, SS_RESET_BUFFERING, 1, ulp_buf, buf, &buf_idx);}

extern const uint8_t bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t bin_end[] asm("_binary_ulp_main_bin_end");

struct tree_t * search_writeback(struct tree_t * t, long v) {
    // a0 = t, a1 = v, a2 = orig, a3 t->val, a4 = tt
    asm volatile("addi sp, sp, -16");
    asm volatile("mv a2, sp");
    WHILE_HEAD:
    asm goto("beqz a0, %l[RET]":::: RET);
    asm volatile("mv a4, a0");
    asm volatile("lw a3, 8(a0)"); // t->val
    asm goto("beq a0, a4, %l[NE]":::: NE); // t != tt
    asm volatile("sw a0, 0(a2)");
    NE:
    asm goto("beq a1, a3, %l[RET]":::: RET); // t->val == v
    asm volatile("mv a2, a0");
    asm goto("bgt a3, a1, %l[WHILE_END]":::: WHILE_END);
    asm volatile("addi a2, a2, 4");
    WHILE_END:
    asm volatile("lw a0, 0(a2)");
    asm goto("j %l[WHILE_HEAD]":::: WHILE_HEAD);
    RET:
    asm volatile("addi sp, sp, 16");
    asm volatile("ret");
    /*
    struct tree_t * volatile * orig = (struct tree_t * volatile*)&t;
    while(t != NULL) {
        volatile struct tree_t * tt = t;
        long t_val = t->val;
        if(t != tt) *orig = (struct tree_t *)t;
        if(t_val == v) return (struct tree_t *)t;
        orig = tt->val > v ? &(tt->l) : &(tt->r);
        t = *orig;
    }*/
    return 0;
}
struct tree_t * search_writeback_hint(struct tree_t * t, long v) {
    // a0 = t, a1 = v, a2 = orig, a3 = t->val
    asm volatile("addi sp, sp, -16");
    asm volatile("mv a2, sp");
    WHILE_HEAD:
    asm goto("beqz a0, %l[RET]":::: RET);
    asm volatile("lw a3, 8(a0)"); // t->val
    asm volatile("slti zero, a2, 0");
    asm goto("beq a1, a3, %l[RET]":::: RET); // t->val == v
    asm volatile("mv a2, a0");
    asm goto("bgt a3, a1, %l[WHILE_END]":::: WHILE_END);
    asm volatile("addi a2, a2, 4");
    WHILE_END:
    asm volatile("lw a0, 0(a2)");
    asm goto("j %l[WHILE_HEAD]":::: WHILE_HEAD);
    RET:
    asm volatile("addi sp, sp, 16");
    asm volatile("ret");
    /*
    struct tree_t * volatile * orig = (struct tree_t * volatile*)&t;
    while(t != NULL) {
        if(t->val == v) return (struct tree_t *)t;
        orig = t->val > v ? &(t->l) : &(t->r);
        t = *orig;
    }*/
    return 0;
}

int copied;
int translated;
struct tree_t * search_investigate(struct tree_t * t, long v) {
    for(;t != NULL; t = t->val > v ? t->l : t->r) {
        if(t->val2) {
            translated++;
        } else {
            copied++;
            t->val2 = 1;
        }
        if(t->val == v) return t;
    }
    return 0;
}

void init_investigate(void) {buf_idx = 0; copied = 0; translated = 0; for(int * j = (int *)buf; j < (int*)((long)buf + sizeof(buf)); ++j) *j = 0; }

void app_main(void)
{
    printf("ulp_lp_core_load_binary: %d\n", ulp_lp_core_load_binary(bin_start,(bin_end-bin_start)));
    rtc_gpio_init(4);
    rtc_gpio_set_direction(4, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_pulldown_dis(4);
    rtc_gpio_pullup_dis(4);
    rtc_gpio_hold_dis(4);
    rtc_gpio_set_level(4, 0);
    ulp_lp_core_cfg_t cfg  = {
        .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_HP_CPU,
        .lp_timer_sleep_duration_us = 0
    };
    ulp_lp_core_run(&cfg);
    // printf("count, copied, translated, main, lpsram, table, table_inlined, table_wb, table_inlined_wb, tree, tree_wb, tree_inlined, tree_inlined_wb, mpu, mpu_wb\n");
    for(int j = 32; j < 8192*128; j*=2) {
        int i = 2;
        // printf("i = %d\n", i);
        int64_t result[1];
        do_bench(i, j, init_investigate, init2_none, allocate_main, search_investigate);
        // GROUP 0
        // result[0] = do_bench(i, j, init_main, init2_none, allocate_main, search);
        // result[1] = do_bench(i, j, init_ulp, init2_none, allocate_ulp, search);
        // result[2] = do_bench(i, j, init_cached, init2_none, allocate_ulp, search_cached);
        // result[3] = do_bench(i, j, init_cached, init2_none, allocate_ulp, search_cached_with_rewriting);
        // result[4] = do_bench(i, j, init_cached, init2_none, allocate_ulp, search_inline_cached);
        // result[5] = do_bench(i, j, init_cached, init2_none, allocate_ulp, search_inline_cached_with_rewriting);
        // result[6] = do_bench(i, j, init_cached_tree, init2_none, allocate_ulp, search_cached_tree);
        // result[7] = do_bench(i, j, init_cached_tree, init2_none, allocate_ulp, search_cached_tree_with_rewriting);
        // result[8] = do_bench(i, j, init_cached_tree, init2_none, allocate_ulp, search_inline_cached_tree);
        // result[9] = do_bench(i, j, init_cached_tree, init2_none, allocate_ulp, search_inline_cached_tree_with_rewriting);
        // result[10] = do_bench(i, j, init_ulp, init2_cached_table, allocate_ulp, search);
        // result[11] = do_bench(i, j, init_ulp, init2_cached_table, allocate_ulp, search_writeback);

        // GROUP 1
        // result[0] = do_bench(i, j, init_ulp, init2_cached_tree, allocate_ulp, search);
        // result[1] = do_bench(i, j, init_ulp, init2_cached_tree, allocate_ulp, search_writeback);

        // GROUP 2
        // result[0] = do_bench(i, j, init_ulp, init2_cached_table, allocate_ulp, search_writeback_hint);

        // GROUP 3
        // result[0] = do_bench(i, j, init_ulp, init2_cached_tree, allocate_ulp, search_writeback_hint);


        // GROUP 4
        // result[0] = do_bench(i, j, init_cached_nocopy, init2_nocopy, allocate_ulp, search_cached);
        // result[1] = do_bench(i, j, init_cached_nocopy, init2_nocopy, allocate_ulp, search_cached_with_rewriting);
        // result[2] = do_bench(i, j, init_cached, init2_nocopy, allocate_ulp, search_inline_cached);
        // result[3] = do_bench(i, j, init_cached, init2_nocopy, allocate_ulp, search_inline_cached_with_rewriting);
        // result[4] = do_bench(i, j, init_cached_nocopy, init2_nocopy_mpu, allocate_ulp, search);
        // result[5] = do_bench(i, j, init_cached_nocopy, init2_nocopy_mpu, allocate_ulp, search_writeback);

        // GROUP 5
        result[0] = do_bench(i, j, init_cached_nocopy, init2_nocopy_mpu, allocate_ulp, search_writeback_hint);

        // OTHERS
        // do_bench(i, j, init_cached_lru, init2_none, allocate_ulp, search_cached_lru);
        // do_bench(i, j, init_cached_lru, init2_none, allocate_ulp, search_cached_lru_with_rewriting);
        printf("%d, %d, %d, ", j, copied, translated);
        for(int k = 0; k < (sizeof(result) / sizeof(result[0])); ++k)
            printf("%lld, ", result[k]);
        printf("\n");
    }


    // while(1) {
    //     fread(&usr_in, 1, 1, stdin);
    //     if(usr_in == '0') {
    //         usr_in = 0;
    //         break;
    //     } else if(usr_in == '1') {
    //         usr_in = 1;
    //         break;
    //     }
    // }
    // if(usr_in) {
    //     esp_tee_service_call(2, 201, 1);
    // }
    // // RV_SET_CSR_FIELD()
    // volatile int * tst = (int *)0x50000000;
    // *tst = 0;
}
