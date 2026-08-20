#ifndef MALBOLGE_VM_H
#define MALBOLGE_VM_H

#include <stdint.h>
#include <stddef.h>

#define MEM_SIZE   59049u
#define LAST       (MEM_SIZE - 1)
#define POW9       19683u
#define HALF       243u
#define BLOCK_SIZE 243u
#define MAX_OVERLAY 200000u
#define OUT_CAP    65536u

static const char ENCRYPT[95] =
    "5z]&gqtyfr$(we4{WP)H-Zn,[%\\3dL+Q;>U!pJS72FhOA1CB6v^=I_0/8|jsb9m<.TVac`uY*MK'X~xDl}REokN:#?G\"i@";

typedef struct {
    uint64_t steps;
    int terminated;
    size_t output_len;
    uint32_t final_c, final_a, final_d;
} RunOutcome;

void vm_init(void);
void vm_build_crazy5(void);
uint32_t vm_crazy(uint32_t a, uint32_t b);
RunOutcome vm_run(const uint32_t *cells, size_t ncells, const uint8_t *input, size_t input_len, uint32_t max_steps);

extern uint8_t vm_out_buf[OUT_CAP];

#endif
