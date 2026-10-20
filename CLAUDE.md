# ESP32 CAN Dashboard (OBD-II)

Medidor para auto: WT32-SC01 (ESP32-WROVER-B rev. v1.0) + SN65HVD230, firmware en C con ESP-IDF v6.1 y LVGL 9.
Documento vivo (concepto, requerimientos SYS-xx, preguntas Qn, arquitectura, CI/CD):
https://claude.ai/code/artifact/7d05a37c-db8c-4c35-bbf3-e4ddce83907b

## Reglas

- El código de `03_firmware/` sigue `03_firmware/CODING_STYLE.md`. Leerlo antes de escribir o revisar firmware.
- Formatear con el clang-format de `esp-clang` (comando en `CODING_STYLE.md` §1) antes de dar un cambio por terminado.
- Una decisión nueva (hardware, arquitectura, requerimiento, respuesta a una Qn) se registra en el documento vivo, no en un archivo nuevo.
- Los requerimientos se redactan con el skill `writing-requirements` (INCOSE), y al enmendar uno se anota fecha, texto anterior y motivo.
- `vehicle_logic` es C puro: nada de ESP-IDF, FreeRTOS ni LVGL, para que compile en host.
- No commitear ni hacer push sin que el usuario lo pida.

## Comandos

```bash
get_idf                                   # activa ESP-IDF v6.1 (alias en ~/.zshrc)

cd 03_firmware
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor      # salir con Ctrl+]

cd 03_firmware/test/host                  # tests en host (Unity con fixtures)
cmake -S . -B build -G Ninja && cmake --build build && ctest --test-dir build --output-on-failure

python 07_tools/01_scripts/read_log.py --reset --save   # log con backtraces decodificados
```

## Restricciones del hardware que ya costaron un bug

- ESP32 rev. v1.0: `esp-hal` (Rust) no la soporta; ESP-IDF sí, con `SPIRAM_CACHE_WORKAROUND`.
- IRAM de 128 KB: `LV_ATTRIBUTE_FAST_MEM_USE_IRAM` no cabe.
- El DMA de SPI no lee PSRAM: los buffers de LVGL van en RAM interna.
- Evitar widgets que creen capas ARGB8888 grandes (p. ej. `lv_bar` con radio): con poca memoria LVGL reintenta sin ceder la CPU y salta el watchdog.
