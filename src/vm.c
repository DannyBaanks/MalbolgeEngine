#include "vm.h"
#include <string.h>

static const uint8_t CRAZY_TBL[3][3] = {
    {1, 0, 0},
    {1, 0, 2},
    {2, 2, 1}
};

static uint32_t crazy5[HALF][HALF];
static uint32_t chain[MEM_SIZE];
static uint32_t ovl_idx[MAX_OVERLAY];
static uint32_t ovl_val[MAX_OVERLAY];
static size_t ovl_len = 0;
static size_t chain_until = 0;
static size_t fill_start = 0;

uint8_t vm_out_buf[OUT_CAP];

void vm_build_crazy5(void) {
    for (size_t a5 = 0; a5 < HALF; a5++) {
        for (size_t b5 = 0; b5 < HALF; b5++) {
            uint32_t r = 0, p = 1;
            size_t aa = a5, bb = b5;
            for (int k = 0; k < 5; k++) {
                r += (uint32_t)CRAZY_TBL[bb % 3][aa % 3] * p;
                aa /= 3;
                bb /= 3;
                p *= 3;
            }
            crazy5[a5][b5] = r;
        }
    }
}

uint32_t vm_crazy(uint32_t a, uint32_t b) {
    return crazy5[a % HALF][b % HALF] + HALF * crazy5[a / HALF][b / HALF];
}

static void ensure_filled(uint32_t x);

static uint32_t rotate(uint32_t n) {
    return POW9 * (n % 3) + n / 3;
}

static int ovl_get(uint32_t x, uint32_t *out) {
    for (size_t i = 0; i < ovl_len; i++) {
        if (ovl_idx[i] == x) { *out = ovl_val[i]; return 1; }
    }
    return 0;
}

static void ovl_set(uint32_t x, uint32_t v) {
    for (size_t i = 0; i < ovl_len; i++) {
        if (ovl_idx[i] == x) { ovl_val[i] = v; return; }
    }
    if (ovl_len < MAX_OVERLAY) {
        ovl_idx[ovl_len] = x;
        ovl_val[ovl_len] = v;
        ovl_len++;
    }
}

static uint32_t mem_get(uint32_t x) {
    uint32_t v;
    if (ovl_get(x, &v)) return v;
    if (x < fill_start) return 0;
    if ((size_t)x >= chain_until) ensure_filled(x);
    return chain[x];
}

static void mem_set(uint32_t x, uint32_t v) {
    ovl_set(x, v);
}

static void ensure_filled(uint32_t x) {
    uint32_t block_end = (x / BLOCK_SIZE + 1) * BLOCK_SIZE;
    uint32_t tail_end = (fill_start / BLOCK_SIZE + 1) * BLOCK_SIZE;
    uint32_t p1, p2;
    size_t i;
    uint32_t tmp;

    if (chain_until == fill_start) {
        p1 = ovl_get((uint32_t)(fill_start - 1), &tmp) ? tmp : 0;
        p2 = ovl_get((uint32_t)(fill_start - 2), &tmp) ? tmp : 0;
        i = fill_start;
    } else {
        i = chain_until;
        p1 = chain[i - 1];
        p2 = chain[i - 2];
    }

    while (i < block_end) {
        uint32_t nxt;
        if (i == tail_end) {
            nxt = vm_crazy(mem_get((uint32_t)(i - 1)), mem_get((uint32_t)(i - 2)));
        } else if (i == tail_end + 1) {
            nxt = vm_crazy(p1, mem_get((uint32_t)(i - 2)));
        } else {
            nxt = vm_crazy(p1, p2);
        }
        chain[i] = nxt;
        p2 = p1;
        p1 = nxt;
        i++;
    }
    chain_until = block_end;
}

void vm_init(void) {
    ovl_len = 0;
    chain_until = 0;
    fill_start = 0;
    memset(vm_out_buf, 0, OUT_CAP);
}

RunOutcome vm_run(const uint32_t *cells, size_t ncells, const uint8_t *input, size_t input_len, uint32_t max_steps) {
    fill_start = ncells > 2 ? ncells : 2;
    chain_until = fill_start;
    ovl_len = 0;

    for (size_t i = 0; i < ncells; i++) {
        ovl_set((uint32_t)i, cells[i]);
    }

    uint32_t a = 0, c = 0, d = 0;
    uint64_t executed = 0;
    int terminated = 0;
    size_t input_pos = 0;
    size_t output_len = 0;

    while (executed < max_steps) {
        executed++;
        uint32_t ins = mem_get(c);
        if (ins < 33 || ins > 126) { terminated = 1; break; }
        uint32_t v = (ins + c) % 94;
        switch (v) {
            case 4:  c = mem_get(d); break;
            case 5:
                if (output_len < OUT_CAP) {
                    vm_out_buf[output_len++] = (uint8_t)(a % 256);
                }
                break;
            case 23:
                if (input_pos < input_len) {
                    a = input[input_pos++];
                } else {
                    terminated = 1;
                }
                break;
            case 39: {
                uint32_t r = rotate(mem_get(d));
                mem_set(d, r);
                a = r;
                break;
            }
            case 40: d = mem_get(d); break;
            case 62: {
                uint32_t r = vm_crazy(a, mem_get(d));
                mem_set(d, r);
                a = r;
                break;
            }
            case 81: terminated = 1; break;
            default: break;
        }
        if (terminated) break;
        uint32_t enc = mem_get(c);
        if (enc >= 33 && enc <= 126) mem_set(c, (uint32_t)ENCRYPT[enc - 33]);
        c = (c == LAST) ? 0 : c + 1;
        d = (d == LAST) ? 0 : d + 1;
    }

    RunOutcome o;
    o.steps = executed;
    o.terminated = terminated;
    o.output_len = output_len;
    o.final_c = c;
    o.final_a = a;
    o.final_d = d;
    return o;
}
