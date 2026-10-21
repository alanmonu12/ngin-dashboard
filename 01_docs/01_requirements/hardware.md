# Requisitos de hardware (HW-xx)

Requisitos del hardware derivados de los requisitos de sistema (SYS-xx) del
[documento vivo](https://claude.ai/code/artifact/7d05a37c-db8c-4c35-bbf3-e4ddce83907b).
Viven en el repo junto al esquemático (`02_hardware/02_kicad`) para cambiar en el mismo PR que el diseño que los
cumple. Mismo esquema que `software.md`.

## Reglas

- Se redactan con el skill `writing-requirements` (INCOSE GtWR v3.1): una oración, "debe", medible, sin solución
  impuesta salvo que la razón la justifique. La pieza elegida (buck, TVS, fusible) va en la razón como línea base.
- El sujeto es **"El hardware"**: el WT32-SC01, el transceptor, la fuente, las protecciones y el cable al conector
  OBD-II. Los términos con `Mayúscula_Con_Guion_Bajo` están en el glosario del documento vivo; un término nuevo se
  agrega ahí en el mismo PR.
- Cada HW-xx traza al menos a un SYS-xx, o es `derivado` y su razón dice por qué hace falta.
- Al enmendar un HW-xx se anota en su razón la fecha, el texto anterior y el motivo.
- Un ID no se reutiliza: un requisito eliminado queda con estado `Eliminado`.

## Identificadores

`HW-nnn`, tres dígitos, con una centena por área:

| Rango | Área |
| --- | --- |
| HW-1xx | Alimentación desde el OBD-II |
| HW-2xx | Protección de la entrada de alimentación |
| HW-3xx | Interfaz CAN y conector OBD-II |
| HW-4xx | Pantalla |
| HW-5xx | Ambiente (temperatura) |

## Formato

Una tabla por área, con las mismas columnas que `software.md`: la matriz de trazabilidad
(`07_tools/01_scripts/trace.py`) la lee tal cual y CI falla si una fila está mal formada.

| Columna | Contenido |
| --- | --- |
| ID | `HW-nnn` |
| Requisito | El enunciado, una oración |
| Traza | SYS-xx separados por coma, o `derivado` |
| Componente | `fuente`, `protección`, `can`, `cable`, `pantalla` o `equipo` (todo el hardware) |
| Verificación | Método (prueba, análisis, inspección o demostración) y criterio de éxito |
| Estado | `Propuesto`, `Implementado`, `Verificado` o `Eliminado` |
| Razón | Por qué existe, de dónde salen los números y si son provisionales |

- **Implementado**: está en el esquemático y en el prototipo armado.
- **Verificado**: la prueba o el análisis de su columna Verificación pasó y quedó registrado en un PR, citado como
  `PR #nn` (en `06_testing/02_bench` van los datos de banco). Sin esa cita, `trace.py` falla.
- Una verificación por **análisis** compara la hoja de datos de la pieza elegida contra el requisito. Se usa cuando
  no hay equipo para la prueba (generador de transitorios, pistola ESD); el análisis queda en el PR.

## Interfaz OBD-II

Asignación de pines del conector SAE J1962 tipo A (12 V) que usa HW-301.

| Pin J1962 | Señal | Va a |
| --- | --- | --- |
| 4 | Tierra de chasis | Tierra de la fuente |
| 5 | Tierra de señal | Tierra de la fuente y del transceptor |
| 6 | CAN_H | CANH del transceptor |
| 14 | CAN_L | CANL del transceptor |
| 16 | +12 V de batería, permanente | Entrada de la protección y de la fuente |

Del lado del WT32-SC01 (header de 2×20, paso 2.0 mm): TWAI TX en GPIO 32 (pin 6) y RX en GPIO 33 (pin 5); el
transceptor se alimenta del 3V3 del módulo y la fuente entrega 5 V a la entrada externa. Los pines del header para
3V3, 5 V y tierra están por confirmar con la hoja de datos.

**Terminación del bus.** El equipo no agrega terminación (HW-303): el bus del auto ya tiene 120 Ω en cada extremo, y
una tercera resistencia bajaría la resistencia del bus de 60 Ω a 40 Ω. En el banco, el arnés pone los dos 120 Ω, uno en
cada extremo, junto al simulador de ECU (`07_tools/02_can_simulator`).

## HW-1xx Alimentación desde el OBD-II

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| HW-101 | Mientras la Tensión_Batería_OBD está entre 9 V y 16 V [AND] la corriente de carga está entre 0 A y 1 A, el hardware debe alimentar el WT32-SC01 con una tensión entre 4.75 V y 5.25 V. | SYS-10 | fuente | Prueba con fuente variable y carga electrónica: a 9 V, 13.5 V y 16 V, con 0 A y 1 A, la salida queda entre 4.75 V y 5.25 V. | Propuesto | 5 V ± 5 % es la tolerancia de la entrada USB del WT32-SC01. 1 A da margen sobre el consumo del módulo con el backlight al 100 %, que falta medir (TBD). Línea base: un convertidor buck de 12 V a 5 V con entrada de 40 V o más (lo pide HW-203). |
| HW-102 | Cuando la Tensión_Batería_OBD regresa a un valor entre 9 V y 16 V después del arranque del motor, el hardware debe completar la Prueba_Funcional en menor o igual a 3 s. | SYS-10, SYS-29 | fuente | Prueba en el vehículo: arrancar el motor 10 veces con el equipo conectado; después de cada arranque, la pantalla de boost vuelve en 3 s o menos. | Propuesto | Al arrancar, la batería cae por debajo de 9 V durante una fracción de segundo y el ESP32 puede reiniciarse por brownout. Se acepta el reinicio si el equipo vuelve solo y a tiempo. Sin fuente programable, la prueba es con el arranque real. |
| HW-103 | Mientras el vehículo está apagado, el hardware debe limitar su corriente desde el pin 16 del conector OBD-II a menor o igual a 1 mA. | SYS-11 | fuente | Prueba con multímetro en serie con el pin 16, con el equipo en su estado de vehículo apagado. | Propuesto | Diferido (Q9): el prototipo se desconecta al bajar del auto. Con el ESP32 en deep sleep la tarjeta completa consume hasta 18 mA, así que hace falta cortar los 5 V con un interruptor de carga que el firmware apague y un evento despierte (TBD). |
| HW-104 | El hardware debe medir la Tensión_Batería_OBD entre 0 V y 20 V con un error menor o igual a 0.2 V. | SYS-11 | fuente | Prueba con fuente variable y multímetro de referencia: de 9 V a 16 V en pasos de 1 V, la lectura del firmware difiere 0.2 V o menos. | Propuesto | Sirve para detectar el vehículo apagado por tensión (menos de unos 13 V con el motor apagado) además de por falta de tráfico CAN. Línea base: divisor resistivo a un GPIO del ADC1 con la calibración de fábrica del ESP32; falta confirmar un GPIO de ADC1 libre en el header. 0.2 V TBD. |

## HW-2xx Protección de la entrada de alimentación

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| HW-201 | Cuando la Tensión_Batería_OBD pasa a 13.5 V después de −14 V durante 60 s, el hardware debe completar la Prueba_Funcional. | SYS-10 | protección | Prueba con fuente de banco limitada a 1 A: −14 V durante 60 s, luego 13.5 V y la Prueba_Funcional. | Propuesto | Valores de polaridad inversa de ISO 16750-2 para sistemas de 12 V. El conector J1962 no se puede invertir, pero el cable se arma a mano y en el banco se alimenta con fuente. Línea base: MOSFET P o diodo en serie. |
| HW-202 | Cuando la Tensión_Batería_OBD pasa a 13.5 V después de 24 V durante 60 s, el hardware debe completar la Prueba_Funcional. | SYS-10 | protección | Prueba con fuente variable: 24 V durante 60 s, luego 13.5 V y la Prueba_Funcional. | Propuesto | Arranque con puente (jump start) de ISO 16750-2 para sistemas de 12 V; sección por confirmar contra la edición vigente. |
| HW-203 | Cuando la Tensión_Batería_OBD pasa a 13.5 V después de un Pulso_Load_Dump, el hardware debe completar la Prueba_Funcional. | SYS-10 | protección | Análisis: la TVS de la entrada absorbe la energía del Pulso_Load_Dump y su tensión de recorte queda por debajo de la entrada máxima de la fuente. | Propuesto | Desconexión de la batería con el alternador cargando. Se toma la prueba con supresión centralizada (el auto ya recorta el pulso) porque es la de los vehículos actuales. Sin generador de transitorios, se verifica por análisis. |
| HW-204 | El hardware debe completar la Prueba_Funcional después de los pulsos 1, 2a, 3a y 3b de ISO 7637-2:2011 en el pin 16 del conector OBD-II, con los niveles de prueba III del anexo A. | SYS-10 | protección | Análisis: hojas de datos de la TVS, del filtro de entrada y de la fuente contra cada pulso. | Propuesto | Transitorios de la línea de batería al conmutar cargas inductivas. Nivel III TBD; anexo y tabla por confirmar contra la norma. Sin generador de transitorios, se verifica por análisis. |
| HW-205 | Si la corriente desde el pin 16 del conector OBD-II es mayor o igual a 2 A, el hardware debe interrumpirla en menor o igual a 5 s. | derivado | protección | Prueba con fuente de banco: cortocircuito a la salida de la protección; la corriente se interrumpe en 5 s o menos. | Propuesto | Una falla en el equipo o un cable pellizcado no debe quemar el fusible del auto, que comparte el pin 16 con otros circuitos. 2 A queda arriba del consumo normal (HW-101) y abajo del fusible del auto. Línea base: fusible o PTC en serie. |

## HW-3xx Interfaz CAN y conector OBD-II

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| HW-301 | El hardware debe conectarse al vehículo por un conector macho SAE J1962 tipo A con la asignación de pines de la tabla Interfaz OBD-II de este documento. | SYS-01, SYS-10 | cable | Inspección con multímetro: continuidad de cada pin del conector al punto de la tabla y sin continuidad entre pines distintos. | Propuesto | Conector del OBD-II del Forte GT. Tipo A es el de 12 V. |
| HW-302 | El hardware debe intercambiar las señales TX y RX del TWAI con el transceptor CAN por los GPIO 32 y 33 del WT32-SC01 con niveles lógicos de 3.3 V. | SYS-01 | can | Prueba en banco: el TWAI en modo self-test transmite y recibe por el transceptor; el osciloscopio muestra niveles de 0 V y 3.3 V en los dos GPIO. | Propuesto | Decisión Q11: TWAI interno con SN65HVD230, que trabaja a 3.3 V y se conecta directo, sin convertidor de nivel. GPIO 32 y 33 son los libres del header (Q1). |
| HW-303 | El hardware debe tener una Resistencia_Diferencial_CAN mayor o igual a 10 kΩ. | SYS-01 | can | Inspección con multímetro: sin alimentación y desconectado del auto, la resistencia entre los pines 6 y 14 del conector es de 10 kΩ o más. | Propuesto | Decisión 2026-10-21: el equipo no agrega terminación. El bus del auto ya está terminado en sus extremos (60 Ω entre CAN_H y CAN_L); un tercer 120 Ω lo baja a 40 Ω, fuera del rango de ISO 11898-2, y reduce la tensión diferencial. 10 kΩ es la resistencia diferencial interna mínima de un nodo en ISO 11898-2. Muchos módulos SN65HVD230 traen soldado un 120 Ω (R2) que hay que retirar. |
| HW-304 | El hardware debe tener una capacitancia de cada línea CAN a tierra menor o igual a 100 pF. | SYS-01 | can | Análisis: suma de las capacitancias de la hoja de datos del transceptor, de la protección y del cable. | Propuesto | Una protección con mucha capacitancia deforma los flancos a 500 kbit/s. Limita la elección de la TVS a una de bus CAN. 100 pF TBD. |
| HW-305 | El hardware debe completar la Prueba_Funcional después de descargas de ±8 kV por contacto en los pines 6 y 14 del conector OBD-II conforme a IEC 61000-4-2, nivel 4. | SYS-01 | can | Análisis: la TVS del bus y el transceptor soportan ±8 kV por contacto según sus hojas de datos. Prueba con pistola ESD si se consigue una. | Propuesto | El conector se manipula al conectar y desconectar el equipo en cada uso. Línea base: TVS de bus CAN entre cada línea y tierra. |
| HW-306 | Cuando se retira un cortocircuito de 60 s de cada línea CAN contra tierra y contra 16 V, el hardware debe completar la Prueba_Funcional. | SYS-01 | can | Prueba en banco: cada uno de los cuatro cortocircuitos durante 60 s, luego la Prueba_Funcional. | Propuesto | Falla de cableado en el arnés de banco o en el auto. El SN65HVD230 aguanta de −4 V a 16 V en el bus (máximo absoluto de su hoja de datos), justo hasta la tensión máxima de SYS-10. |
| HW-307 | Mientras el hardware intercambia solicitudes OBD-II con el vehículo durante 10 min, el hardware debe limitar los errores de transmisión del TWAI a 0. | SYS-01 | cable | Prueba en el vehículo con el cable de montaje: 10 min de solicitudes OBD-II con el contador de errores de transmisión del TWAI en 0 en el log. | Propuesto | El equipo va en las ventilas (Q12), lejos del conector OBD-II, y el cable es un ramal del bus. La longitud máxima del cable sale de esta prueba (TBD). |

## HW-4xx Pantalla

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| HW-401 | Mientras el backlight está al 100 %, el hardware debe tener una Luminancia_Pantalla mayor o igual a 400 cd/m². | SYS-13 | pantalla | Prueba con luminancímetro sobre una pantalla blanca. | Propuesto | La pantalla es la del WT32-SC01 y no se cambia en esta fase. 400 cd/m² TBD: falta el dato del panel y un criterio de legibilidad para SYS-13. Si el panel no llega, SYS-13 se enmienda o se cambia el montaje para darle sombra. |

## HW-5xx Ambiente

| ID | Requisito | Traza | Componente | Verificación | Estado | Razón |
| --- | --- | --- | --- | --- | --- | --- |
| HW-501 | Mientras la Temperatura_Ambiente está entre −10 °C y 85 °C, el hardware debe completar la Prueba_Funcional. | SYS-34 | equipo | Análisis: el rango de operación de cada componente de la tabla Rango de temperatura cubre de −10 °C a 85 °C. Prueba: Prueba_Funcional después de 1 h en una cámara o un horno a −10 °C y a 85 °C, si se consigue uno; si no, demostración en el auto estacionado al sol con un termómetro junto al equipo. | Propuesto | Decisión 2026-10-21 (SYS-34). Riesgo: el panel LCD y el touch suelen operar hasta 70 °C; si la hoja de datos del WT32-SC01 lo confirma, se enmienda SYS-34 o se acepta que arriba de 70 °C la pantalla pierda contraste. La fuente y las protecciones de M3 se eligen con rango de 85 °C o más. |

### Rango de temperatura

Análisis de HW-501: rango de operación de cada componente según su hoja de datos. Se completa al elegir las
piezas en M3 (#36).

| Componente | Rango de operación | Fuente | Cubre −10 °C a 85 °C |
| --- | --- | --- | --- |
| ESP32-WROVER-B | −40 °C a 85 °C | Hoja de datos de Espressif | Sí, justo en el límite |
| SN65HVD230 | −40 °C a 85 °C | Hoja de datos de TI | Sí, justo en el límite |
| Panel LCD y touch del WT32-SC01 | Por confirmar | Hoja de datos del WT32-SC01 | Por confirmar |
| Fuente de 12 V a 5 V | Por elegir | — | — |
| TVS de entrada y de bus | Por elegir | — | — |
| Fusible o PTC (HW-205) | Por elegir | — | — |

Un PTC baja su corriente de sostén con la temperatura: a 85 °C puede abrir abajo de la corriente normal del equipo.
Si la protección de HW-205 es un PTC, se elige con su curva de derating a 85 °C.
