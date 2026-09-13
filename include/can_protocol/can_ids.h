/*
 * Nodos e identificadores del bus CAN1 (ver ICD.md, secciones 4 y 5).
 *
 * Ninguna placa escribe IDs a mano: todos se construyen con CAN_ID().
 *
 * Estructura del ID estándar de 11 bits:
 *
 *    bit  10  9  8 | 7  6  5  4 | 3  2  1  0
 *        [ clase  ] [   nodo   ] [ mensaje  ]
 *         3 bits      4 bits       4 bits
 *
 * - clase:   prioridad. En CAN gana el ID más bajo, así que la clase manda
 *            sobre todo lo demás en el arbitraje.
 * - nodo:    placa ORIGEN del mensaje, salvo en los comandos, donde es la
 *            placa DESTINO (el origen de los comandos es siempre el core).
 * - mensaje: número de mensaje dentro de esa clase y ese nodo (0–15).
 *
 * Con esta estructura cada ID tiene un único transmisor: el nodo del campo
 * "nodo", o el core en los comandos.
 */

#ifndef CAN_IDS_H
#define CAN_IDS_H

/* ------------------------------------------------------------------------
 * Nodos (4 bits)
 * ------------------------------------------------------------------------ */
#define CAN_NODE_BROADCAST 0x0U /* solo como destino de comandos: todos */
#define CAN_NODE_CORE 0x1U      /* core H7: ordenador de vuelo */
#define CAN_NODE_POWER 0x2U     /* placa de potencia */
#define CAN_NODE_AVIONICS 0x3U  /* placa de aviónica */
#define CAN_NODE_RF 0x4U        /* placa de radiofrecuencia */
/* 0x5–0xF libres */

/* ------------------------------------------------------------------------
 * Clases (3 bits), de más a menos prioridad
 * ------------------------------------------------------------------------ */
#define CAN_CLASS_CRITICAL 0x0U  /* emergencias y abort */
#define CAN_CLASS_COMMAND 0x1U   /* core -> nodo (nodo = destino) */
#define CAN_CLASS_RESPONSE 0x2U  /* nodo -> core, respuesta a un comando */
#define CAN_CLASS_TELEMETRY 0x3U /* datos periódicos de cada nodo */
/* 0x4–0x5 reservadas */
#define CAN_CLASS_HEARTBEAT 0x6U /* estado y versión de cada nodo */
#define CAN_CLASS_DEBUG 0x7U     /* diagnóstico, no necesario en vuelo */

/* ------------------------------------------------------------------------
 * Construcción y lectura de IDs
 * ------------------------------------------------------------------------ */
#define CAN_ID_CLASS_SHIFT 8U
#define CAN_ID_NODE_SHIFT 4U

#define CAN_ID_CLASS_MASK 0x700U
#define CAN_ID_NODE_MASK 0x0F0U
#define CAN_ID_MSG_MASK 0x00FU

#define CAN_ID(cls, node, msg)                                                 \
  ((((cls) & 0x7U) << CAN_ID_CLASS_SHIFT) |                                    \
   (((node) & 0xFU) << CAN_ID_NODE_SHIFT) | ((msg) & 0xFU))

#define CAN_ID_GET_CLASS(id) (((id) & CAN_ID_CLASS_MASK) >> CAN_ID_CLASS_SHIFT)
#define CAN_ID_GET_NODE(id) (((id) & CAN_ID_NODE_MASK) >> CAN_ID_NODE_SHIFT)
#define CAN_ID_GET_MSG(id) ((id) & CAN_ID_MSG_MASK)

/*
 * Máscaras para filtros por hardware (filtro "ID + máscara"):
 * - Todos los mensajes de una clase:  ID = CAN_ID(cls, 0, 0), máscara = CAN_ID_CLASS_MASK
 * - Todo lo que envía un nodo:        ID = CAN_ID(0, node, 0), máscara = CAN_ID_NODE_MASK
 * - Una clase de un nodo concreto:    ID = CAN_ID(cls, node, 0),
 *                                     máscara = CAN_ID_CLASS_MASK | CAN_ID_NODE_MASK
 */

/* ------------------------------------------------------------------------
 * Mensajes
 * Se definen aquí según se añadan al ICD, por ejemplo:
 *   #define CAN_ID_POWER_HEARTBEAT CAN_ID(CAN_CLASS_HEARTBEAT, CAN_NODE_POWER, 0x0U)
 * ------------------------------------------------------------------------ */

#endif /* CAN_IDS_H */
