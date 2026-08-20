# Malbolge-Engine

Standalone Malbolge interpreter engine — pure C, no dependencies.

Three build targets:
- **`malbolge`** — CLI interpreter (`malbolge <file.malbolge> [max_steps] < input`)
- **`malbolge-ipc`** — JSONL IPC server (stdin/stdout, one JSON request per line)
- **`malbolge`** (library) — import `vm.h` and link `vm.c` into your project

## Build

```bash
make            # builds both malbolge + malbolge-ipc
make ipc        # builds only the IPC server
make test       # builds + runs the hello_world test
make clean      # removes binaries
```

Requires: any C compiler (gcc, clang, tcc, ...). No external libraries.

## Usage

### CLI

```bash
echo "" | ./malbolge examples/hello.malbolge 100000
# Output: Hello, world.
#         steps: 48 | status: HALTED
```

### IPC (JSONL protocol)

```bash
echo '{"id":1,"op":"run","program":"(=<`#9]~6ZY...","steps":100000}
{"id":2,"op":"ping"}
{"id":3,"op":"quit"}' | ./malbolge-ipc
```

Response:
```
{"status":"ready"}
{"id":1,"status":"OK","steps":48,"output":"Hello, world."}
{"id":2,"status":"OK","output":"pong"}
```

### Protocol

| Request | Response |
|---------|----------|
| `{"id":N,"op":"run","program":"...","steps":M,"input":"..."}` | `{"id":N,"status":"OK\|INVALID\|TIMEOUT","steps":K,"output":"..."}` |
| `{"id":N,"op":"ping"}` | `{"id":N,"status":"OK","output":"pong"}` |
| `{"id":N,"op":"quit"}` | (server exits) |

Status values:
- `OK` — program halted normally
- `INVALID` — program contained invalid opcodes
- `TIMEOUT` — exceeded `steps` limit
- `ERROR` — runtime/parse error

### Library

```c
#include "vm.h"

uint32_t cells[59049];
size_t ncells = parse_program(source, cells);

vm_build_crazy5();      // call once
vm_init();              // call before each run
RunOutcome o = vm_run(cells, ncells, input, input_len, max_steps);
// o.output_len bytes are in vm_out_buf[]
```

## Architecture

```
src/
  vm.h         — public API + constants
  vm.c         — Malbolge VM (exact semantics, lazy-fill overlay)
  malbolge.c   — CLI interpreter
  ipc.c        — JSONL IPC server
examples/
  hello.malbolge — "Hello, world." test program
```

### VM semantics

The VM replicates the exact behavior of the `malbolge` package's SparseMemory debugger:

- **Overlay** for source cells + writes; reads check overlay first
- **Lazy crazy-fill** in blocks of 243 cells: tail-block seeds are read from the overlay (not source) at first far access, so prior writes (encrypts, crz, rotr) can change fill values
- **Crazy table** precomputed as 243×243 trit pairs (5-trit blocks, combined as `crazy5[a%243][b%243] + 243*crazy5[a/243][b/243]`)
- **Encrypt table** applied after each instruction execution

Verified against 299,593 catalog entries + 417 randomized parity cases, 0 mismatches.

## License

MIT
