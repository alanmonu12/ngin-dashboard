# Demostración en la tarjeta de los SW-xx en Implementado (M1, issue #28)

Guion para pasar a **Verificado** los SW-xx con código que no tienen test en host. Cada prueba copia el criterio de
la columna Verificación de `01_docs/01_requirements/software.md`. El resultado va al PR que cierra #28: por cada
SW-xx, pasa o no pasa y su evidencia (líneas del log o salida de `bench screen`). Con ese PR, cada SW-xx que pasa cambia a Verificado y su
verificación cita el PR (`PR #nn`), como pide `trace.py`.

## Partes

Todo se corre con el build de banco (ver "Build de banco"): los tiempos salen del log, sin video.

| Parte | Qué necesita | SW-xx |
| --- | --- | --- |
| A | Alguien con la tarjeta en la mano: deslizar y mirar la pantalla. Los tiempos los mide el firmware | SW-306, SW-505, SW-506, SW-507, SW-901 |
| B | Solo la consola `bench` por el puerto serial | SW-104, SW-108, SW-109, SW-302, SW-307, SW-501, SW-502, SW-503, SW-504, SW-701, SW-702, SW-703, SW-706, SW-904, SW-906 |
| C | Inspección o CI, registrada en el PR; sin tarjeta | SW-704, SW-902, SW-903, SW-907, SW-908, SW-909 |

