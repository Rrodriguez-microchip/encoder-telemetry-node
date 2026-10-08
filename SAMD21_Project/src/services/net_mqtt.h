/* net_mqtt.h - publish node telemetry to the MQTT broker over Ethernet.
 *
 * Wraps the WIZnet/Paho MQTT client (ioLibrary_Driver/Internet/MQTT) on a
 * W5500 TCP socket. The payload is the SAME JSON that telemetry.c already
 * builds (telemetry_build), so UART and MQTT never drift apart; this file only
 * owns the transport and the topic.
 *
 * P4 scope (telemetry only): CONNECT to the broker, PUBLISH to
 *   bldg/<area>/<node_id>/telemetry   at QoS 0, ~1 Hz.
 * The retained /status topic + last-will ("offline") are the next P4 step.
 * Non-blocking reconnect is P5 robustness; for now a failed connect is logged
 * and the superloop carries on (encoder/LCD/UART telemetry unaffected).
 *
 * Requires MilliTimer_Handler() (the MQTT lib's 1 ms tick) to be called from
 * the TC3 tick ISR -- see timebase.c.
 */
#ifndef NET_MQTT_H
#define NET_MQTT_H

#include <stdbool.h>

/* Open the TCP socket to the broker and send MQTT CONNECT. Call once after
 * w5500_net_up(). Returns true if connected; on false the caller should log
 * and continue (do not spin). */
bool net_mqtt_connect(void);

/* Build the current telemetry line and PUBLISH it, rate-limited to ~1 Hz, then
 * service the MQTT keepalive. Call every superloop pass; self-gates on the
 * timebase. No-op while disconnected. */
void net_mqtt_task(void);

/* True once CONNECT succeeded and the link is still up. */
bool net_mqtt_is_connected(void);

#endif /* NET_MQTT_H */
