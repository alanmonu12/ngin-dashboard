# Requisitos de software (SW-xx)

Requisitos del firmware derivados de los requisitos de sistema (SYS-xx) del
[documento vivo](https://claude.ai/code/artifact/7d05a37c-db8c-4c35-bbf3-e4ddce83907b).
Viven en el repo para cambiar en el mismo PR que el código y el test que los verifican.

## Reglas

- Se redactan con el skill `writing-requirements` (INCOSE GtWR v3.1): una oración, "debe", medible, sin solución
  impuesta salvo que la razón la justifique.
- El sujeto es **"El firmware"**. Los términos con `Mayúscula_Con_Guion_Bajo` están en el glosario del documento
  vivo; un término nuevo se agrega ahí en el mismo PR.
- Cada SW-xx traza al menos a un SYS-xx. Un requisito sin SYS es **derivado** (una decisión de diseño necesaria,
  como la compensación de latencia) y se marca con `derivado` en la columna de traza y su razón.
- Al enmendar un SW-xx se anota en su razón la fecha, el texto anterior y el motivo.
- Un ID no se reutiliza: un requisito eliminado queda con estado `Eliminado`.

## Identificadores

`SW-nnn`, tres dígitos, con una centena por área para leerlos de un vistazo:

| Rango | Área |
| --- | --- |
| SW-1xx | Medidor de boost |
| SW-2xx | Detección de marcha |
| SW-3xx | Shift light |
| SW-4xx | Cronómetro 0–100 |
| SW-5xx | UI, navegación y ajustes |
| SW-6xx | Fuente de datos, CAN y OBD-II |
| SW-7xx | Persistencia y logs |
| SW-9xx | Arranque, errores y recursos (memoria, watchdog) |

## Formato

Una tabla por área, con estas columnas exactas: la matriz de trazabilidad (`07_tools/01_scripts/trace.py`)
la lee tal cual y CI falla si una fila está mal formada.

| Columna | Contenido |
| --- | --- |
| ID | `SW-nnn` |
| Requisito | El enunciado, una oración |
| Traza | SYS-xx separados por coma, o `derivado` |
| Componente | `vehicle_logic`, `ui`, `bsp`, `data_source`, `settings` o `main` |
| Verificación | Método (prueba, análisis, inspección o demostración) y criterio de éxito |
| Estado | `Propuesto`, `Implementado`, `Verificado` o `Eliminado` |
| Razón | Por qué existe, de dónde salen los números y si son provisionales |

Qué test lo verifica no va aquí: cada test cita sus SW-xx con un comentario `/* Verifica: SW-nnn */`
(`CODING_STYLE.md` §8), y la matriz los junta. Así no hay dos listas que mantener.

- **Implementado**: el código existe. Para pasar a **Verificado** necesita al menos un test que lo cite y pase en
  CI, o una prueba en la tarjeta registrada en un PR, citado en su verificación como `PR #nn`.

## SW-1xx Medidor de boost

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-101 | El firmware debe calcular el Boost_Relativo con la última presión del múltiple de admisión y la última presión barométrica recibidas, con un error menor o igual a 0.001 bar. | SYS-02 | vehicle_logic | Prueba en host: MAP de 178 kPa con barómetro de 78 kPa da 1.00 ± 0.001 bar; un MAP nuevo usa el último barómetro recibido. | Verificado | Boost_Relativo = (MAP − barómetro) / 100 kPa/bar. OBD entrega los PID 0x0B y 0x33 en respuestas y ritmos distintos (el barómetro casi no cambia), por eso se combina el último valor de cada uno. 0.001 bar queda un orden por debajo de la resolución de SYS-02. |
| SW-102 | Si el Boost_Relativo es menor que 0 bar, el firmware debe mostrar 0 en la Unidad_Presión seleccionada. | SYS-21 | vehicle_logic | Prueba en host: MAP de 40 kPa con barómetro de 78 kPa da 0 bar. | Verificado | Decisión Q4: el medidor no tiene escala de vacío. El recorte se hace en `vehicle_logic`, así la UI y el pico ya reciben 0. |
| SW-103 | El firmware debe convertir el Boost_Relativo a la Unidad_Presión seleccionada con un error relativo menor o igual a 0.01 %. | SYS-17 | vehicle_logic | Prueba en host: 1 bar da 14.5038 psi, 100 kPa y 1 bar, cada uno ± 0.001. | Verificado | Factores de SYS-17: 1 bar = 14.5038 psi = 100 kPa. |
| SW-104 | El firmware debe mostrar el Boost_Relativo con la Resolución_Unidad de la Unidad_Presión seleccionada. | SYS-02, SYS-17 | ui | Prueba en host: 2, 1 y 0 decimales para bar, psi y kPa. Demostración en la tarjeta (falta): con un valor fijo del simulador, la pantalla muestra esos decimales en cada unidad. | Implementado | Resolución_Unidad: 0.01 bar, 0.1 psi o 1 kPa (SYS-17). La tabla de decimales vive en `vehicle_logic` y la UI la usa para formatear; pasa a Verificado con la demostración en la tarjeta. |
| SW-105 | Mientras no se ha recibido ninguna presión del múltiple de admisión, el firmware debe mostrar un Boost_Relativo de 0 bar. | derivado | vehicle_logic | Prueba en host: con solo el barómetro recibido, el Boost_Relativo y el pico quedan en 0 bar. | Verificado | Sin MAP no hay lectura: el valor inicial no debe aparecer como boost ni quedar como pico. |
| SW-106 | Mientras no se ha recibido ninguna presión barométrica, el firmware debe calcular el Boost_Relativo con una presión barométrica de 101.3 kPa. | derivado | vehicle_logic | Prueba en host: MAP de 201.3 kPa sin barómetro da 1.00 ± 0.001 bar. | Verificado | Presión estándar a nivel del mar. El PID 0x33 se pide con baja prioridad; mientras llega, el error es la diferencia de altitud (unos 0.12 bar a 1000 m). |
| SW-107 | El firmware debe calcular el Boost_Pico con un error menor o igual a 0.001 bar. | SYS-23 | vehicle_logic | Prueba en host: después de 1.00 bar y 0.50 bar, el Boost_Pico es 1.00 ± 0.001 bar. | Verificado | El pico se calcula sobre el Boost_Relativo ya recortado a 0, así nunca es negativo. Se reinicia solo al apagar. |
| SW-108 | El firmware debe mostrar el Boost_Pico con la Resolución_Unidad de la Unidad_Presión seleccionada. | SYS-23 | ui | Demostración en la tarjeta (falta): con un pico fijo del simulador, "Pico" muestra los decimales de cada unidad. | Implementado | Misma resolución que el Boost_Relativo para poder compararlos. |
| SW-109 | El firmware debe llenar el Arco_Boost en pasos de 0.01 bar entre 0 bar y 1.5 bar. | SYS-24 | ui | Demostración en la tarjeta (falta): con 0.75 bar el arco llega a la mitad y con 2 bar queda lleno. | Implementado | Arriba de 1.5 bar el arco queda lleno y el número sigue subiendo. |

La frecuencia de SYS-03 (10 Hz) depende de cada cuánto se pide el PID 0x0B: lo pide SW-620.

## SW-2xx Detección de marcha

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-201 | Cuando el firmware recibe una RPM o una velocidad del vehículo nueva, el firmware debe recalcular la Marcha_Engranada con la última RPM y la última velocidad recibidas. | SYS-14 | vehicle_logic | Prueba en host: a 50 km/h, la RPM de cada razón calibrada da su marcha de 1.ª a 6.ª; con la RPM y la velocidad en muestras separadas se detecta la 2.ª. | Verificado | La caja manual no reporta la marcha: se calcula con los PID 0x0C y 0x0D, que OBD puede entregar en respuestas separadas. La frecuencia de 5 Hz de SYS-14 la cubren SW-501 y el ritmo de los PID (SW-621). |
| SW-202 | Mientras la velocidad del vehículo es menor que 10 km/h, el firmware debe mostrar "–" en lugar de la Marcha_Engranada. | SYS-14 | vehicle_logic | Prueba en host: 1200 rpm a 9 km/h dan marcha nula. | Verificado | Límite de SYS-14. Debajo de él, la resolución de 1 km/h hace saltar la Razón_RPM_Velocidad (a 5 km/h, ±1 km/h es ±20 %). |
| SW-203 | Si la Razón_RPM_Velocidad difiere en más de 5 % de la razón calibrada de cada marcha [AND] en más de 7 % de la razón calibrada de la Marcha_Engranada mostrada, el firmware debe mostrar "–" en lugar de la Marcha_Engranada. | SYS-15 | vehicle_logic | Prueba en host: 900 rpm a 60 km/h (razón 15, clutch pisado) dan marcha nula. | Verificado | Tolerancias de SYS-15, provisionales. Cubre clutch pisado, neutral y cambio en curso. Enmienda 2026-10-20: antes "Si la Razón_RPM_Velocidad difiere en más de 5 % de la razón calibrada de cada marcha, el firmware debe mostrar '–' en lugar de la Marcha_Engranada"; se agrega la condición de 7 % al enmendar SYS-15. |
| SW-204 | Mientras la Razón_RPM_Velocidad difiere en menor o igual a 7 % de la razón calibrada de la Marcha_Engranada mostrada, el firmware debe conservar esa Marcha_Engranada. | SYS-22 | vehicle_logic | Prueba en host: 6 % arriba de la razón de 3.ª da marcha nula sin marcha previa y 3.ª con la 3.ª como previa. | Verificado | Histéresis de 2 % sobre la tolerancia para que la marcha no parpadee cerca del límite. Enmienda 2026-10-20: la traza pasa de `derivado` a SYS-22, que se agregó para esta histéresis. |
| SW-207 | Si la Razón_RPM_Velocidad difiere en más de 7 % de la razón calibrada de la Marcha_Engranada mostrada, el firmware debe mostrar la marcha cuya razón calibrada difiere en menor o igual a 5 % de la Razón_RPM_Velocidad. | SYS-15, SYS-22 | vehicle_logic | Prueba en host: con la 3.ª como previa, 4 % arriba de la razón de 4.ª da 4.ª y 6 % arriba da marcha nula. | Verificado | La histéresis solo conserva la marcha mostrada: una marcha nueva entra con la tolerancia de 5 %. |
| SW-205 | Si la razón calibrada de una marcha es 0, el firmware debe excluir esa marcha de la detección de la Marcha_Engranada. | derivado | vehicle_logic | Prueba en host: con la razón de 3.ª en 0, la RPM y velocidad de 3.ª dan marcha nula. | Verificado | 0 significa "sin calibrar"; además evita dividir entre 0. |
| SW-206 | El firmware debe conservar la razón calibrada de cada marcha de 1.ª a 6.ª tras un corte de alimentación. | SYS-16 | settings | Demostración en la tarjeta (falta): calibrar, desconectar, encender y comparar las razones. | Propuesto | `settings` ya guarda `gear_ratio` en NVS, pero falta la Pantalla_Ajustes que lo escriba (SW-512). Mientras, se usan las razones estimadas por defecto o las del simulador. |

Parámetros provisionales (`vehicle.h`):

| Parámetro | Valor | Origen | Estado |
| --- | --- | --- | --- |
| Tolerancia de la razón | 5 % | SYS-15 | TBD tras medir las razones en el Forte GT |
| Histéresis | 2 % (7 % en total) | SYS-22 | TBD junto con la tolerancia |
| Velocidad mínima | 10 km/h | SYS-14 | Firme |
| Razones por defecto | 120, 70, 48, 37, 30, 25 rpm/(km/h) | Estimadas | Se reemplazan con la calibración |

## SW-3xx Shift light

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-301 | El firmware debe calcular la fracción encendida de la Barra_RPM como (RPM − RPM_Inicio_Barra) / (RPM_Cambio − RPM_Inicio_Barra), acotada entre 0 y 1, con la RPM_Cambio de la Marcha_Engranada. | SYS-18 | vehicle_logic | Prueba en host: con RPM_Inicio_Barra de 2500 y RPM_Cambio de 6000, 4250 rpm dan 0.5 ± 0.001, 2000 rpm dan 0 y 7000 rpm dan 1. | Verificado | La lógica entrega una fracción y la UI la convierte en LEDs: así se prueba en host sin LVGL. |
| SW-302 | El firmware debe encender en la Barra_RPM una cantidad de LED_Simulado igual a la parte entera de 15 por la fracción encendida de la Barra_RPM. | SYS-18 | ui | Demostración en la tarjeta (falta): barrer RPM con el simulador y contar los LED_Simulado encendidos con el criterio de SYS-18. | Implementado | 15 LED_Simulado: 6 verdes, 5 amarillos y 4 rojos (glosario, Barra_RPM). |
| SW-303 | Cuando las RPM del motor alcanzan la RPM_Cambio de la Marcha_Engranada, el firmware debe activar la Alarma_Cambio en menor o igual a 150 ms. | SYS-05 | vehicle_logic | Prueba en host: con RPM estables y RPM_Cambio de 6000, 5990 rpm no activan la alarma y 6000 rpm sí; 5800 rpm la activan en 1.ª (RPM_Cambio 5500) y no en 2.ª (6000). Prueba en banco de los 150 ms (falta): simulador CAN. | Implementado | `vehicle_logic` decide en la misma muestra; los 150 ms incluyen la latencia OBD y el dibujo, por eso solo se verifican en banco. |
| SW-304 | Mientras la Tasa_RPM es mayor que 0 rpm/s, el firmware debe activar la Alarma_Cambio cuando las RPM del motor alcanzan la RPM_Cambio de la Marcha_Engranada menos la Tasa_RPM por 75 ms. | derivado | vehicle_logic | Prueba en host, con la latencia configurada en 100 ms: subiendo a 3000 rpm/s, 5750 rpm ya activan la alarma con RPM_Cambio de 6000; bajando de 6500 a 5900 rpm no la activan. | Verificado | Compensación de latencia: cada RPM llega por OBD con 50–100 ms de retraso. 75 ms es la mitad de ese rango, provisional hasta medir la latencia con los logs de OBD. Bajando no se adelanta. Tasa_RPM con suavizado exponencial, α = 0.3, también provisional. |
| SW-305 | Si la Marcha_Engranada es nula, el firmware debe usar la menor RPM_Cambio configurada como RPM_Cambio. | derivado | vehicle_logic | Prueba en host: con marcha nula, RPM_Cambio de 5500 en 1.ª y de 6000 en las demás, 5500 rpm activan la alarma y 5400 rpm no. | Verificado | Con clutch pisado o en neutral no hay marcha: avisar de más es más seguro que no avisar. |
| SW-306 | Mientras la Alarma_Cambio está activa, el firmware debe alternar cada uno de los 15 LED_Simulado entre su color encendido y su color apagado con una frecuencia entre 4 Hz y 8 Hz. | SYS-05 | ui | Análisis: medio periodo de 120 ms (`BLINK_HALF_MS`, 4.2 Hz) contado con el reloj de LVGL y dibujado cada 20 ms. Demostración en la tarjeta: con el simulador, los 15 LED_Simulado parpadean juntos, cada uno en su color, al llegar a la RPM_Cambio. | Implementado | Frecuencia de la Alarma_Cambio (glosario). La UI usa 4.2 Hz (medio periodo de 120 ms, seis refrescos de 20 ms). Cambio 2026-10-21: antes 6 Hz; en la tarjeta se veían líneas negras al parpadear, por tearing: el panel no expone su señal TE a ningún GPIO, así que no se puede sincronizar, y parpadear más lento lo deja ver menos veces. Enmienda 2026-10-21 de la verificación: antes "video a 60 cuadros/s o más; contar los ciclos en 2 s". La frecuencia la fija el código; en la tarjeta basta ver que parpadea. Enmienda 2026-10-21 del enunciado: antes "Mientras la Alarma_Cambio está activa, el firmware debe alternar los 15 LED_Simulado entre rojo y apagado con una frecuencia entre 4 Hz y 8 Hz.". Parpadear en su color cambia solo el brillo y el corte del tearing se nota menos; queda abierto en el issue de tearing. |
| SW-307 | El firmware debe rechazar cada RPM_Cambio menor que 3000 rpm, mayor que 9000 rpm o que no sea múltiplo de 100 rpm. | SYS-06 | settings | Inspección de `valid()` en `settings.c`. Prueba (falta test): `settings` usa NVS y no compila en host. | Implementado | Rango y paso de SYS-06, TBD según el limitador del 1.6 T-GDI (Q14). Falta la pantalla de ajustes que permita cambiarlas (SW-5xx). |

Se verifica en host la lógica (SW-301, SW-303 sin el tiempo, SW-304 y SW-305); solo en la tarjeta, el dibujo
(SW-302, SW-306) y los 150 ms de SW-303. La frecuencia de SYS-04 (15 Hz) depende del ritmo del PID 0x0C (SW-621).


## SW-4xx Cronómetro 0–100

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-401 | Cuando la velocidad del vehículo lleva 1000 ms o más en 0 km/h, el firmware debe poner el cronómetro 0–100 km/h en Cronómetro_Armado. | SYS-07 | vehicle_logic | Prueba en host: a 0 km/h sigue sin armar a los 500 ms y queda armado a los 1000 ms; una salida en movimiento (20 km/h) no lo arma; vuelve a armarse después de una medición. | Verificado | SYS-07 pide arrancar desde 0 km/h. 1 s detenido descarta un alto momentáneo; provisional. |
| SW-402 | Cuando la velocidad del vehículo pasa de 0 km/h a un valor mayor que 0 km/h en Cronómetro_Armado, el firmware debe iniciar la medición del tiempo de 0 a 100 km/h. | SYS-07 | vehicle_logic | Prueba en host: armado, una muestra de 2 km/h pone el cronómetro a medir. | Verificado | |
| SW-403 | El firmware debe medir el tiempo de 0 a 100 km/h con un error menor o igual a ±0.2 s a partir de velocidades muestreadas a 10 Hz con resolución de 1 km/h. | SYS-08 | vehicle_logic | Prueba en host: aceleración constante de 0 a 100 km/h en 6.5 s, velocidad truncada a 1 km/h y 10 desfases de muestreo; cada resultado a ±200 ms. Integración: el modelo del simulador da ±200 ms del tiempo real. | Verificado | Límite de SYS-08 con el PID 0x0D a unos 10 Hz. La prueba en el vehículo contra una referencia externa es de SYS-08 y sigue pendiente. |
| SW-404 | El firmware debe estimar el Instante_Arranque por extrapolación lineal de las dos primeras velocidades mayores que 0 km/h, acotado entre la última muestra a 0 km/h y la primera mayor que 0 km/h. | derivado | vehicle_logic | Prueba en host: la de SW-403, con 10 desfases entre el arranque y las muestras. | Verificado | Con resolución de 1 km/h el arranque real cae entre dos muestras. La interpolación de los cruces es la decisión de diseño de SYS-08. |
| SW-405 | Cuando la velocidad del vehículo alcanza 100 km/h, el firmware debe calcular el instante del cruce de 100 km/h por interpolación lineal entre las dos últimas velocidades recibidas. | derivado | vehicle_logic | Prueba en host: la de SW-403. | Verificado | Igual que SW-404, para el final de la medición. |
| SW-406 | Si la velocidad del vehículo vuelve a 0 km/h durante la medición, el firmware debe cancelar la medición. | derivado | vehicle_logic | Prueba en host: armado, 2 y 5 km/h y otra vez 0 km/h dejan la medición cancelada. | Verificado | Un arranque abortado no es un 0–100 válido. |
| SW-407 | Si la medición del tiempo de 0 a 100 km/h supera 30 s, el firmware debe cancelar la medición. | derivado | vehicle_logic | Prueba en host: a 10 km/h constantes, la medición sigue a los 29 s y queda cancelada después de los 30 s. | Verificado | Descarta una salida lenta o un 100 km/h que nunca llega. 30 s provisional. |
| SW-408 | Cuando el contador de milisegundos de 32 bits se desborda durante la medición, el firmware debe medir el tiempo de 0 a 100 km/h con un error menor o igual a ±0.2 s. | derivado | vehicle_logic | Prueba en host: la medición empieza 3 s antes del desborde y da 6.5 s ± 200 ms. | Verificado | El contador da la vuelta a los 49.7 días encendido. Las restas en `uint32_t` lo toleran (`CODING_STYLE.md` §5). |
| SW-409 | Mientras el cronómetro 0–100 km/h no está midiendo, el firmware debe mostrar el último tiempo de 0 a 100 km/h completado. | SYS-27 | vehicle_logic | Prueba en host: después de un 0–100 completo y de volver a armarse, se sigue mostrando el mismo resultado. | Verificado | El resultado no se pierde al detenerse ni al cancelar otra medición. Sin resultados todavía se muestra 0.00 s. |
| SW-410 | Mientras el cronómetro 0–100 km/h está midiendo, el firmware debe mostrar el tiempo transcurrido desde el Instante_Arranque. | SYS-28 | vehicle_logic | Prueba en host: con el Instante_Arranque estimado en 1050 ms, a los 1500 ms se muestran 450 ms. | Verificado | El conductor ve correr el tiempo. |
| SW-411 | El firmware debe conservar los 5 menores tiempos de 0 a 100 km/h tras un corte de alimentación. | SYS-09 | settings | Demostración en la tarjeta: medir 6 tiempos, desconectar, encender y verificar que se conservan los 5 menores. | Propuesto | Sin implementar; asignado al hito M2 (Firmware completo sin CAN). La cantidad sigue TBD en SYS-09. |

## SW-5xx UI, navegación y ajustes

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-501 | Cuando cambia un valor que muestra la pantalla activa, el firmware debe dibujar el valor nuevo en menor o igual a 50 ms. | SYS-03, SYS-04, SYS-14 | ui | Demostración en la tarjeta (falta): registrar el instante de cada muestra y el del envío del cuadro por SPI; pasa si la diferencia es menor o igual a 50 ms en 1000 cambios. | Implementado | La UI copia el estado cada 20 ms, redibuja solo lo que cambió y dibuja en el mismo ciclo (`lv_refr_now`). Enmienda 2026-10-21 de la razón: antes refrescaba cada 33 ms y esperaba al temporizador de LVGL; se midió hasta 76 ms en el build de banco y bajó a 47 ms. Con este límite, la frecuencia que ve el usuario la fija el ritmo de las respuestas OBD (SW-620 a SW-622). 50 ms es provisional: falta medir el render en la tarjeta. |
| SW-502 | El firmware debe mostrar las RPM del motor redondeadas al múltiplo de 10 rpm más cercano. | SYS-25 | ui | Demostración en la tarjeta (falta): 4234 rpm del simulador se muestran como 4230. | Implementado | |
| SW-503 | El firmware debe mostrar la velocidad del vehículo redondeada al km/h entero más cercano. | SYS-26 | ui | Demostración en la tarjeta (falta): con valores fijos del simulador. | Implementado | El PID 0x0D ya es entero; el redondeo cubre al simulador. |
| SW-504 | El firmware debe mostrar el tiempo de 0 a 100 km/h truncado a 0.01 s. | SYS-27, SYS-28 | ui | Demostración en la tarjeta (falta): 6789 ms se muestran como 6.78 s. | Implementado | Truncar evita mostrar un tiempo menor que el medido. |
| SW-505 | Cuando el usuario desliza hacia la izquierda sobre la pantalla táctil, el firmware debe mostrar la pantalla siguiente del Ciclo_Pantallas en menor o igual a 300 ms. | SYS-19 | ui | Prueba en la tarjeta con el build de banco: deslizar 3 veces a la izquierda; cada línea `cambio de pantalla: N ms` del log da 300 ms o menos. | Implementado | Gesto_Cambio_Pantalla. La animación dura 150 ms; el resto del margen es para detectar el gesto y dibujar. Enmienda 2026-10-21 de la verificación: antes "video a 60 cuadros/s o más; medir del fin del gesto al fin de la animación". El log mide desde que LVGL reconoce el gesto, mientras el dedo todavía se desliza, hasta la pantalla cargada; un video no resuelve tiempos tan cortos. |
| SW-506 | Cuando el usuario desliza hacia la derecha sobre la pantalla táctil, el firmware debe mostrar la pantalla anterior del Ciclo_Pantallas en menor o igual a 300 ms. | SYS-19 | ui | Prueba en la tarjeta con el build de banco: como SW-505, deslizando a la derecha. | Implementado | Deslizar en los dos sentidos evita dar la vuelta completa para regresar. Enmienda 2026-10-21 de la verificación: antes "como SW-505" con video. |
| SW-507 | Cuando cambia la pantalla activa, el firmware debe dibujar los valores actuales en la pantalla nueva antes de que termine la animación del cambio de pantalla. | derivado | ui | Prueba en la tarjeta con el build de banco: con el simulador, cambiar de pantalla 6 veces; en cada cambio, la línea `valores en la pantalla nueva: N ms` del log da un N menor que el de la línea `cambio de pantalla: M ms` que la sigue. | Implementado | La UI solo redibuja lo que cambia; al cambiar de pantalla invalida lo último dibujado para no mostrar valores viejos. Enmienda 2026-10-21 de la verificación: antes "verificar que ningún valor aparece en 0 o atrasado" a simple vista. Enmienda 2026-10-21 del enunciado: antes "Cuando cambia la pantalla activa, el firmware debe dibujar en la pantalla nueva los valores actuales en menor o igual a 50 ms". Los valores se dibujan durante la animación de 150 ms, que redibuja la pantalla completa en cada cuadro: medidos desde el gesto daban de 50 ms a 95 ms, siempre antes del fin de la animación (150 ms a 205 ms). Lo que importa es que la pantalla nueva no llegue con valores viejos. |
| SW-508 | Cuando el usuario mantiene presionada la pantalla táctil durante 2 s o más, el firmware debe mostrar la Pantalla_Ajustes. | SYS-20 | ui | Demostración en la tarjeta (falta). | Propuesto | 2 s evita abrir los ajustes por accidente al manejar. Gesto TBD en el diseño de UI. |
| SW-509 | Mientras muestra la Pantalla_Ajustes, el firmware debe aceptar por la pantalla táctil una Unidad_Presión de bar, psi o kPa. | SYS-17, SYS-20 | ui | Demostración en la tarjeta (falta): elegir cada unidad, confirmar y verificar la pantalla de boost. | Propuesto | |
| SW-510 | Mientras muestra la Pantalla_Ajustes, el firmware debe aceptar por la pantalla táctil una RPM_Cambio para cada marcha de 1.ª a 6.ª entre 3000 rpm y 9000 rpm en pasos de 100 rpm. | SYS-06, SYS-20 | ui | Demostración en la tarjeta (falta): llevar cada marcha a 3000 y a 9000 rpm y verificar que no pasa de esos límites. | Propuesto | Rango y paso de SYS-06 (TBD según el limitador, Q14). `settings` ya rechaza lo que salga del rango (SW-307). |
| SW-511 | Mientras muestra la Pantalla_Ajustes, el firmware debe aceptar por la pantalla táctil una RPM_Inicio_Barra entre 1000 rpm y 2900 rpm en pasos de 100 rpm. | SYS-20 | ui | Demostración en la tarjeta (falta): como SW-510. | Propuesto | Debajo de la RPM_Cambio más baja (3000 rpm). El mínimo de 1000 rpm es provisional; hoy `settings` solo exige menos de 3000 rpm y habría que agregarlo a SW-703. |
| SW-512 | Mientras muestra la Pantalla_Ajustes, el firmware debe aceptar por la pantalla táctil una razón calibrada para cada marcha de 1.ª a 6.ª de 0 o entre 10.0 y 200.0 rpm/(km/h) en pasos de 0.1. | SYS-16, SYS-20 | ui | Demostración en la tarjeta (falta): editar cada razón, confirmar y verificar la marcha detectada con el simulador. | Propuesto | Decisión de arranque (2026-10-19): sin calibración guiada en esta fase, las razones son parámetros. 0 deja la marcha sin calibrar (SW-205). El rango cubre las razones estimadas (25 a 120) con margen. |
| SW-513 | Mientras muestra la Pantalla_Ajustes, el firmware debe mostrar la Razón_RPM_Velocidad actual con una resolución de 0.1 rpm/(km/h). | SYS-16 | ui | Demostración en la tarjeta (falta): con valores fijos del simulador, comparar contra RPM / velocidad. | Propuesto | Es la calibración de esta fase: manejar estable en una marcha, leer la razón y copiarla en esa marcha. |
| SW-514 | Cuando el usuario confirma los cambios de la Pantalla_Ajustes, el firmware debe guardarlos en la NVS y aplicarlos a la lógica y a la pantalla en menor o igual a 1 s. | SYS-20 | ui | Demostración en la tarjeta (falta): cambiar la unidad y una RPM_Cambio, confirmar y verificar sin reiniciar; después reiniciar y verificar que se conservan. | Propuesto | Hoy la unidad y la configuración de la lógica se leen solo al arrancar (`app_main`): aplicarlas en vivo necesita cambios en `ui` y en `main`. |
| SW-515 | Cuando el usuario sale de la Pantalla_Ajustes sin confirmar, el firmware debe conservar los ajustes anteriores. | derivado | ui | Demostración en la tarjeta (falta). | Propuesto | Salir sin querer no debe cambiar la calibración. |

La navegación (SW-505 a SW-507) ya existe; la Pantalla_Ajustes (SW-508 a SW-515) es de M2.

## SW-6xx Fuente de datos, CAN y OBD-II

Lo que convierte tramas y decide qué pedir es C puro, para probarlo en host como `vehicle_logic`; el bus, los
tiempos y la ECU real solo se prueban en banco (simulador CAN, M3) o en el vehículo (M4). Salvo SW-640 y SW-641,
nada está implementado.

Bus y direcciones:

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-601 | El firmware debe operar el controlador CAN a 500 kbit/s con identificadores de 11 bits conforme a ISO 15765-4. | SYS-01 | data_source | Prueba en banco (falta): intercambio con el simulador de ECU; prueba en el vehículo: respuesta válida a 01 00. | Propuesto | TWAI interno con el SN65HVD230 (GPIO 32/33, `bsp.h`). En modo normal: el equipo tiene que dar ACK a las respuestas. |
| SW-602 | El firmware debe enviar al bus únicamente solicitudes de modo 01 o de modo 09. | SYS-12 | data_source | Prueba en host (falta): el codificador rechaza cualquier otro modo. Inspección: una sola función envía tramas. | Propuesto | Alcance de solo lectura: no se escriben datos ni se borran códigos. |
| SW-603 | Cuando inicia la comunicación, el firmware debe enviar la solicitud 01 00 a la dirección funcional 0x7DF. | SYS-01 | data_source | Prueba en banco (falta): log del bus. | Propuesto | Identifica qué ECU responden y qué PID soportan. |
| SW-604 | Cuando recibe respuestas a la solicitud 01 00, el firmware debe dirigir las solicitudes siguientes a la dirección física de la ECU que respondió desde 0x7E8. | derivado | data_source | Prueba en host (falta): con respuestas de 0x7E8 y 0x7E9, las solicitudes van a 0x7E0. Prueba en banco (falta). | Propuesto | Decisión de arquitectura: ir directo a la ECU del motor (0x7E0 → 0x7E8) evita que respondan las demás. Si no responde 0x7E8, TBD: usar la primera que responda. |
| SW-605 | El firmware debe solicitar únicamente los PID que la ECU reporta como soportados en sus respuestas a 01 00 y 01 20. | derivado | data_source | Prueba en host (falta): con un mapa de PID soportados sin 0x33, el planificador nunca lo pide. | Propuesto | Pedir un PID no soportado gasta un timeout cada vez. 0x0B, 0x0C y 0x0D están en 01–20; 0x33 en 21–40. |
| SW-606 | Si la ECU no soporta un PID que muestra la pantalla activa, el firmware debe mostrar "–" en lugar de ese valor. | derivado | ui | Prueba en banco (falta): simulador de ECU sin 0x0B. | Propuesto | Un 0 fijo parece una lectura válida. Sin 0x33 no aplica: hay barómetro por defecto (SW-106). |

Tramas:

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-610 | El firmware debe convertir la respuesta de cada PID 0x0B, 0x0C, 0x0D y 0x33 con las fórmulas de modo 01 de SAE J1979. | SYS-02, SYS-04, SYS-14 | data_source | Prueba en host (falta): tramas de ejemplo de cada PID, incluidos los valores extremos (0 y 255 en cada byte). | Propuesto | MAP = A kPa; RPM = (256 A + B) / 4; velocidad = A km/h; barómetro = A kPa. |
| SW-611 | El firmware debe agrupar en cada solicitud solo los PID cuya respuesta cabe en una trama CAN de 8 bytes. | derivado | data_source | Prueba en host (falta): el planificador nunca arma una solicitud con respuesta de más de 7 bytes de datos. | Propuesto | Con respuesta de una trama no hace falta ISO-TP de varias tramas ni control de flujo. `01 0C 0D` cabe (6 bytes) y trae RPM y velocidad del mismo instante; `01 0B 0C 0D` ya no. |
| SW-612 | El firmware debe marcar cada muestra con el instante de recepción de la respuesta con una resolución de 1 ms o menor. | SYS-08 | data_source | Inspección; prueba en banco (falta): respuestas con retraso conocido del simulador de ECU. | Propuesto | El 0–100 interpola con estos instantes (SW-404, SW-405). Se marca al recibir, no al procesar. |

Planificador de PID (tabla "Planificador de PIDs" del documento vivo):

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-620 | Mientras la pantalla de boost está activa, el firmware debe solicitar el PID 0x0B con una frecuencia mayor o igual a 10 Hz. | SYS-03 | data_source | Prueba en host (falta): planificador con respuestas simuladas a 25 ms. Prueba en el vehículo (falta): intervalo entre respuestas en el log. | Propuesto | Con SW-501, cumple la frecuencia de SYS-03 en pantalla. |
| SW-621 | Mientras la pantalla de RPM está activa, el firmware debe solicitar los PID 0x0C y 0x0D con una frecuencia mayor o igual a 15 Hz. | SYS-04, SYS-14 | data_source | Como SW-620. | Propuesto | En una sola solicitud (SW-611). 15 Hz es de SYS-04 y cubre los 5 Hz de SYS-14. Factibilidad TBD: una ECU contesta unas 10–30 veces por segundo. |
| SW-622 | Mientras la pantalla 0–100 está activa, el firmware debe solicitar el PID 0x0D con una frecuencia mayor o igual a 10 Hz. | SYS-07, SYS-08 | data_source | Como SW-620. | Propuesto | SW-403 supone muestras a 10 Hz. |
| SW-623 | Mientras el cronómetro 0–100 km/h está midiendo, el firmware debe solicitar únicamente el PID 0x0D. | derivado | data_source | Prueba en host (falta): durante la medición, el planificador solo arma solicitudes de 0x0D. | Propuesto | Cada solicitud de otro PID le quita ritmo a la velocidad y agranda el error del 0–100. |
| SW-624 | Mientras una pantalla de datos está activa, el firmware debe solicitar cada PID de "frecuencia baja" de esa pantalla con una frecuencia mayor o igual a 1 Hz. | derivado | data_source | Prueba en host (falta). | Propuesto | Boost: 0x0C; RPM: 0x0B (tabla del documento vivo). Mantiene la lógica al día sin quitarle ritmo al PID principal. 1 Hz TBD. |
| SW-625 | El firmware debe solicitar el PID 0x33 con un periodo menor o igual a 30 s. | derivado | data_source | Prueba en host (falta). | Propuesto | La presión barométrica casi no cambia; mientras llega se usa el valor por defecto (SW-106). |

Fallas:

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-630 | Si la ECU no responde una solicitud en 100 ms, el firmware debe enviar la solicitud siguiente. | derivado | data_source | Prueba en host (falta): con una solicitud sin respuesta, la siguiente sale a los 100 ms. Prueba en banco (falta). | Propuesto | ISO 15765-4 da a la ECU 50 ms (P2CAN) para responder; 100 ms deja margen. TBD. |
| SW-631 | Si el controlador CAN entra en bus-off, el firmware debe recuperarlo y reanudar las solicitudes en menor o igual a 1 s. | derivado | data_source | Prueba en banco (falta): provocar bus-off con el bus en corto o sin terminación. | Propuesto | Un bus-off sin recuperar deja el equipo sin datos hasta reiniciarlo. |
| SW-632 | Si no recibe ninguna respuesta válida de la ECU durante 2 s o más, el firmware debe mostrar el Aviso_Sin_Datos. | SYS-30 | data_source | Prueba en banco (falta): desconectar el CAN con el equipo funcionando y medir cuándo aparece el aviso. | Propuesto | Igual que SYS-30. Con la fuente que no inicia, el aviso es SW-904. |
| SW-633 | Cuando vuelve a recibir respuestas válidas de la ECU, el firmware debe retirar el Aviso_Sin_Datos en menor o igual a 500 ms. | SYS-30 | data_source | Prueba en banco (falta): reconectar el CAN. | Propuesto | |
| SW-634 | Mientras no recibe respuestas de la ECU, el firmware debe repetir la solicitud 01 00 con un periodo de 1 s. | derivado | data_source | Prueba en host (falta); prueba en el vehículo (falta): conectar con el motor apagado y luego encenderlo. | Propuesto | El equipo puede encenderse antes que la ECU, o la ECU apagarse y volver. |

Fuentes de datos:

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-640 | El firmware debe obtener las muestras de la Fuente_Datos elegida en la configuración de compilación. | derivado | main | Inspección: `Kconfig.projbuild` y `create_source()` en `app_main`. | Implementado | Decisión de arquitectura: la UI y la lógica se desarrollan sin el SN65HVD230 ni el auto. Hoy solo existe el simulador. |
| SW-641 | Cuando la Fuente_Datos es el simulador, el firmware debe entregar RPM y velocidad cada 50 ms, MAP cada 100 ms y presión barométrica cada 30 s. | derivado | data_source | Inspección de `sim_model.h`; prueba en host (falta test): contar los campos de las muestras de `sim_model_sample`. | Implementado | Mismos ritmos y cuantización que se esperan de OBD, para que la lógica se pruebe en condiciones parecidas. |
| SW-642 | Cuando la Fuente_Datos es el replay, el firmware debe entregar las muestras de un Registro_Datos grabado con sus intervalos originales ± 5 ms. | derivado | data_source | Prueba en la tarjeta (falta): reproducir un registro y comparar los instantes. | Propuesto | Repetir en banco lo que pasó en el auto. Dónde se guarda el registro a reproducir: TBD. |

## SW-7xx Persistencia y logs

Ajustes en NVS (`settings`):

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-701 | El firmware debe conservar la Unidad_Presión, la RPM_Cambio de cada marcha y la RPM_Inicio_Barra tras un corte de alimentación. | SYS-20 | settings | Demostración en la tarjeta (falta): cambiar cada ajuste, desconectar, encender y comparar. | Implementado | `settings_save` guarda el struct completo en NVS y `settings_init` lo lee al arrancar. Falta la pantalla de ajustes que llame a `settings_save` (SW-5xx). Las razones calibradas son SW-206. |
| SW-702 | Si no hay ajustes guardados, el firmware debe usar los Ajustes_Por_Defecto. | derivado | settings | Inspección de `settings_init`. Demostración en la tarjeta (falta): borrar la flash y arrancar. | Implementado | Primer arranque o NVS recién borrada. Los valores por defecto son provisionales: las razones son estimadas y la RPM_Cambio depende del limitador (Q14). |
| SW-703 | Si los ajustes guardados están fuera de rango, el firmware debe usar los Ajustes_Por_Defecto. | derivado | settings | Inspección de `valid()` en `settings.c`. Prueba (falta test): `settings` usa NVS y no compila en host. | Implementado | Un ajuste corrupto no debe llegar a la lógica. Rangos: Unidad_Presión conocida, RPM_Cambio de SW-307, RPM_Inicio_Barra menor que 3000 rpm y razones mayores o iguales a 0. |
| SW-704 | Si la NVS no tiene páginas libres o tiene el formato de otra versión de ESP-IDF, el firmware debe borrarla y volver a inicializarla. | derivado | settings | Inspección de `nvs_open_flash` en `settings.c`. | Implementado | Es la recuperación que recomienda ESP-IDF; pierde los ajustes, pero sin ella el equipo no arranca (SW-902). |
| SW-705 | Cuando los ajustes guardados tienen una versión anterior a la del firmware, el firmware debe conservar la razón calibrada de cada marcha y la RPM_Cambio de cada marcha guardadas. | derivado | settings | Prueba en la tarjeta (falta): guardar ajustes con la versión 1, cargar un firmware con la versión 2 y comparar. | Propuesto | El documento vivo pide versionar los ajustes "para agregar campos sin perder la calibración". Hoy `settings_init` descarta todo lo de otra versión y carga los Ajustes_Por_Defecto. Falta migrar de una versión a la siguiente. |
| SW-706 | Si un ajuste a guardar está fuera de rango, el firmware debe conservar los ajustes guardados anteriores. | derivado | settings | Inspección: `settings_save` valida antes de escribir y regresa `ESP_ERR_INVALID_ARG`. | Implementado | Un error en la pantalla de ajustes no debe dañar la calibración. Qué es fuera de rango: SW-703. |

Registro de datos (Q13). Nada está implementado todavía; asignado a M2:

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-711 | El firmware debe guardar en el Registro_Datos cada muestra recibida de la fuente de datos con su instante en ms. | SYS-32 | data_source | Prueba en host (falta): con un registro de 4 muestras, las 4 se leen en orden con su instante. | Propuesto | Guarda la muestra tal como llega: los campos que no trae quedan vacíos (OBD entrega cada PID a su ritmo). |
| SW-712 | Mientras el Registro_Datos está lleno, el firmware debe reemplazar la muestra más antigua con cada muestra nueva. | SYS-32 | data_source | Prueba en host (falta): con capacidad de 4 muestras, después de 6 se leen la 3.ª a la 6.ª. | Propuesto | Buffer circular: siempre quedan las lecturas más recientes. |
| SW-713 | El firmware debe tener un Registro_Datos con capacidad para 10 min o más de muestras al ritmo máximo de la fuente de datos. | SYS-32 | data_source | Análisis: capacidad del buffer entre el ritmo máximo de muestras de la fuente. | Propuesto | Una muestra (`vehicle_sample_t`) ocupa 24 B: a 50 muestras/s, 10 min son unos 0.7 MB de los 8 MB de PSRAM. Ritmo TBD con el planificador de PIDs (SW-620 a SW-625). |
| SW-714 | Cuando recibe el pedido de volcado por el puerto serial, el firmware debe enviar el Registro_Datos en el Formato_CSV_Registro, de la muestra más antigua a la más reciente. | SYS-33 | main | Demostración en la tarjeta (falta): pedir el volcado con `read_log.py` y abrir el CSV. | Propuesto | El texto del pedido y la velocidad del puerto quedan para el diseño: en CSV, 10 min de registro pasan de 1 MB y a 115200 baud tardan más de 1 min. |

SW-206 (razones calibradas) y SW-411 (5 mejores tiempos) se quedan en SW-2xx y SW-4xx: moverlos obligaría a
eliminarlos y crear IDs nuevos sin ganar nada. SW-7xx cubre el mecanismo que comparten.

## SW-9xx Arranque, errores y recursos

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-901 | Cuando se aplica la alimentación, el firmware debe mostrar la pantalla de boost en menor o igual a 3 s. | SYS-29 | main | Prueba en la tarjeta con el build de banco: 5 arranques; la línea `primer cuadro con datos: N ms desde el reset` del log da 3000 ms o menos. Demostración: al conectar la alimentación se ve la pantalla de boost. | Implementado | Orden de arranque en `app_main`: ajustes, pantalla, inicio, fuente, lógica y UI. 3 s TBD, igual que SYS-29: falta medirlo (incluye el bootloader y la prueba de la PSRAM). Enmienda 2026-10-21 de la verificación: antes "video desde que se conecta la alimentación". El log cuenta desde el reset con el bootloader incluido; no cubre la subida de la alimentación, de unos ms. |
| SW-902 | Si falla la inicialización de la memoria no volátil, el firmware debe reiniciar el ESP32. | derivado | main | Inspección: `ESP_ERROR_CHECK(settings_init())` en `app_main` con `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`. | Implementado | Sin NVS no hay ajustes ni calibración. `settings_init` ya borra y reintenta una NVS llena o de otra versión: si aun así falla, es la flash. Va antes que la pantalla, así que no puede avisar en ella; el aviso queda en el log. |
| SW-903 | Si falla la inicialización de la pantalla, el firmware debe reiniciar el ESP32. | derivado | main | Inspección: `ESP_ERROR_CHECK(bsp_display_start())` en `app_main` con `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`. | Implementado | Sin pantalla el equipo no tiene función. Una falla del touch no cuenta: se avisa en el log y la pantalla sigue mostrando datos. |
| SW-904 | Si la fuente de datos no se puede crear o iniciar, el firmware debe mostrar el Aviso_Sin_Datos sobre la pantalla de boost. | SYS-30 | main | Demostración en la tarjeta (falta): build de prueba con `data_source_start` que regresa error. | Implementado | Decisión de arquitectura: una falla de CAN o de la ECU degrada con aviso y la pantalla sigue. El aviso cuando dejan de llegar datos con la fuente ya iniciada es SW-632. |
| SW-905 | Si una tarea no cede la CPU durante 5 s o más, el firmware debe reiniciar el ESP32. | SYS-31 | main | Prueba en la tarjeta (falta): build de prueba con un bucle sin ceder en la tarea de LVGL; pasa si reinicia en 6 s o menos y el log muestra el backtrace. | Propuesto | El watchdog de tareas vigila las tareas idle de los dos núcleos a 5 s, pero hoy solo avisa por el log (`CONFIG_ESP_TASK_WDT_PANIC` desactivado): la pantalla se queda congelada. Falta activarlo. |
| SW-906 | Antes de reiniciar por un panic, el firmware debe enviar el backtrace por el puerto serial. | derivado | main | Inspección de `sdkconfig` (`CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`). Demostración con `read_log.py`, que decodifica el backtrace con el `.elf` del build. | Implementado | Diagnóstico de reinicios en banco y en el auto. |
| SW-907 | El firmware debe ubicar los buffers de dibujo de la pantalla en RAM interna con acceso por DMA. | derivado | bsp | Inspección: `buff_dma = true` en `bsp.c`. | Implementado | El DMA de SPI del ESP32 no lee PSRAM. Dos buffers de 40 líneas de 480 píxeles a 16 bits: 76.8 KB de RAM interna. |
| SW-908 | El firmware debe ocupar menor o igual a 85 % de la IRAM. | derivado | main | Análisis en CI en cada build: `check_size.py`. | Implementado | La IRAM es de 128 KB y ya se llenó una vez (LVGL en IRAM no cupo). Hoy, 50 %. |
| SW-909 | El firmware debe ocupar menor o igual a 80 % de la partición de app más pequeña. | derivado | main | Análisis en CI en cada build: `check_size.py`. | Implementado | Margen para crecer sin cambiar la tabla de particiones. Hoy, 52 %. |

SW-908 y SW-909 se revisan en cada build de CI, pero `trace.py` solo cuenta tests de host o pruebas en la tarjeta
registradas: por eso siguen en Implementado.
