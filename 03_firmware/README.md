# Firmware (ESP-IDF v6.1 + LVGL 9)

Arquitectura y decisiones: ver la sección "Arquitectura de software" del documento vivo.

| Carpeta | Contenido |
| --- | --- |
| `main/` | `app_main`: crea y conecta los componentes, en orden |
| `bsp/<tarjeta>/bsp/` | Un componente `bsp` por tarjeta, todos con la misma API (`bsp.h`). Hoy: `wt32sc01` |
| `components/vehicle_logic/` | Boost, marcha, shift light y 0–100. C puro, prefijo `vehicle_` |
| `components/data_source/` | Interfaz `data_source_t` y simulador interno |
| `components/settings/` | Ajustes en NVS |
| `components/ui/` | Pantallas LVGL: boost, RPM/marcha y 0–100 |
| `test/host/` | Tests en la PC: `unit/` e `integration/`, con su propio `main.c` |

Cada componente tiene `include/` (API pública) y `src/` (implementación).

## Compilar y flashear

```bash
get_idf
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor   # salir con Ctrl+]
```

- Tarjeta: `idf.py -DBOARD=<subcarpeta de bsp/> build` (por defecto `wt32sc01`). Para una tarjeta nueva se agrega `bsp/<tarjeta>/bsp/` con la misma API.
- Fuente de datos: `idf.py menuconfig` → Dashboard → Fuente de datos (por ahora solo el simulador).

## Tests en host

```bash
get_idf   # usa la copia de Unity (con fixtures) de ESP-IDF
cd test/host
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure
./build/host_tests -v        # todos, con detalle
./build/host_tests -g z2h    # un solo grupo
```

Un test nuevo: archivo en `unit/<componente>/` o `integration/` con `TEST_GROUP`, agregarlo a
`CMakeLists.txt` (fuente y lista de grupos) y su `RUN_TEST_GROUP` en `main.c`.

## CI

`.github/workflows/pr-checks.yml` corre en cada PR y en cada push a `main`: formato (clang-format), tests de
host con cobertura, build del firmware con guardas de tamaño (`07_tools/01_scripts/check_size.py`) y
artefactos (`.bin`, `.elf`, `.map`). Ramas y PRs: `CODING_STYLE.md` §9.

## Ajustes de la pantalla

Si la imagen sale girada, en espejo o con colores invertidos, se corrige con
`LCD_SWAP_XY`, `LCD_MIRROR_X`, `LCD_MIRROR_Y`, `LCD_INVERT` y `LCD_BGR` en
`bsp/wt32sc01/bsp/src/bsp.c`. El touch usa la misma rotación.
