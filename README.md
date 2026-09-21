![AlternaDC — Monitoreo de Doble Alternador](media/portada_alternadc.jpg)

# ⚡ AlternaDC
### Monitoreo y Control de Energía — DC Laboratory

AlternaDC es un sistema local para el **monitoreo simultáneo de dos alternadores o fuentes DC** con un ESP32 y dos sensores INA226. Su tablero web funciona sin internet y muestra voltaje, corriente, potencia, energía acumulada y carga estimada en ambos canales.

> **Acceso local:** conecta un celular, tablet, PC o Raspberry Pi a la red Wi-Fi `AlternaDC` y abre [http://192.168.4.1](http://192.168.4.1).

## Funciones principales

| Canal | Dirección I²C | Color | Datos |
|---|---:|---|---|
| **Alternador Principal** | `0x40` | Azul neón `#00ccff` | V, A, W, Wh y % de carga |
| **Alternador Secundario** | `0x41` | Amarillo neón `#ffdd00` | V, A, W, Wh y % de carga |

La interfaz presenta las dos tarjetas a la vez, una gráfica comparativa con curvas superpuestas, alertas independientes por canal y un indicador de conexión. Todos los archivos del tablero se sirven desde LittleFS, sin dependencias externas ni CDN.

## Control automático de luces

El relé conectado al pin configurable `RELAY_PIN` se gobierna por el nivel de voltaje del sistema. La interfaz permite elegir `AUTO`, `ENCENDER` o `APAGAR`; en modo automático se aplica lo siguiente:

| Estado | Voltaje | Acción |
|---|---:|---|
| Carga plena | `> 13.8 V` | Enciende luces extras o tiras LED |
| Carga media | `12.8–13.7 V` | Mantiene únicamente las luces esenciales |
| Batería baja | `< 12.7 V` | Apaga las luces extras automáticamente |

## Avisos sonoros

La bocina piezoeléctrica usa `BUZZER_PIN`. Al arrancar se reproduce un tono de confirmación de dos notas; la carga plena usa un aviso doble; el voltaje bajo genera un aviso cada 30 segundos y los fallos de lectura se muestran por canal. El botón de campana del tablero silencia o habilita las alertas.

## Hardware y conexiones

![Diagrama de conexión](media/diagrama_conexion.jpg)

| Componente | ESP32 / conexión |
|---|---|
| INA226 Principal SDA/SCL | GPIO21 / GPIO22, dirección `0x40` |
| INA226 Secundario SDA/SCL | GPIO21 / GPIO22, dirección `0x41` |
| Relé de luces | GPIO26 (`RELAY_PIN`) |
| Bocina piezoeléctrica | GPIO27 (`BUZZER_PIN`) |
| Shunt de cada canal | En el lado de corriente correspondiente; configurar `SHUNT_OHMS` |
| Alimentación | Convertidor automotriz protegido de 12 V a 5 V/3.3 V |

Los dos INA226 comparten SDA/SCL y deben tener direcciones distintas mediante el puente de dirección del módulo. Usa fusible, protección contra inversión de polaridad y TVS en instalaciones automotrices. No conectes directamente la batería de un vehículo al pin de 3.3 V del ESP32.

![Ejemplo de tablero](media/tablero_vivo.jpg)

## Compilación y carga

Se recomienda [PlatformIO](https://platformio.org/) con VS Code o su CLI:

```bash
pio run
pio run -t upload
pio run -t uploadfs
pio device monitor -b 115200
```

La orden `uploadfs` es necesaria para copiar `data/` (la interfaz web) a LittleFS. Después del arranque, el ESP32 crea el punto de acceso `AlternaDC` con contraseña `alternadc` y sirve el tablero en `192.168.4.1`.

## Estructura del repositorio

```text
AlternaDC/
├── data/                     # Archivos web cargados en LittleFS
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   └── logo_alternadc.png
├── firmware/
│   └── AlternaDC.ino        # Firmware dual INA226 para ESP32
├── hardware/
│   └── esquema_conexion.png
├── media/                    # Fotografías y recursos del proyecto
├── web/                      # Fuente de la interfaz web
│   ├── index.html
│   ├── style.css
│   └── app.js
├── platformio.ini
├── README.md
└── LICENSE
```

## Imágenes del proyecto

![Gráficas comparativas](media/graficas_comparativas.jpg)

Las fotografías incluidas muestran el ESP32 Dev Kit, los módulos INA226, los shunts de 50–100 A, la separación visual azul/amarilla y las conexiones de relé y bocina. El logo y las imágenes suministradas se conservan en `media/`.

## Créditos y licencia

AlternaDC es una adaptación enfocada en doble canal del proyecto original [12VBatteryMonitor](https://github.com/tipih/12VBatteryMonitor), conservando su licencia y atribución en [LICENSE](LICENSE). Repositorio oficial: [github.com/dcg0/AlternaDC](https://github.com/dcg0/AlternaDC).

© 2026 DC Laboratory.