SW-303 sale de este guion: sus 150 ms incluyen la latencia OBD y solo se pueden medir con el simulador de ECU
(#43, M3).

## Equipo

- WT32-SC01 con el build de banco del commit que se prueba, alimentada por USB-C.
- PC con ESP-IDF (`get_idf`).

## Preparación

1. Anotar el commit: `git rev-parse --short HEAD`.
2. Construir y flashear el build de banco (comandos en "Build de banco").
3. Guardar la salida de `idf.py -B build_bench monitor` de cada sesión en `06_testing/02_bench/logs/`; se cita en el
   PR.

Valores del simulador (`bench live`): arranca 4 s en ralentí, acelera de 1.ª a 6.ª cambiando a 6200 rpm, llega a
170 km/h y frena. Con los Ajustes_Por_Defecto la RPM_Cambio es 6000 rpm, así que la Alarma_Cambio se activa en cada
marcha.

## Parte A: con la tarjeta en la mano

### SW-306 Parpadeo de la Alarma_Cambio

1. `bench live` y pasar a la pantalla de RPM.
2. **Pasa si** al llegar a 6000 rpm los 15 LED_Simulado parpadean juntos en rojo. La frecuencia (6 Hz) se verifica
   por análisis del código: no se mide.

### SW-505 y SW-506 Cambio de pantalla en ≤ 300 ms

1. Deslizar 3 veces a la izquierda (SW-505) y 3 a la derecha (SW-506).
2. **Pasa si** el orden es boost → RPM → 0–100 → boost a la izquierda y el inverso a la derecha, y cada línea
   `cambio de pantalla: N ms` del log da 300 ms o menos. Se mide desde que LVGL reconoce el gesto hasta la pantalla
   cargada.

### SW-507 Valores actuales al cambiar de pantalla en ≤ 50 ms

1. Con `bench live`, cambiar de pantalla 6 veces.
2. **Pasa si** cada línea `valores en la pantalla nueva: N ms` del log da 50 ms o menos, y a la vista ningún valor
   aparece en 0 ni con el número de la última vez que se vio esa pantalla.

### SW-901 Arranque hasta la pantalla de boost en ≤ 3 s

1. Desconectar y conectar el USB-C 5 veces, con 10 s desconectada entre cada una.
2. **Pasa si** cada línea `primer cuadro con datos: N ms desde el reset` del log da 3000 ms o menos y al conectar se
   ve la pantalla de boost. Anotar los 5 tiempos: también sirven para SYS-29.

## Build de banco

Las pruebas de la parte B necesitan meter valores fijos, guardar ajustes y provocar fallas, cosas que el firmware
de producción no permite. El build de banco (opción `APP_BENCH`, apagada por defecto, nunca en el build del auto)
agrega una consola `bench` por el puerto serial y **arranca con valores fijos de ralentí** (800 rpm, 0 km/h, MAP de
32 kPa, barómetro de 78 kPa) en lugar del simulador, así el Boost_Pico parte de 0 en cada arranque.

```bash
cd 03_firmware
idf.py -B build_bench -D SDKCONFIG=build_bench/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.bench" build
idf.py -B build_bench -p /dev/ttyUSB0 flash monitor
```

| Comando | Para qué |
| --- | --- |
| `bench fix rpm=<rpm> kmh=<km/h> map=<kPa> baro=<kPa>` | Reemplaza al simulador por muestras fijas cada 50 ms. Los campos omitidos quedan igual |
| `bench live` | Regresa al simulador |
| `bench z2h <ms>` · `bench z2h off` | Fuerza el tiempo 0–100 mostrado, o deja de forzarlo |
| `bench save unit <bar\|psi\|kPa>` | Guarda la Unidad_Presión con `settings_save` e imprime `OK` o el error |
| `bench save shift <marcha> <rpm>` | Guarda la RPM_Cambio de una marcha con `settings_save` e imprime `OK` o el error |
| `bench save bar_start <rpm>` | Guarda la RPM_Inicio_Barra con `settings_save` e imprime `OK` o el error |
| `bench settings` | Imprime los ajustes en uso |
| `bench corrupt` | Escribe en NVS los ajustes actuales con una RPM_Cambio de 1.ª de 2900 rpm, sin validarlos |
| `bench screen` | Imprime lo que muestra la pantalla activa: texto de cada etiqueta, valor del arco y patrón de los LED_Simulado (`G`, `Y`, `R` o `.` apagado) |
| `bench screen next` · `bench screen prev` | Pasa a la pantalla siguiente o anterior del Ciclo_Pantallas, sin el touch |
| `bench latency` · `bench latency reset` | Latencia de muestra a cuadro dibujado (máxima, p99, p50 y media) en las últimas 1000 actualizaciones, o vacía la ventana |
| `bench fail_source` | Reinicia y hace fallar el arranque de la fuente de datos una vez |
| `bench panic` | Llama a `abort()` |

La unidad solo se lee al arrancar (`ui_start`): después de `bench save unit` hay que reiniciar, y eso de paso prueba
SW-701.

`bench latency` mide del instante de la muestra al fin del render del cuadro que la incluye; el último bloque puede
tardar unos ms más en salir por SPI (a 80 MHz, un bloque de 40 líneas tarda unos 4 ms).

Además, el build de banco escribe en el log los tiempos de la parte A: `cambio de pantalla: N ms` y
`valores en la pantalla nueva: N ms` en cada cambio de pantalla, contados desde el gesto, y una vez por arranque
`primer cuadro con datos: N ms desde el reset`.

Con la consola, `read_log.py` no sirve para escribir comandos: usar `idf.py monitor` y guardar su salida.

## Parte B: solo la consola

Valores comunes: barómetro de 78 kPa, Ajustes_Por_Defecto (RPM_Cambio 6000 rpm, RPM_Inicio_Barra 2500 rpm).

`bench screen` es la evidencia de lo que muestra la pantalla: imprime el texto y el estado que LVGL manda al panel.

### SW-702 Ajustes_Por_Defecto sin ajustes guardados

1. `idf.py -B build_bench -p /dev/ttyUSB0 erase-flash flash monitor` y `bench settings`.
2. **Pasa si** el log dice `sin ajustes guardados; valores por defecto` y `bench settings` muestra
   `unidad=bar bar_start=2500 shift=6000,6000,6000,6000,6000,6000`.

### SW-104 Boost_Relativo con la Resolución_Unidad

1. `bench fix map=190 baro=78` (1.12 bar).
2. Con `bench save unit` y un reinicio para cada unidad, `bench screen` en la pantalla de boost.
3. **Pasa si** muestra `1.12` en bar, `16.2` en psi y `112` en kPa.

### SW-108 Boost_Pico con la Resolución_Unidad

1. Reiniciar (el pico parte de 0), `bench fix map=190 baro=78`, esperar 1 s y luego `bench fix map=150` (0.72 bar).
2. Repetir en las tres unidades.
3. **Pasa si** el valor baja a `0.72`, `10.4` y `72`, y "Pico" sigue en `1.12`, `16.2` y `112`.

### SW-109 Arco_Boost entre 0 bar y 1.5 bar

1. En bar, `bench fix map=153 baro=78` (0.75 bar), luego `map=79` (0.01 bar) y `map=278` (2.00 bar).
2. **Pasa si** con 0.75 bar el extremo del arco queda arriba al centro (mitad de los 270°), con 0.01 bar se ve el
   primer paso del arco y con 2.00 bar el arco queda lleno mientras el número muestra `2.00`.

### SW-302 LED_Simulado encendidos

Con RPM_Inicio_Barra de 2500 y RPM_Cambio de 6000, los LED encendidos son la parte entera de
15 × (RPM − 2500) / 3500. La velocidad en 0 deja la marcha nula (SW-305 usa la menor RPM_Cambio).

1. `bench fix kmh=0` y cada RPM de la tabla, con `bench screen` en la pantalla de RPM.

   | RPM | LEDs encendidos |
   | --- | --- |
   | 2400 | 0 |
   | 2740 | 1 |
   | 4250 | 7 |
   | 5800 | 14 |
   | 5990 | 14 |

2. **Pasa si** cada foto tiene esa cantidad, de izquierda a derecha: 6 verdes, 5 amarillos y el resto rojos.

### SW-502 RPM redondeadas a 10 rpm

1. `bench fix rpm=4234`, luego `rpm=4236`.
2. **Pasa si** muestra `4230` y luego `4240`.

### SW-503 Velocidad redondeada al km/h

1. `bench fix kmh=87.4`, luego `kmh=87.6`.
2. **Pasa si** muestra `87 km/h` y luego `88 km/h`.

### SW-504 Tiempo 0–100 truncado a 0.01 s

1. En la pantalla 0–100, `bench z2h 6789`, luego `bench z2h 6799`.
2. **Pasa si** muestra `6.78 s` y luego `6.79 s`.

### SW-501 Valor nuevo dibujado en ≤ 50 ms

1. `bench live`. En cada pantalla (`bench screen next` para pasar a la siguiente): `bench latency reset`, esperar
   40 s y `bench latency`. El reset deja fuera la animación del cambio de pantalla, que redibuja la pantalla entera.
2. **Pasa si** la latencia máxima de las 1000 actualizaciones es de 50 ms o menos en las 3 pantallas. Si no pasa,
   anotar los valores: 50 ms es provisional y puede enmendarse.

### SW-701, SW-307 y SW-706 Ajustes que se conservan y ajustes que se rechazan

1. `bench save unit psi`, `bench save shift 1 5500`, `bench save bar_start 2000`; las tres responden OK.
2. `bench save shift 2 2900`, `bench save shift 2 9100` y `bench save shift 2 5550`; las tres responden
   `ESP_ERR_INVALID_ARG`.
3. Desconectar 10 s, conectar y ejecutar `bench settings`.
4. `bench save shift 2 3000` y `bench save shift 3 9000`; las dos responden OK.
5. **Pasa si** SW-701: en el paso 3 la unidad es psi, la RPM_Cambio de 1.ª es 5500, la RPM_Inicio_Barra es 2000 y
   la pantalla de boost muestra `psi`. SW-706: en el paso 3 la RPM_Cambio de 2.ª sigue en 6000. SW-307: los tres
   valores del paso 2 se rechazaron y los dos del paso 4 se aceptaron.

### SW-703 Ajustes guardados fuera de rango

1. `bench corrupt` y reiniciar.
2. **Pasa si** el log dice `ajustes guardados inválidos o de otra versión; valores por defecto` y
   `bench settings` muestra los Ajustes_Por_Defecto.

### SW-906 Backtrace antes de reiniciar por un panic

1. En `idf.py -B build_bench monitor`, `bench panic`, guardando la salida. `monitor` decodifica el backtrace con
   el `.elf` de `build_bench`; `read_log.py` usa el de `build` y aquí no sirve.
2. **Pasa si** la salida tiene el backtrace decodificado con `abort` llamado desde `bench_cmd`, y la tarjeta
   reinicia sola hasta la pantalla de boost.

### SW-904 Aviso_Sin_Datos si la fuente no inicia

1. `bench fail_source`: la tarjeta reinicia y la fuente falla al arrancar.
2. **Pasa si** se ve la pantalla de boost con `Sin datos` en rojo arriba a la derecha y el log dice
   `no se pudo iniciar la fuente de datos`.

## Parte C: inspección y CI

El PR cita el archivo y la línea, o la corrida de CI, como evidencia.

| SW | Evidencia |
| --- | --- |
| SW-704 | `nvs_open_flash` en `components/settings/src/settings.c`: borra y reintenta con `ESP_ERR_NVS_NO_FREE_PAGES` y `ESP_ERR_NVS_NEW_VERSION_FOUND` |
| SW-902 | `ESP_ERROR_CHECK(settings_init())` en `main/app_main.c` y `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y` en `sdkconfig` |
| SW-903 | `ESP_ERROR_CHECK(bsp_display_start())` en `main/app_main.c` con la misma opción |
| SW-907 | `buff_dma = true` en `bsp/wt32sc01/bsp/src/bsp.c` |
| SW-908, SW-909 | Resumen de tamaño de "Build del firmware" en la corrida de CI del PR (`check_size.py`) |

## Resultados

Copiar al PR y llenar.

Commit: ` ` · Fecha: ` ` · Log: ` `

| SW | Pasa | Evidencia | Notas |
| --- | --- | --- | --- |
| SW-104 | | | |
| SW-108 | | | |
| SW-109 | | | |
| SW-302 | | | |
| SW-306 | | | |
| SW-307 | | | |
| SW-501 | | | Latencia máxima por pantalla: |
| SW-502 | | | |
| SW-503 | | | |
| SW-504 | | | |
| SW-505 | | | 3 tiempos: |
| SW-506 | | | 3 tiempos: |
| SW-507 | | | |
| SW-701 | | | |
| SW-702 | | | |
| SW-703 | | | |
| SW-704 | | | |
| SW-706 | | | |
| SW-901 | | | 5 tiempos: |
| SW-902 | | | |
| SW-903 | | | |
| SW-904 | | | |
| SW-906 | | | |
| SW-907 | | | |
| SW-908 | | | |
| SW-909 | | | |
