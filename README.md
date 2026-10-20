# ESP32 CAN Dashboard (OBD-II)

Prototipo de medidor para auto con módulo WT32-SC01 (ESP32-WROVER-B, LCD táctil 3.5" 320×480) y transceptor CAN SN65HVD230 (TWAI interno).
Funciones: medidor de boost, tacómetro con marcha y shift light por marcha, cronómetro 0–100 km/h.
Primer vehículo: Kia Forte GT 2019, 1.6 T-GDI, manual 6 + R.

Documento vivo de concepto, requerimientos y preguntas abiertas:
https://claude.ai/code/artifact/7d05a37c-db8c-4c35-bbf3-e4ddce83907b

## Estructura

| Carpeta | Contenido |
| --- | --- |
| `01_docs/01_requirements` | Exportes del documento de requerimientos |
| `01_docs/02_decisions` | Registro de decisiones (una nota por decisión) |
| `01_docs/03_references` | PIDs OBD-II, ISO 15765-4, notas de Kia, artículos |
| `01_docs/04_images` | Fotos y capturas para la documentación |
| `02_hardware/01_datasheets` | ESP32-WROVER-B, WT32-SC01, SN65HVD230, fuente buck |
| `02_hardware/02_kicad` | Proyecto KiCad (esquemático y PCB) |
| `02_hardware/03_wiring_diagrams` | Diagramas de cableado del prototipo y pinout OBD-II |
| `02_hardware/04_bom` | Lista de materiales |
| `03_firmware` | Proyecto ESP-IDF + LVGL (se crea al confirmar el módulo) |
| `04_ui/01_mockups` | Bocetos de pantallas (boost, RPM/marcha, 0–100, ajustes) |
| `04_ui/02_ui_project` | Proyecto de SquareLine Studio o EEZ Studio |
| `04_ui/03_assets` | Fuentes, íconos e imágenes de la UI |
| `05_mechanical/01_cad` | Diseño del soporte (ventila) y carcasa |
| `05_mechanical/02_stl` | Archivos para impresión 3D |
| `06_testing/01_vehicle_logs` | Logs CAN/OBD tomados en el auto (MAP, RPM, velocidad) |
| `06_testing/02_bench` | Resultados de pruebas en banco |
| `06_testing/03_videos` | Videos de referencia (p. ej. verificación 0–100). Excluidos de git |
| `07_tools/01_scripts` | Scripts de análisis de logs y utilidades |
| `07_tools/02_can_simulator` | Simulador de ECU para pruebas en banco |
