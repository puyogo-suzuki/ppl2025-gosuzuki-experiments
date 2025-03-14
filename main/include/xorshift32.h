#include <stdint.h>
struct xorshift32_state { uint32_t a; };
uint32_t xorshift32(struct xorshift32_state *state);