# Estilo de código del firmware

Reglas cortas y verificables. Cada una dice cómo se revisa: **formato** (clang-format, automático),
**análisis** (clang-tidy, automático cuando esté en CI) o **revisión** (a mano o con `/code-review` y `/simplify`).
Una excepción se justifica en un comentario junto al código.

## 1. Formato — formato

- `.clang-format` manda: 4 espacios, 110 columnas, llave de función en su propia línea, llaves siempre en `if`/`for`/`while`.
- Formatear antes de cada commit:
  ```bash
  CF=$(ls ~/.espressif/tools/esp-clang/*/esp-clang/bin/clang-format | head -1)
  find main components bsp test -name '*.[ch]' -not -path '*/build/*' | xargs "$CF" -i --style=file
  ```
- El pre-commit local (`.githooks/pre-commit`) rechaza el commit si lo staged no tiene formato, y corre los
  tests de host si toca `components/` o `test/host/`. Se activa una vez por clon con
  `git config core.hooksPath .githooks`; `git commit --no-verify` lo salta.
- Includes en bloques separados por una línea vacía, en este orden: header propio del `.c`, C estándar,
  ESP-IDF/FreeRTOS/terceros, proyecto. clang-format los agrupa y ordena.
- No se formatea código de terceros (`managed_components/`, `build/`).

## 2. Nombres — revisión

| Qué | Forma | Ejemplo |
| --- | --- | --- |
| Funciones y variables | `snake_case`, en inglés | `vehicle_on_sample`, `shift_rpm` |
| API pública | Prefijo del componente, completo | `vehicle_`, `bsp_`, `ui_`, `settings_`, `data_source_`, `app_` |
| Tipos | `snake_case_t` con el mismo prefijo | `vehicle_cfg_t` |
| Macros y constantes de enum | `MAYUSCULAS` con el prefijo | `VEHICLE_GEAR_COUNT`, `BSP_CAN_TX` |
| Magnitudes físicas | Unidad al final del nombre | `latency_ms`, `speed_kmh`, `map_kpa`, `rate_rpm_s` |
| Booleanos | Se leen como pregunta o estado | `shift_alarm`, `has_last`, `running` |

Sin abreviaturas inventadas: `z2h` y `rpm` están en el glosario del documento vivo; si una abreviatura no está ahí, se escribe completa.

## 3. Estructura — revisión

- Cada componente: `include/` (API pública) y `src/` (implementación y headers privados).
- Todo lo que no es API es `static`. Sin variables globales compartidas entre archivos.
- Las dependencias solo bajan: `main` → componentes → `bsp`. Un componente no incluye headers de otro que no esté en su `REQUIRES`.
- `vehicle_logic` es C puro: no incluye nada de ESP-IDF, FreeRTOS ni LVGL. Así compila y se prueba en host.
- Los headers empiezan con `#pragma once` y llevan `extern "C"`.

## 4. Funciones y errores — revisión y análisis

- Una función hace una cosa y cabe en una pantalla (unas 60 líneas). Si no, se parte.
- Las funciones que pueden fallar regresan `esp_err_t`, y los constructores regresan NULL si fallan. Quien llama decide qué hacer.
- `ESP_ERROR_CHECK` solo en `app_main` y solo para fallas críticas: NVS o pantalla. Un problema con el CAN o la ECU degrada y avisa en pantalla.
- Dentro de los componentes: `ESP_RETURN_ON_ERROR` / `ESP_RETURN_ON_FALSE` con un mensaje corto.
- Parámetros de entrada que no se modifican: `const`.

## 5. Números y tipos — revisión y análisis

- Sin números mágicos: `#define` o `static const` con nombre y unidad. Se exceptúan 0, 1, las conversiones
  evidentes (1000 ms/s) y las coordenadas de layout dentro de la función que construye una pantalla.
- Enteros de ancho fijo (`uint32_t`, `int16_t`) para datos del bus, tiempos y tamaños; `int` solo para contadores locales.
- `float`, nunca `double`: el ESP32 tiene FPU de precisión simple. Literales con sufijo `f`.
- Tiempos en `uint32_t` ms desde el arranque; las restas de tiempos se hacen en `uint32_t`, que tolera el desborde.

## 6. Embebido — revisión

- Sin `malloc` en el camino de datos (muestras, refresco de UI). La memoria se reserva al arrancar.
- LVGL solo desde su tarea o con `bsp_display_lock()`. Los callbacks de LVGL ya tienen el lock tomado.
- La UI redibuja solo lo que cambió: se guarda el último valor dibujado.
- Las tareas declaran core, prioridad y stack con `#define` junto a su creación.
- Nada que bloquee más de unos ms con un lock o un mutex tomado.

## 7. Comentarios — revisión

- En español, en la misma voz que el documento vivo.
- Explican el porqué, una restricción o un requerimiento (`/* SYS-08: ±0.2 s ... */`), no lo que el código ya dice.
- Los `TODO` llevan qué falta y de qué depende: `/* TODO(Q14): RPM del limitador */`.
- Cada valor provisional se marca como tal y dice qué lo reemplazará.

## 8. Tests — revisión y CI

- La lógica nueva o modificada de `vehicle_logic` lleva test en `test/host/unit/`.
- Un bug corregido lleva un test que fallaba antes de la corrección.
- Un grupo de Unity por módulo; nombres de test que describan el comportamiento (`stopping_aborts`).
- Las pruebas de varios módulos juntos van en `test/host/integration/`.
- Cada test cita los requisitos que verifica en la línea anterior a su `TEST(...)`: `/* Verifica: SW-101, SW-105 */`
  (`01_docs/01_requirements/software.md`). CI lo revisa con `07_tools/01_scripts/trace.py`.

## 9. Ramas, commits y PRs — CI y revisión

- GitHub Flow: `main` siempre compila y pasa los tests. Está protegida: todo entra por PR con los checks
  en verde (Formato, Tests en host, Build del firmware), sin push directo.
- Una rama por cambio, de vida corta (días), con prefijo: `feat/`, `fix/`, `ui/`, `docs/`, `ci/`, `chore/`.
  Ejemplo: `feat/obd-twai`. Se borra al hacer merge.
- Merge con squash: un commit por PR en `main`. El título del PR es el mensaje del commit, en español
  e imperativo: `Agrega fila de LEDs al shift light`.
- Si el PR toca UI, BSP o CAN, la descripción dice cómo se probó en la tarjeta (hasta que haya HIL).
- Las versiones son tags `vX.Y.Z` sobre `main`.
- Antes de abrir el PR: formato, tests de host y build del firmware sin warnings; en cambios grandes,
  `/code-review` y `/simplify`.
