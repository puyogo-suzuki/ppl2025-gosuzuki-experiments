#include "../../main/tree_t.h"
#include "esp_rom_sys.h"

#define NULL (void *)0

struct tree_t * buf;
int * buf_idx;
struct tree_t * allocate_main(void) {
    int _buf_idx = *buf_idx;
    if(_buf_idx >= (8192 / sizeof(struct tree_t)))
        return NULL;
    *buf_idx = _buf_idx + 1;
    return &(buf[_buf_idx]);
}

struct tree_t * buf_ulp_begin;
struct tree_t * cache[8192 / sizeof(struct tree_t)];

struct tree_t cache2[(8192 / sizeof(struct tree_t))];
struct tree_t * cache2_top;
int cache2_idx;
int state;
void reset_buffering(int st, struct tree_t * buf_ulp, struct tree_t * buf_begin, int * buf_i){
    state = st;
    buf = buf_begin;
    buf_idx = buf_i;
    buf_ulp_begin = buf_ulp;
#define NO_COPY_ON_READ 1
#if NO_COPY_ON_READ
    for(int i = 0; i < (8192 / sizeof(struct tree_t)); ++i) cache[i] = &buf[i];
#else
    for(int i = 0; i < (8192 / sizeof(struct tree_t)); ++i) cache[i] = NULL;
#endif
    cache2_top = NULL;
    cache2_idx = 0;
}
struct tree_t * allocate_cache_tree(void) {
    if(cache2_idx >= (sizeof(cache2) / sizeof(cache2[0])))
        return NULL;
    int i = cache2_idx;
    cache2_idx++;
    return &(cache2[i]);
}
struct tree_t * search(struct tree_t * t, long v) {
    while(t != NULL) {
        if(t->val == v) return t;
        t = t->val > v ? t->l : t->r;
    }
    return 0;
}
struct tree_t * append_cache_tree(struct tree_t ** _t, long v) {
    struct tree_t * t = *_t;
    while(t != NULL){
        if(t->val == v) return t; // not added.
        _t = t->val > v ? &(t->l) : &(t->r);
        t = *_t;
    }
    t = allocate_cache_tree();
    t->l = NULL; t->r = NULL;
    t->val = v;
    *_t = t;
    return t;
}

#define IS_TREE 0

#define IS_IN_ULP(v) (((v) >= buf_ulp_begin) && ((v) < &buf_ulp_begin[8192 / sizeof(struct tree_t)]))
void * copy_buffer(void * _from) {
    struct tree_t ** dst;
    struct tree_t * ret;
    if(!IS_IN_ULP((struct tree_t *)_from)) return NULL;
    long idx = ((long)_from - (long)buf_ulp_begin) / sizeof(struct tree_t);
    void * from = &buf_ulp_begin[idx];
    #if IS_TREE
    // if(state) {
        struct tree_t * translated = search(cache2_top, (long)from);
        if(translated) {
            ret = (void *)(translated->val2);
            goto RET;
        }
        translated = append_cache_tree(&cache2_top, (long)from);
        dst = (struct tree_t **)(&(translated->val2));
    // } else {
    #else
        dst = &cache[idx];
        ret = *dst;
        if(ret) goto RET;
    // }
    #endif
    ret = allocate_main();
    if(ret == NULL) return NULL;
    struct tree_t * t = (struct tree_t *)from;
    ret->l = t->l;
    ret->r = t->r;
    ret->val = t->val;
    ret->val2 = t->val2;
    // esp_rom_printf("copied: addr=%lx l=%lx r=%lx val=%lx val2=%lx\n", (long)ret, (long)(ret->l), (long)(ret->r), ret->val, ret->val2);
    *dst = ret;
    RET:
    return (void *)((long)ret + ((long)_from - (long)from));
}
