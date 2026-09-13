# ICD — Protocolo CAN1

Documento de control de interfaz (*Interface Control Document*) del bus CAN1
entre las placas de la aviónica modular. **Es la referencia:** si el código de
una placa y este documento no coinciden, el error está en el código.

## 1. Versión

Versión actual: **0.1** (ver historial al final).

La versión se define en `include/can_protocol/can_protocol_version.h`.

| Número | Cuándo sube | Ejemplos |
|--------|-------------|----------|
| MAJOR | Cambio incompatible: una placa con la versión anterior interpretaría mal los datos. Al subir MAJOR, MINOR vuelve a 0. | Mover o quitar un campo, cambiar un tipo o una unidad, reutilizar un ID con otro significado |
| MINOR | Cambio compatible: una placa antigua puede ignorarlo sin problema. | Añadir un mensaje nuevo, usar un byte que antes era relleno |

- Todo cambio de versión va en el mismo PR que el cambio del protocolo y se
  anota en el historial (sección 7).
- Codificación en las tramas: `uint16` con MAJOR en el byte alto y MINOR en el
  bajo (`CAN_PROTOCOL_VERSION`). Se enviará en el heartbeat de cada nodo.
- Pendiente de decidir: regla de compatibilidad mientras MAJOR = 0 (exigir
  también el mismo MINOR, o pasar a 1.0 con el primer mensaje real probado).

## 2. Capa física

### 2.1 Parámetros del bus

| Parámetro | Valor |
|-----------|-------|
| Estándar | CAN FD ISO 11898-1:2015 (ISO CAN FD, no "non-ISO") |
| Formato de trama | CAN FD **sin BRS** (toda la trama a velocidad nominal) |
| Velocidad nominal | **1 Mbit/s** |
| Punto de muestreo | **87,5 %** (tolerancia ±1 %) |
| Tipo de identificador | Estándar, 11 bits |
| Tramas remotas | No se usan |
| Longitud máxima de datos | 32 bytes (pendiente de confirmar) |
| Retransmisión automática | TODO: definir por nodo (ver estado actual en 2.2) |

Todos los nodos deben usar exactamente la misma velocidad y el mismo punto de
muestreo, con una desviación máxima de ±1 % en este último (con un número
entero de tq por bit no siempre se puede dar el valor exacto). Estos valores
están en `include/can_protocol/can_bus_config.h`.

Motivo de la elección (2026-09-14):

- **1 Mbit/s:** es el máximo que contempla ISO 11898 para la fase de
  arbitraje. Sin BRS toda la trama va a esa velocidad, así que ir más rápido
  exigiría un bus muy corto y transceptores muy rápidos.
- **87,5 %:** todas las placas lo alcanzan con su reloj FDCAN actual (16, 32 y
  100 MHz), sin tocar los PLL que comparten otros periféricos. Un 80 % no es
  alcanzable con 16 ni 32 MHz dentro de la tolerancia.

Un nodo con el controlador en modo CAN clásico (no FD) no puede conectarse a
este bus: vería las tramas FD como error y generaría error frames. Un nodo en
modo FD sí puede enviar y recibir tramas en formato clásico.

### 2.2 Tiempos de bit por nodo

El prescaler y los segmentos dependen del reloj FDCAN de cada micro, así que no
son iguales en todas las placas. Cada una debe dar 1 Mbit/s y un muestreo del
87,5 % ±1 %. Los tiempos de la fase de datos no se usan (sin BRS).

| Nodo | MCU | Periférico | Reloj FDCAN | Prescaler | Sync + Seg1 + Seg2 | tq/bit | SJW | Muestreo | Retransmisión |
|------|-----|------------|-------------|-----------|--------------------|--------|-----|----------|---------------|
| Core | STM32H723 | FDCAN1 | 100 MHz (PLL2Q) | 4 | 1 + 21 + 3 | 25 | 3 | 88 % | Activada |
| Power | STM32G473 | FDCAN1 | 32 MHz (PLLQ) | 2 | 1 + 13 + 2 | 16 | 2 | 87,5 % | Activada |
| Aviónica | STM32G473 | FDCAN1 | 100 MHz (PCLK1) | 4 | 1 + 21 + 3 | 25 | 3 | 88 % | Desactivada |
| RF | STM32G473 | FDCAN1 | 16 MHz (PCLK1) | 1 | 1 + 13 + 2 | 16 | 2 | 87,5 % | Activada |

Comprobación: velocidad = reloj / (prescaler × tq/bit);
muestreo = (1 + Seg1) / tq/bit.

Cada placa lo verifica con `CAN_BUS_TIMING_IS_VALID()` de `can_bus_config.h`.
La forma recomendada es en tiempo de ejecución, justo después de inicializar
el periférico (en el bloque `USER CODE BEGIN FDCAN1_Init 2` de `fdcan.c`),
porque usa el reloj y los valores reales y detecta también un cambio al
regenerar con CubeMX:

