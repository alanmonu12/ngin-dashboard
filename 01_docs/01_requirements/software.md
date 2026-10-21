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

Una tabla por área, con estas columnas exactas: la matriz de trazabilidad (`07_tools/01_scripts/trace.py`, M1)
la lee tal cual.

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
  CI, o una prueba en la tarjeta registrada en el PR.

## SW-1xx Medidor de boost

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-101 | El firmware debe calcular el Boost_Relativo con la última presión del múltiple de admisión y la última presión barométrica recibidas, con un error menor o igual a 0.001 bar. | SYS-02 | vehicle_logic | Prueba en host: MAP de 178 kPa con barómetro de 78 kPa da 1.00 ± 0.001 bar; un MAP nuevo usa el último barómetro recibido. | Verificado | Boost_Relativo = (MAP − barómetro) / 100 kPa/bar. OBD entrega los PID 0x0B y 0x33 en respuestas y ritmos distintos (el barómetro casi no cambia), por eso se combina el último valor de cada uno. 0.001 bar queda un orden por debajo de la resolución de SYS-02. |
| SW-102 | Si el Boost_Relativo es menor que 0 bar, el firmware debe mostrar 0 en la Unidad_Presión seleccionada. | SYS-21 | vehicle_logic | Prueba en host: MAP de 40 kPa con barómetro de 78 kPa da 0 bar. | Verificado | Decisión Q4: el medidor no tiene escala de vacío. El recorte se hace en `vehicle_logic`, así la UI y el pico ya reciben 0. |
| SW-103 | El firmware debe convertir el Boost_Relativo a la Unidad_Presión seleccionada con un error relativo menor o igual a 0.01 %. | SYS-17 | vehicle_logic | Prueba en host: 1 bar da 14.5038 psi, 100 kPa y 1 bar, cada uno ± 0.001. | Verificado | Factores de SYS-17: 1 bar = 14.5038 psi = 100 kPa. |
| SW-104 | El firmware debe mostrar el Boost_Relativo con la Resolución_Unidad de la Unidad_Presión seleccionada. | SYS-02, SYS-17 | ui | Prueba en host: 2, 1 y 0 decimales para bar, psi y kPa. Demostración en la tarjeta (falta): con un valor fijo del simulador, la pantalla muestra esos decimales en cada unidad. | Implementado | Resolución_Unidad: 0.01 bar, 0.1 psi o 1 kPa (SYS-17). La tabla de decimales vive en `vehicle_logic` y la UI la usa para formatear; pasa a Verificado con la demostración en la tarjeta. |
| SW-105 | Mientras no se ha recibido ninguna presión del múltiple de admisión, el firmware debe mostrar un Boost_Relativo de 0 bar. | derivado | vehicle_logic | Prueba en host: con solo el barómetro recibido, el Boost_Relativo y el pico quedan en 0 bar. | Verificado | Sin MAP no hay lectura: el valor inicial no debe aparecer como boost ni quedar como pico. |
| SW-106 | Mientras no se ha recibido ninguna presión barométrica, el firmware debe calcular el Boost_Relativo con una presión barométrica de 101.3 kPa. | derivado | vehicle_logic | Prueba en host (falta test): MAP de 201.3 kPa sin barómetro da 1.00 ± 0.001 bar. | Implementado | Presión estándar a nivel del mar. El PID 0x33 se pide con baja prioridad; mientras llega, el error es la diferencia de altitud (unos 0.12 bar a 1000 m). |

Lo que hace el código sin un SYS que lo pida: el **pico de boost** (`boost_peak_bar`, "Pico" en pantalla) y el
**arco de 0 a 1.5 bar**. Falta decidir si se agrega un SYS o se quita.

La frecuencia de SYS-03 (10 Hz) depende de cada cuánto se pide el PID 0x0B: queda para SW-6xx, con el cliente OBD.

