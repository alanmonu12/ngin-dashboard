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

_Pendiente: issue #3._

## SW-2xx Detección de marcha

_Pendiente: issue #4._

## SW-3xx Shift light

_Pendiente: issue #5._

## SW-4xx Cronómetro 0–100

_Pendiente: issue #6._