```c
if ((hfdcan1.Init.FrameFormat != FDCAN_FRAME_FD_NO_BRS) ||
    !CAN_BUS_TIMING_IS_VALID(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN),
                             hfdcan1.Init.NominalPrescaler,
                             hfdcan1.Init.NominalTimeSeg1,
                             hfdcan1.Init.NominalTimeSeg2)) {
  Error_Handler();
}
```

Un nodo mal configurado puede tirar el bus entero, así que es preferible que
se detenga al arrancar.

### 2.3 Conexión en cada nodo

| Nodo | Periférico | RX | TX | Transceptor | Pin standby |
|------|------------|----|----|-------------|-------------|
| Core | FDCAN1 | PB8 | PB9 | TCAN1044 | PD4 `CAN1_STB` (LOW = normal, HIGH = standby) |
| Power | FDCAN1 | PA11 | PA12 | TODO | No tiene pin de standby en el `.ioc` |
| Aviónica | FDCAN1 | PA11 | PA12 | TODO | PB1 `CAN1_STB` |
| RF | FDCAN1 | PA11 | PA12 | TODO | PA9 `FDCAN1_STB` |

### 2.4 Cableado y terminación

- Topología en bus lineal (daisy chain), sin estrellas.
- Terminación de **120 Ω en los dos extremos físicos** del bus y en ningún otro
  nodo. Con el bus sin alimentar, entre CANH y CANL deben medirse ~60 Ω.
- Derivaciones (stubs) desde el bus a cada placa lo más cortas posible.
- Cable de par trenzado, impedancia característica ~120 Ω.
- TODO: conector y asignación de pines (CANH, CANL, GND).
- TODO: qué nodos están en los extremos y llevan la terminación.
- TODO: longitud total aproximada del bus.

## 3. Codificación de datos

Reglas comunes a todos los mensajes, pensadas para que el protocolo funcione
igual en cualquier micro (STM32, ESP32, PC...):

| Regla | Motivo |
|-------|--------|
| Todos los campos multibyte en **little-endian** | El orden de bytes en memoria depende del micro |
| Solo tipos de **tamaño fijo**: `uint8_t`, `int16_t`, `uint32_t`... (`<stdint.h>`, estándar C99) | `int` puede ser de 16 o 32 bits según el micro |
| Números reales como **`float32` IEEE-754** | Formato común a prácticamente todos los micros actuales |
| Serializar **campo a campo** con `can_pack.h`; nunca enviar un struct con `memcpy` ni usar `packed` | Cada compilador puede meter padding distinto entre campos |
| Booleanos como `uint8_t`: 0 = falso, 1 = verdadero | `bool` no tiene tamaño garantizado |
| Bytes de relleno a **0** al transmitir e **ignorados** al recibir | La longitud CAN FD salta a 12, 16, 20, 24, 32... |
| El receptor saca el número de campos del **ID del mensaje**, no de la longitud de la trama | Por el relleno, la longitud no indica cuántos campos hay |

Tipos disponibles en `include/can_protocol/can_pack.h` (funciones
`can_put_<tipo>` / `can_get_<tipo>`):

| Tipo | Bytes | Función |
|------|-------|---------|
| `uint8_t` / `int8_t` | 1 | `u8` / `i8` |
| `bool` | 1 | `bool` (0 = falso, 1 = verdadero; al leer, ≠ 0 es verdadero) |
| `uint16_t` / `int16_t` | 2 | `u16_le` / `i16_le` |
| `uint32_t` / `int32_t` | 4 | `u32_le` / `i32_le` |
| `uint64_t` | 8 | `u64_le` |
| `float` (IEEE-754) | 4 | `f32_le` |

Excepción conocida: los micros sin tipo de 8 bits exactos (p. ej. DSP TI C2000,
con `char` de 16 bits) no definen `uint8_t` y necesitarían una adaptación.

## 4. Nodos

Número de nodo de 4 bits, definido en `include/can_protocol/can_ids.h`.

| Node ID | Macro | Placa | Función |
|---------|-------|-------|---------|
| 0x0 | `CAN_NODE_BROADCAST` | — | Solo como destino de comandos: todos los nodos |
| 0x1 | `CAN_NODE_CORE` | Core H7 | Ordenador de vuelo, orquesta el bus |
| 0x2 | `CAN_NODE_POWER` | Potencia | TODO |
| 0x3 | `CAN_NODE_AVIONICS` | Aviónica | TODO |
| 0x4 | `CAN_NODE_RF` | Radiofrecuencia | TODO |
| 0x5–0xF | — | Libres | — |

## 5. Mapa de identificadores

Reglas generales:

- En CAN el ID es también la prioridad: **el ID más bajo gana el arbitraje**.
  Los rangos se reparten por importancia, no por orden de creación.
- **Cada ID tiene un único nodo transmisor.** Si dos nodos envían el mismo ID a
  la vez, ambos ganan el arbitraje y colisionan en el campo de datos.
- Ninguna placa escribe IDs a mano: todos se construyen con `CAN_ID()` de
  `include/can_protocol/can_ids.h`.

### 5.1 Estructura del ID

```
 bit  10  9  8 | 7  6  5  4 | 3  2  1  0
     [ clase  ] [   nodo   ] [ mensaje  ]
```

| Campo | Bits | Significado |
|-------|------|-------------|
| Clase | 10–8 | Prioridad del mensaje. Manda sobre el resto en el arbitraje |
| Nodo | 7–4 | Placa **origen**, salvo en comandos, donde es la placa **destino** |
| Mensaje | 3–0 | Número de mensaje dentro de esa clase y ese nodo (0–15) |

Así cada ID tiene un único transmisor: el nodo indicado, o el core en los
comandos (el único que envía comandos). Por eso no existen comandos con
nodo = `CAN_NODE_CORE`.

Prioridad resultante:

1. Primero decide la **clase**: cualquier mensaje crítico gana a cualquier
   comando, cualquier comando a cualquier respuesta, etc.
2. Dentro de una misma clase decide el **nodo**: a menor número, más prioridad
   (core > power > avionics > rf). En comandos, los de broadcast (nodo 0) ganan
   a los dirigidos a una placa.
3. Por último, el **número de mensaje**: el 0 es el más prioritario.

### 5.2 Clases

| Clase | Rango de IDs | Macro | Uso |
|-------|--------------|-------|-----|
| 0 | 0x000–0x0FF | `CAN_CLASS_CRITICAL` | Emergencias y abort |
| 1 | 0x100–0x1FF | `CAN_CLASS_COMMAND` | Core → nodo (nodo = destino) |
| 2 | 0x200–0x2FF | `CAN_CLASS_RESPONSE` | Nodo → core, respuesta a un comando |
| 3 | 0x300–0x3FF | `CAN_CLASS_TELEMETRY` | Datos periódicos de cada nodo |
| 4–5 | 0x400–0x5FF | — | Reservadas |
| 6 | 0x600–0x6FF | `CAN_CLASS_HEARTBEAT` | Estado y versión de cada nodo |
| 7 | 0x700–0x7FF | `CAN_CLASS_DEBUG` | Diagnóstico, no necesario en vuelo |

### 5.3 Filtros por hardware

Con filtros de tipo "ID + máscara":

| Qué aceptar | ID | Máscara |
|-------------|----|---------|
| Todos los mensajes de una clase | `CAN_ID(clase, 0, 0)` | `CAN_ID_CLASS_MASK` (0x700) |
| Todo lo que lleva un nodo en el campo nodo | `CAN_ID(0, nodo, 0)` | `CAN_ID_NODE_MASK` (0x0F0) |
| Una clase de un nodo concreto | `CAN_ID(clase, nodo, 0)` | 0x7F0 |

Ojo con el filtro por nodo: como en los comandos el campo nodo es el
**destino**, `CAN_ID(0, nodo, 0)` con máscara 0x0F0 acepta lo que envía ese
nodo **y también** los comandos que el core le manda.

Filtros típicos de una placa (que no sea el core) para recibir sus comandos:

| Filtro | ID | Máscara |
|--------|----|---------|
| Comandos dirigidos a esta placa | `CAN_ID(CAN_CLASS_COMMAND, mi_nodo, 0)` | 0x7F0 |
| Comandos de broadcast | `CAN_ID(CAN_CLASS_COMMAND, CAN_NODE_BROADCAST, 0)` | 0x7F0 |
| Mensajes críticos de cualquier nodo | `CAN_ID(CAN_CLASS_CRITICAL, 0, 0)` | 0x700 |

## 6. Mensajes

Todavía no hay mensajes definidos. Cada mensaje nuevo se añade con esta
plantilla:

### Plantilla: 0xXXX — NOMBRE_MENSAJE

- Transmisor: / Receptor(es):
- Periodo: / Longitud:

| Byte | Campo | Tipo | Unidad | Descripción |
|------|-------|------|--------|-------------|

## 7. Historial de cambios

| Versión | Fecha | Cambio |
|---------|-------|--------|
| 0.1 | 2026-09-14 | Estructura inicial: versionado, capa física (1 Mbit/s, CAN FD sin BRS, muestreo 87,5 % ±1 %) con tiempos de bit de cada placa y su verificación, reglas de codificación de datos y tipos de `can_pack.h`, nodos (core, power, avionics, rf) y estructura de IDs (clase 3 bits + nodo 4 bits + mensaje 4 bits) |