## SW-2xx Detección de marcha

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-201 | Cuando el firmware recibe una RPM o una velocidad del vehículo nueva, el firmware debe recalcular la Marcha_Engranada con la última RPM y la última velocidad recibidas. | SYS-14 | vehicle_logic | Prueba en host: a 50 km/h, la RPM de cada razón calibrada da su marcha de 1.ª a 6.ª; con la RPM y la velocidad en muestras separadas se detecta la 2.ª. | Verificado | La caja manual no reporta la marcha: se calcula con los PID 0x0C y 0x0D, que OBD puede entregar en respuestas separadas. La frecuencia de 5 Hz de SYS-14 la cubren SW-501 y el ritmo de los PID (SW-6xx). |
| SW-202 | Mientras la velocidad del vehículo es menor que 10 km/h, el firmware debe mostrar "–" en lugar de la Marcha_Engranada. | SYS-14 | vehicle_logic | Prueba en host: 1200 rpm a 9 km/h dan marcha nula. | Verificado | Límite de SYS-14. Debajo de él, la resolución de 1 km/h hace saltar la Razón_RPM_Velocidad (a 5 km/h, ±1 km/h es ±20 %). |
| SW-203 | Si la Razón_RPM_Velocidad difiere en más de 5 % de la razón calibrada de cada marcha, el firmware debe mostrar "–" en lugar de la Marcha_Engranada. | SYS-15 | vehicle_logic | Prueba en host: 900 rpm a 60 km/h (razón 15, clutch pisado) dan marcha nula. | Verificado | Tolerancia de SYS-15, provisional. Cubre clutch pisado, neutral y cambio en curso. |
| SW-204 | Mientras la Razón_RPM_Velocidad difiere en menor o igual a 7 % de la razón calibrada de la Marcha_Engranada mostrada, el firmware debe conservar esa Marcha_Engranada. | derivado | vehicle_logic | Prueba en host: 6 % arriba de la razón de 3.ª da marcha nula sin marcha previa y 3.ª con la 3.ª como previa. | Verificado | Histéresis de 2 % sobre la tolerancia para que la marcha no parpadee cerca del límite. **Conflicto:** entre 5 % y 7 % contradice SYS-15, que pide "–" arriba de 5 %. Pendiente enmendar SYS-15 o quitar la histéresis. |
| SW-205 | Si la razón calibrada de una marcha es 0, el firmware debe excluir esa marcha de la detección de la Marcha_Engranada. | derivado | vehicle_logic | Prueba en host: con la razón de 3.ª en 0, la RPM y velocidad de 3.ª dan marcha nula. | Verificado | 0 significa "sin calibrar"; además evita dividir entre 0. |
| SW-206 | El firmware debe conservar la razón calibrada de cada marcha de 1.ª a 6.ª tras un corte de alimentación. | SYS-16 | settings | Demostración en la tarjeta (falta): calibrar, desconectar, encender y comparar las razones. | Propuesto | `settings` ya guarda `gear_ratio` en NVS, pero no hay calibración que lo escriba (SW-5xx). Mientras, se usan las razones estimadas por defecto o las del simulador. |

Parámetros provisionales (`vehicle.h`):

| Parámetro | Valor | Origen | Estado |
| --- | --- | --- | --- |
| Tolerancia de la razón | 5 % | SYS-15 | TBD tras medir las razones en el Forte GT |
| Histéresis | 2 % | Diseño (SW-204) | TBD junto con la tolerancia; ver el conflicto con SYS-15 |
| Velocidad mínima | 10 km/h | SYS-14 | Firme |
| Razones por defecto | 120, 70, 48, 37, 30, 25 rpm/(km/h) | Estimadas | Se reemplazan con la calibración |

