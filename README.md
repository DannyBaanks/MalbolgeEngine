# Malbolge-Engine

Motor interprete Malbolge independiente — C puro, sin dependencias.

Tres targets de build:
- **`malbolge`** — CLI interprete (`malbolge <file.malbolge> [max_steps] < input`)
- **`malbolge-ipc`** — servidor JSONL IPC (stdin/stdout, un JSON request por linea)
- **`malbolge`** (libreria) — importa `vm.h` y liga `vm.c` a tu proyecto

## Build

```bash
make            # construye malbolge + malbolge-ipc
make ipc        # construye solo el servidor IPC
make test       # construye + corre la prueba hello_world
make clean      # elimina binarios
```

Requisitos: cualquier compilador C (gcc, clang, tcc, ...). Sin librerias externas.

## Uso

### CLI

```bash
echo "" | ./malbolge examples/hello.malbolge 100000
# Output: Hello, world.
#         steps: 48 | status: HALTED
```

### IPC (protocolo JSONL)

```bash
echo '{"id":1,"op":"run","program":"(=<`#9]~6ZY...","steps":100000}
{"id":2,"op":"ping"}
{"id":3,"op":"quit"}' | ./malbolge-ipc
```

Respuesta:
```
{"status":"ready"}
{"id":1,"status":"OK","steps":48,"output":"Hello, world."}
{"id":2,"status":"OK","output":"pong"}
```

### Protocolo

| Request | Response |
|---------|----------|
| `{"id":N,"op":"run","program":"...","steps":M,"input":"..."}` | `{"id":N,"status":"OK\|INVALID\|TIMEOUT","steps":K,"output":"..."}` |
| `{"id":N,"op":"ping"}` | `{"id":N,"status":"OK","output":"pong"}` |
| `{"id":N,"op":"quit"}` | (el servidor sale) |

Valores de status:
- `OK` — el programa paro normalmente
- `INVALID` — el programa contenía opcodes invalidos
- `TIMEOUT` — se excedio el limite de `steps`
- `ERROR` — error de runtime/parseo

### Libreria

```c
#include "vm.h"

uint32_t cells[59049];
size_t ncells = parse_program(source, cells);

vm_build_crazy5();      // llamar una vez
vm_init();              // llamar antes de cada corrida
RunOutcome o = vm_run(cells, ncells, input, input_len, max_steps);
// o.output_len bytes estan en vm_out_buf[]
```

## Arquitectura

```
src/
  vm.h         — API publica + constantes
  vm.c         — VM de Malbolge (semantica exacta, overlay lazy-fill)
  malbolge.c   — CLI interprete
  ipc.c        — servidor JSONL IPC
examples/
  hello.malbolge — programa de prueba "Hello, world."
```

### Semanticas de la VM

La VM replica el comportamiento exacto del debugger SparseMemory del paquete `malbolge`:

- **Overlay** para celdas del fuente + escrituras; las lecturas checan el overlay primero
- **Lazy crazy-fill** en bloques de 243 celdas: las semillas del bloque final se leen del overlay (no del fuente) en el primer acceso lejado, asi que escrituras previas (encrypts, crz, rotr) pueden cambiar los valores de fill
- **Tabla crazy** precomputada como pares de trits 243×243 (bloques de 5 trits, combinados como `crazy5[a%243][b%243] + 243*crazy5[a/243][b/243]`)
- **Tabla encrypt** aplicada despues de cada ejecucion de instruccion

Verificado contra 299,593 entradas de catalogo + 417 casos aleatorios de paridad, 0 desacuerdos.

## Licencia

MIT

## Por que GitHub dice que esto es C?

Porque lo es. El motor esta escrito en C; Malbolge es lo que *corre*.

Hay tambien una segunda razon, y vale la pena anotarla: **GitHub Linguist
no tiene entrada para Malbolge.** Brainfuck si tiene una (`language_id: 38`, `.b`/`.bf`),
asi que los lenguajes esotericos no se excluyen por principio — Malbolge simplemente nunca se
agrego. Una linea `.gitattributes` como `*.malbolge linguist-language=Malbolge`
por lo tanto no hace nada: el atributo nombra un lenguaje que Linguist no
conoce, y se ignora silenciosamente.

Que lo agreguen upstream no es un pedido pequeno. Linguist requiere al menos 2000
archivos con la extension indexada en el ultimo ano, repartidos en muchos
repositorios, mas una gramatica de syntax-highlighting con licencia aprobada y
ejemplos de codigo del mundo real — archivos "hello world" se rechazan explicitamente.

Asi que los ejemplos aqui se marcan `linguist-detectable=false` en su lugar. La
barra de lenguajes muestra de que esta hecho el repositorio, y este archivo dice para que es.
