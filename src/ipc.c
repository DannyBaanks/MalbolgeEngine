#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 65536

static char line_buf[MAX_LINE];

static const char *find_field(const char *line, const char *key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *pos = strstr(line, pattern);
    return pos ? pos + strlen(pattern) : NULL;
}

static uint64_t parse_u64(const char *s) {
    uint64_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

static const char *parse_string(const char *s, size_t *len) {
    const char *q1 = strchr(s, '"');
    if (!q1) { *len = 0; return NULL; }
    const char *p = q1 + 1;
    const char *q2 = NULL;
    while (*p) {
        if (*p == '\\' && *(p + 1)) { p += 2; continue; }
        if (*p == '"') { q2 = p; break; }
        p++;
    }
    if (!q2) { *len = 0; return NULL; }
    *len = q2 - q1 - 1;
    return q1 + 1;
}

static void escape_write(FILE *out, const uint8_t *s, size_t len) {
    fputc('"', out);
    for (size_t i = 0; i < len; i++) {
        switch (s[i]) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\n': fputs("\\n", out); break;
            case '\r': fputs("\\r", out); break;
            case '\t': fputs("\\t", out); break;
            default:   fputc(s[i], out); break;
        }
    }
    fputc('"', out);
}

static void json_unescape(char *dst, const char *src, size_t len) {
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            i++;
            switch (src[i]) {
                case '"':  dst[j++] = '"'; break;
                case '\\': dst[j++] = '\\'; break;
                case 'n':  dst[j++] = '\n'; break;
                case 'r':  dst[j++] = '\r'; break;
                case 't':  dst[j++] = '\t'; break;
                default:   dst[j++] = src[i]; break;
            }
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = 0;
}

static void do_run(FILE *out, const char *line, uint64_t req_id) {
    static uint32_t cells[MEM_SIZE];
    size_t ncells = 0;
    const char *prog_raw = find_field(line, "program");
    if (prog_raw) {
        size_t prog_len;
        const char *p = parse_string(prog_raw, &prog_len);
        static char unescaped[MEM_SIZE];
        json_unescape(unescaped, p, prog_len);
        for (size_t i = 0; unescaped[i] && ncells < MEM_SIZE; i++) {
            uint8_t ch = (uint8_t)unescaped[i];
            if (ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') continue;
            if (ch < 33 || ch > 126) continue;
            cells[ncells++] = ch;
        }
    }

    uint32_t max_steps = 100000000u;
    const char *steps_raw = find_field(line, "steps");
    if (steps_raw) {
        while (*steps_raw && (*steps_raw == ':' || *steps_raw == ' ' || *steps_raw == ',')) steps_raw++;
        uint32_t s = (uint32_t)parse_u64(steps_raw);
        if (s > 0) max_steps = s;
    }

    size_t in_len = 0;
    const uint8_t *input = (const uint8_t*)"";
    const char *in_raw = find_field(line, "input");
    if (in_raw) {
        size_t il;
        const char *p = parse_string(in_raw, &il);
        if (p) {
            static char in_unescaped[OUT_CAP];
            json_unescape(in_unescaped, p, il);
            input = (const uint8_t*)in_unescaped;
            in_len = strlen(in_unescaped);
        }
    }

    vm_init();
    RunOutcome o = vm_run(cells, ncells, input, in_len, max_steps);

    const char *status = !o.terminated ? "TIMEOUT" :
                         o.steps >= max_steps ? "TIMEOUT" : "OK";
    fprintf(out, "{\"id\":%llu,\"status\":\"%s\",\"steps\":%llu,\"output\":",
            (unsigned long long)req_id, status, (unsigned long long)o.steps);
    escape_write(out, vm_out_buf, o.output_len);
    fputs("}\n", out);
}

int main(void) {
    vm_build_crazy5();

    FILE *out = stdout;
    fputs("{\"status\":\"ready\"}\n", out);
    fflush(out);

    size_t pos = 0;
    int ch;
    while ((ch = fgetc(stdin)) != EOF) {
        if (ch == '\n') {
            line_buf[pos] = 0;
            if (pos == 0 || line_buf[0] == '#') { pos = 0; continue; }

            const char *id_raw = find_field(line_buf, "id");
            while (id_raw && (*id_raw == ':' || *id_raw == ' ' || *id_raw == ',')) id_raw++;
            uint64_t req_id = id_raw ? parse_u64(id_raw) : 0;

            size_t op_len;
            const char *op_raw = find_field(line_buf, "op");
            const char *op = op_raw ? parse_string(op_raw, &op_len) : NULL;
            (void)op_len;

            if (op && strncmp(op, "quit", 4) == 0) return 0;
            if (op && strncmp(op, "ping", 4) == 0) {
                fprintf(out, "{\"id\":%llu,\"status\":\"OK\",\"output\":\"pong\"}\n",
                        (unsigned long long)req_id);
                fflush(out);
                pos = 0;
                continue;
            }
            if (op && strncmp(op, "run", 3) == 0) {
                do_run(out, line_buf, req_id);
                fflush(out);
                pos = 0;
                continue;
            }

            fprintf(out, "{\"id\":%llu,\"status\":\"ERROR\",\"message\":\"unknown_op\"}\n",
                    (unsigned long long)req_id);
            fflush(out);
            pos = 0;
            continue;
        }
        if (ch != '\r' && pos < MAX_LINE - 1) {
            line_buf[pos++] = ch;
        }
    }
    return 0;
}