## SW-3xx Shift light

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-301 | El firmware debe calcular la fracción encendida de la Barra_RPM como (RPM − RPM_Inicio_Barra) / (RPM_Cambio − RPM_Inicio_Barra), acotada entre 0 y 1, con la RPM_Cambio de la Marcha_Engranada. | SYS-18 | vehicle_logic | Prueba en host: con RPM_Inicio_Barra de 2500 y RPM_Cambio de 6000, 4250 rpm dan 0.5 ± 0.001, 2000 rpm dan 0 y 7000 rpm dan 1. | Verificado | La lógica entrega una fracción y la UI la convierte en LEDs: así se prueba en host sin LVGL. |
| SW-302 | El firmware debe encender en la Barra_RPM una cantidad de LED_Simulado igual a la parte entera de 15 por la fracción encendida de la Barra_RPM. | SYS-18 | ui | Demostración en la tarjeta (falta): barrer RPM con el simulador y contar los LED_Simulado encendidos con el criterio de SYS-18. | Implementado | 15 LED_Simulado: 6 verdes, 5 amarillos y 4 rojos (glosario, Barra_RPM). |
| SW-303 | Cuando las RPM del motor alcanzan la RPM_Cambio de la Marcha_Engranada, el firmware debe activar la Alarma_Cambio en menor o igual a 150 ms. | SYS-05 | vehicle_logic | Prueba en host: con RPM estables y RPM_Cambio de 6000, 5990 rpm no activan la alarma y 6000 rpm sí; 5800 rpm la activan en 1.ª (RPM_Cambio 5500) y no en 2.ª (6000). Prueba en banco de los 150 ms (falta): simulador CAN. | Implementado | `vehicle_logic` decide en la misma muestra; los 150 ms incluyen la latencia OBD y el dibujo, por eso solo se verifican en banco. |
| SW-304 | Mientras la Tasa_RPM es mayor que 0 rpm/s, el firmware debe activar la Alarma_Cambio cuando las RPM del motor alcanzan la RPM_Cambio de la Marcha_Engranada menos la Tasa_RPM por 75 ms. | derivado | vehicle_logic | Prueba en host, con la latencia configurada en 100 ms: subiendo a 3000 rpm/s, 5750 rpm ya activan la alarma con RPM_Cambio de 6000; bajando de 6500 a 5900 rpm no la activan. | Verificado | Compensación de latencia: cada RPM llega por OBD con 50–100 ms de retraso. 75 ms es la mitad de ese rango, provisional hasta medir la latencia con los logs de OBD. Bajando no se adelanta. Tasa_RPM con suavizado exponencial, α = 0.3, también provisional. |
| SW-305 | Si la Marcha_Engranada es nula, el firmware debe usar la menor RPM_Cambio configurada como RPM_Cambio. | derivado | vehicle_logic | Prueba en host (falta test): con marcha nula, RPM_Cambio de 5500 en 1.ª y de 6000 en las demás, 5500 rpm activan la alarma. | Implementado | Con clutch pisado o en neutral no hay marcha: avisar de más es más seguro que no avisar. |
| SW-306 | Mientras la Alarma_Cambio está activa, el firmware debe alternar los 15 LED_Simulado entre rojo y apagado con una frecuencia entre 4 Hz y 8 Hz. | SYS-05 | ui | Demostración en la tarjeta (falta): video a 60 cuadros/s o más; contar los ciclos en 2 s. | Implementado | Frecuencia de la Alarma_Cambio (glosario). La UI usa 6 Hz (medio periodo de 83 ms), muestreado cada 33 ms. |
| SW-307 | El firmware debe rechazar cada RPM_Cambio menor que 3000 rpm, mayor que 9000 rpm o que no sea múltiplo de 100 rpm. | SYS-06 | settings | Inspección de `valid()` en `settings.c`. Prueba (falta test): `settings` usa NVS y no compila en host. | Implementado | Rango y paso de SYS-06, TBD según el limitador del 1.6 T-GDI (Q14). Falta la pantalla de ajustes que permita cambiarlas (SW-5xx). |

Se verifica en host la lógica (SW-301, SW-303 sin el tiempo, SW-304 y SW-305); solo en la tarjeta, el dibujo
(SW-302, SW-306) y los 150 ms de SW-303. La frecuencia de SYS-04 (15 Hz) depende del ritmo del PID 0x0C (SW-6xx).

Sin SYS: la pantalla de RPM redondea a 10 rpm y muestra la velocidad en km/h.

## SW-4xx Cronómetro 0–100

_Pendiente: issue #6._

## SW-5xx UI, navegación y ajustes

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-501 | Cuando cambia un valor que muestra la pantalla activa, el firmware debe dibujar el valor nuevo en menor o igual a 50 ms. | SYS-03, SYS-04, SYS-14 | ui | Demostración en la tarjeta (falta): registrar el instante de cada muestra y el del envío del cuadro por SPI; pasa si la diferencia es menor o igual a 50 ms en 1000 cambios. | Implementado | La UI refresca cada 33 ms (unos 30 Hz) y redibuja solo lo que cambió. Con este límite, la frecuencia que ve el usuario la fija el ritmo de las respuestas OBD (SW-6xx). 50 ms es provisional: falta medir el render en la tarjeta. |

_El resto (navegación, pantalla de ajustes): pendiente._
