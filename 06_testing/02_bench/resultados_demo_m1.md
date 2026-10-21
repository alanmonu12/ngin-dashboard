# Resultados de la demostración en la tarjeta (M1, issue #28)

Guion: `demo_tarjeta_m1.md`. Fecha: 2026-10-21. Tarjeta: WT32-SC01 (ESP32-WROVER-B rev. v1.0) por USB-C.

Firmware: build de banco de la rama de #56 (squash en `main` como `e2b7229`), en sus versiones intermedias de ese
día; cada sesión dice cuál usó. Las diferencias entre versiones (latencia de dibujo, parpadeo) se anotan en los SW
que afectan. La parte B la corrió Claude por el puerto serial; la parte A, Alan con la tarjeta en la mano y el log
capturado por el puerto.

## Logs

| Archivo | Qué tiene |
| --- | --- |
| `logs/2026-10-21_demo_m1_parteB_ajustes.log` | Arranque con la flash borrada, ajustes guardados y rechazados, ajustes corruptos, latencia antes del arreglo, `fail_source` y `panic` |
| `logs/2026-10-21_demo_m1_parteB_pantalla.log` | Latencia después del arreglo, valores en pantalla con `bench screen` en bar, psi y kPa, aviso de fuente que falla |
| `logs/2026-10-21_demo_m1_parteA.log` | 13 cambios de pantalla por gesto y 5 arranques desconectando el USB-C |
| `logs/2026-10-21_te_probe.log` | Prueba de la señal TE del panel (no llega a ningún GPIO); evidencia para #58 |

## Resultados

| SW | Pasa | Evidencia | Notas |
| --- | --- | --- | --- |
| SW-104 | Sí | `bench screen`: `1.12` bar, `16.2` psi, `112` kPa con MAP 190 kPa y barómetro 78 kPa | |
| SW-108 | Sí | Con MAP 150 kPa el valor baja a `0.72` / `10.4` / `72` y "Pico" sigue en `1.12` / `16.2` / `112` | |
| SW-109 | Sí | Arco en 75/150 con 0.75 bar, 1/150 con 0.01 bar y 150/150 con 2.00 bar mientras el número muestra `2.00` | |
| SW-302 | Sí | 2400, 2740, 4250, 5800 y 5990 rpm encienden 0, 1, 7, 14 y 14 LED_Simulado: `GGGGGGYYYYYRRR.` con 14 | |
| SW-306 | Sí | Alan vio los 15 LED_Simulado parpadear juntos al llegar a la RPM_Cambio. Frecuencia por análisis: `BLINK_HALF_MS` de 120 ms, 4.2 Hz | Se probó primero en rojo a 6 Hz y luego cada LED en su color a 4.2 Hz (SW-306 enmendado). Quedan líneas por tearing: #58 |
| SW-307 | Sí | `bench save shift 2` con 2900, 9100 y 5550 responde `ESP_ERR_INVALID_ARG`; con 3000 y 9000 (marcha 3), `OK` | |
| SW-501 | Sí | Latencia máxima en 1000 actualizaciones: 47 ms (boost), 45 ms (RPM), 33 ms (0–100) | Antes del arreglo de #56: 76 ms en boost. No pasaba |
| SW-502 | Sí | 4234 rpm → `4230`; 4236 rpm → `4240` | |
| SW-503 | Sí | 87.4 km/h → `87 km/h`; 87.6 km/h → `88 km/h` | |
| SW-504 | Sí | `bench z2h 6789` → `6.78 s`; `bench z2h 6799` → `6.79 s` | |
| SW-505 | Sí | `cambio de pantalla` de 150 a 205 ms en los 13 cambios por gesto | |
| SW-506 | Sí | Incluido en los 13 cambios, en los dos sentidos | |
| SW-507 | Sí | En los 13 cambios, `valores en la pantalla nueva` (50–95 ms) llega antes que `cambio de pantalla` (150–205 ms); Alan no vio valores en 0 ni viejos | Con el enunciado anterior (≤ 50 ms desde el gesto) no pasaba; SW-507 enmendado en #56 |
| SW-701 | Sí | Tras guardar psi, 5500 rpm en 1.ª y 2000 rpm de RPM_Inicio_Barra, y reiniciar, `bench settings` los muestra y la pantalla dice `psi` | Reinicio por el pin EN, no corte de alimentación: la NVS está en flash y no depende de ella |
| SW-702 | Sí | Con la flash borrada: `sin ajustes guardados; valores por defecto` y `bench settings` = bar, 2500, 6000 en cada marcha | |
| SW-703 | Sí | Tras `bench corrupt` y reiniciar: `ajustes guardados inválidos o de otra versión; valores por defecto` | |
| SW-704 | Sí | Inspección: `nvs_open_flash` en `components/settings/src/settings.c:52` borra y reintenta con `ESP_ERR_NVS_NO_FREE_PAGES` y `ESP_ERR_NVS_NEW_VERSION_FOUND` | |
| SW-706 | Sí | Después de los tres rechazos de SW-307 y un reinicio, la RPM_Cambio de 2.ª sigue en 6000 | |
| SW-901 | Sí | `primer cuadro con datos` a 2438–2445 ms desde el reset en los 5 arranques; Alan vio la pantalla de boost al conectar | No cuenta la subida de la alimentación (unos ms) |
| SW-902 | Sí | Inspección: `ESP_ERROR_CHECK(settings_init())` en `main/app_main.c:67` y `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=y` en `sdkconfig` | |
| SW-903 | Sí | Inspección: `ESP_ERROR_CHECK(bsp_display_start())` en `main/app_main.c:68`, con la misma opción | |
| SW-904 | Sí | Tras `bench fail_source`: `no se pudo iniciar la fuente de datos` y `bench screen` en boost con `aviso="Sin datos"` | |
| SW-906 | Sí | `bench panic`: backtrace por el puerto antes de reiniciar, decodificado hasta `bench_cmd` (`main/bench.c`); la tarjeta vuelve sola a la aplicación | |
| SW-907 | Sí | Inspección: `.buff_dma = true` en `bsp/wt32sc01/bsp/src/bsp.c:150` | |
| SW-908 | Sí | `check_size.py` sobre `main` (`2193ffe`): IRAM 49.7 % contra 85 %. Lo revisa CI en cada build | |
| SW-909 | Sí | `check_size.py` sobre `main` (`2193ffe`): app 52.5 % de la partición contra 80 % | |

SW-303 no entra: sus 150 ms incluyen la latencia OBD y se miden con el simulador de ECU (#43).
