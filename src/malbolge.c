#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *usage =
    "usage: malbolge <program.malbolge> [max_steps] < input\n";

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "%s", usage);
        return 1;
    }
    uint32_t max_steps = 100000000u;
    if (argc >= 3) {
        max_steps = (uint32_t)strtoul(argv[2], NULL, 10);
        if (max_steps == 0) max_steps = 100000000u;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "error: cannot open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *source = malloc(flen + 1);
    fread(source, 1, flen, f);
    source[flen] = 0;
    fclose(f);

    static uint32_t cells[MEM_SIZE];
    size_t ncells = 0;
    for (long i = 0; i < flen; i++) {
        uint8_t ch = source[i];
        if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;
        if (ch < 33 || ch > 126) {
            fprintf(stderr, "error: invalid character 0x%02x in program\n", ch);
            free(source);
            return 2;
        }
        if (ncells >= MEM_SIZE) break;
        cells[ncells++] = ch;
    }
    free(source);

    size_t cap = 65536;
    uint8_t *input = malloc(cap);
    size_t input_len = 0;
    size_t n;
    uint8_t buf[4096];
    while ((n = fread(buf, 1, sizeof(buf), stdin)) > 0) {
        if (input_len + n > cap) {
            while (input_len + n > cap) cap *= 2;
            input = realloc(input, cap);
        }
        memcpy(input + input_len, buf, n);
        input_len += n;
    }

    vm_build_crazy5();
    vm_init();
    RunOutcome o = vm_run(cells, ncells, input, input_len, max_steps);
    free(input);

    fwrite(vm_out_buf, 1, o.output_len, stdout);
    fputc('\n', stdout);
    fprintf(stderr, "steps: %llu | status: %s\n",
            (unsigned long long)o.steps,
            !o.terminated ? "TIMEOUT" : "HALTED");

    return 0;
}
