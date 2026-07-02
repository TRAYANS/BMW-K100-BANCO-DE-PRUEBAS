# BMW K100 - Módulo adaptador de señales con Arduino Nano

Proyecto para adaptar y centralizar varias señales de una BMW K100 usando un Arduino Nano.

El sketch unificado permite gestionar:

- Indicador de marchas
- Testigo combinado de aceite / temperatura
- Limpieza de señal de velocidad
- Limpieza de señal RPM
- Simulación de nivel de gasolina
- Modo banco de pruebas por USB/Serial con interfaz web

## Archivos incluidos

```text
S_BMW_K100_UNIFICADO.ino
index.html
interfaz_banco_pruebas.html
README.md
```

## Mapa de pines

### Velocidad

| Pin Arduino | Función |
|---|---|
| D2 | Entrada velocidad |
| D4 | Salida velocidad limpia |

### RPM

| Pin Arduino | Función |
|---|---|
| D3 | Entrada RPM activa a masa |
| D5 | Salida RPM limpia |

La entrada RPM está pensada para una señal que baja a masa. El código detecta el flanco de bajada mediante interrupción.

### Marchas

| Pin Arduino | Función |
|---|---|
| D6 | Entrada marcha A |
| D7 | Entrada marcha B |
| D8 | Entrada marcha C |
| D9 | Salida neutro |
| D10 | Salida 1ª |
| D11 | Salida 2ª |
| D12 | Salida 3ª |
| D13 | Salida 4ª |
| A0 | Salida 5ª |

### Testigos

| Pin Arduino | Función |
|---|---|
| A1 | Entrada temperatura |
| A2 | Entrada presión aceite |
| A3 | Salida testigo combinado |

La prioridad es:

1. Aceite: testigo fijo
2. Temperatura: testigo parpadeando
3. Sin señal: testigo apagado

Si aceite y temperatura están activos a la vez, gana aceite.

### Gasolina

| Pin Arduino | Función |
|---|---|
| A6 | Sensor 4L / reserva |
| A7 | Sensor 8L / medio |
| A4 | Control resistencia 0Ω |
| A5 | Control resistencia 40Ω |

En Arduino Nano clásico, `A6` y `A7` son solo entradas analógicas. Por eso el sketch usa `analogRead()` para leer estos sensores.

## Modo normal

Al encender el Arduino, el programa arranca en modo normal.

En este modo lee las señales reales de la moto y controla las salidas correspondientes.

## Modo banco de pruebas

El proyecto incluye una interfaz web para probar el módulo desde el ordenador:

```text
index.html
interfaz_banco_pruebas.html
```

`index.html` es la versión pensada para GitHub Pages, para que la interfaz se abra directamente al entrar en la web del proyecto.

Permite enviar comandos por USB/Serial al Arduino para forzar salidas y comprobar el cuadro/testigos sin simular señales físicas con cables.

La interfaz permite probar:

- Marchas: apagado, N, 1ª, 2ª, 3ª, 4ª, 5ª
- Testigo de aceite
- Testigo de temperatura
- Gasolina: lleno, medio, vacío
- Pulsos de velocidad
- Pulsos de RPM
- Lectura de señales reales que llegan desde la moto

### Cómo usar la interfaz

1. Cargar `S_BMW_K100_UNIFICADO.ino` en el Arduino Nano.
2. Abrir `index.html` o `interfaz_banco_pruebas.html` con Chrome o Edge.
3. Pulsar `Conectar Arduino`.
4. Seleccionar el puerto USB del Arduino.
5. Usar los botones para probar salidas.
6. Pulsar `Modo normal` para devolver el Arduino al funcionamiento real.

La comunicación Serial usa:

```text
115200 baudios
```

## Comandos Serial disponibles

La interfaz manda estos comandos al Arduino:

```text
TEST
RUN
STATUS
INPUTS
RESETCOUNTS
GEAR -1
GEAR 0
GEAR 1
GEAR 2
GEAR 3
GEAR 4
GEAR 5
OIL 0
OIL 1
TEMP 0
TEMP 1
FUEL EMPTY
FUEL MID
FUEL FULL
FUEL A4 0
FUEL A4 1
FUEL A5 0
FUEL A5 1
SPD 0
SPD 20
RPM 0
RPM 100
```

`TEST` activa el modo banco de pruebas.

`RUN` vuelve al modo normal.

`INPUTS` devuelve una lectura de las entradas reales del Arduino: velocidad, RPM, marchas, aceite, temperatura y gasolina.

`RESETCOUNTS` pone a cero los contadores de pulsos de velocidad y RPM.

`FUEL A4` y `FUEL A5` permiten probar directamente las salidas de gasolina, sin pasar por la lógica lleno/medio/vacío.

Ejemplo de respuesta de `INPUTS`:

```text
IN;D2_SPEED=1;D3_RPM=1;GEAR_A=0;GEAR_B=0;GEAR_C=0;GEAR=0;TEMP=0;OIL=0;FUEL4_ANALOG=820;FUEL8_ANALOG=790;FUEL4_ACTIVE=0;FUEL8_ACTIVE=0;SPD_COUNT=12;RPM_COUNT=45
```

## Notas importantes

- Ningún pin del Arduino Nano debe recibir 12V directamente.
- Las señales de la moto deben llegar al Arduino acondicionadas a niveles seguros.
- La entrada RPM está configurada como `INPUT`, no como `INPUT_PULLUP`, porque la señal se considera activa a masa y definida por el circuito externo.
- Si la lectura RPM genera pulsos falsos, revisar primero el acondicionamiento de la señal y después ajustar `rpmDebounceUs`.
- Si la lectura RPM se queda corta a altas revoluciones, bajar `rpmDebounceUs`.

Valores actuales:

```cpp
const unsigned int rpmDebounceUs = 500;
const unsigned int rpmOutputPulseUs = 50;
```

## Carga en Arduino IDE

Para cargarlo desde Arduino IDE:

1. Crear una carpeta llamada `S_BMW_K100_UNIFICADO`.
2. Colocar dentro el archivo `S_BMW_K100_UNIFICADO.ino`.
3. Abrir el `.ino` con Arduino IDE.
4. Seleccionar placa `Arduino Nano`.
5. Seleccionar el procesador correcto.
6. Seleccionar el puerto USB.
7. Pulsar `Subir`.

Si usas un Nano clásico antiguo, puede que necesites seleccionar:

```text
Processor: ATmega328P (Old Bootloader)
```

## Licencia

Proyecto personal para adaptación de señales BMW K100.

Puedes modificarlo y adaptarlo a tu instalación bajo tu responsabilidad.
