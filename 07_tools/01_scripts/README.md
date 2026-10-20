# 07_tools/01_scripts

Scripts de Python para el desarrollo. Usan el Python de ESP-IDF (`get_idf`), que ya trae `pyserial`;
con otro Python: `pip install -r 07_tools/01_scripts/requirements.txt`.

| Script | Para qué |
| --- | --- |
| `check_size.py` | Guardas de tamaño: falla si la IRAM pasa de 85 % o la app de 80 % de su partición (lo usa CI) |
| `read_log.py` | Log del firmware en vivo o desde archivo: colores por nivel, filtro por tag/nivel, backtraces decodificados, guardado y resumen |

## read_log.py

```bash
python 07_tools/01_scripts/read_log.py                            # en vivo por /dev/ttyUSB0
python 07_tools/01_scripts/read_log.py --reset --save             # reinicia la tarjeta y guarda en 06_testing/02_bench/logs/
python 07_tools/01_scripts/read_log.py --file run.log --level W   # solo avisos y errores de un log guardado
python 07_tools/01_scripts/read_log.py --tag sim --tag app        # solo esos tags
```

- Los backtraces se decodifican con `xtensa-esp32-elf-addr2line` y `03_firmware/build/esp32_dashboard.elf`.
  Si el log trae el `ELF file SHA256` del arranque y no coincide con el ELF local, no decodifica:
  las direcciones de otro build apuntan a funciones equivocadas.
- No abrir a la vez que `idf.py monitor`: los dos quieren el mismo puerto.

## check_size.py

```bash
cd 03_firmware && idf.py build && cd ..
python 07_tools/01_scripts/check_size.py                       # límites por defecto
python 07_tools/01_scripts/check_size.py --iram-max 90         # otro límite
```
