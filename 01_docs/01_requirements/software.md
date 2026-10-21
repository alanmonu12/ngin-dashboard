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

_Pendiente: issue #4._

## SW-3xx Shift light

_Pendiente: issue #5._

## SW-4xx Cronómetro 0–100

_Pendiente: issue #6._

## SW-5xx UI, navegación y ajustes

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| SW-501 | Cuando cambia un valor que muestra la pantalla activa, el firmware debe dibujar el valor nuevo en menor o igual a 50 ms. | SYS-03 | ui | Demostración en la tarjeta (falta): registrar el instante de cada muestra y el del envío del cuadro por SPI; pasa si la diferencia es menor o igual a 50 ms en 1000 cambios. | Implementado | La UI refresca cada 33 ms (unos 30 Hz) y redibuja solo lo que cambió. Con este límite, la frecuencia que ve el usuario la fija el ritmo de las respuestas OBD (SW-6xx). 50 ms es provisional: falta medir el render en la tarjeta. |

_El resto (navegación, pantalla de ajustes): pendiente._
