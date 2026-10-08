/* net_mqtt.c - see net_mqtt.h. */
#include <stdio.h>
#include <string.h>

#include "services/net_mqtt.h"
#include "services/telemetry.h"
#include "services/timebase.h"
#include "services/config.h"
#include "services/log.h"

/* ioLibrary MQTT (submodule). Needs its include dirs on the project path:
 *   .../Internet/MQTT  and  .../Internet/MQTT/MQTTPacket/src */
#include "MQTTClient.h"

/* ---- bring-up constants (P4) -----------------------------------------
 * Broker is hardcoded for now, same pattern as the node's own static IP in
 * w5500_port.c. P5 moves broker IP/port into services/config (NVM + CLI). */
static uint8_t        BROKER_IP[4] = { 192, 168, 1, 5 };
static const uint16_t BROKER_PORT  = 1883U;

#define MQTT_SOCKET        1          /* W5500 socket for TCP (pin map: socket 1) */
#define PUBLISH_PERIOD_MS  1000U      /* ~1 Hz; UART telemetry stays at 10 Hz */
#define KEEPALIVE_S        60
#define CMD_TIMEOUT_MS     1000U
#define RECONNECT_PERIOD_MS 3000U     /* while down, retry the broker this often */

/* MQTT client working buffers. send/read must hold a whole packet; our payload
 * is < 128 B (TELEMETRY_MAX_LEN) plus a small fixed header, so 256 is ample. */
static unsigned char s_sendbuf[256];
static unsigned char s_readbuf[256];

static Network     s_net;
static MQTTClient  s_client;
static char        s_topic[64];       /* bldg/<area>/<node_id>/telemetry */
static bool        s_connected;

bool net_mqtt_connect(void)
{
    /* Topic is fixed for this node's lifetime; build it once. */
    (void)snprintf(s_topic, sizeof s_topic, "bldg/%s/%s/telemetry",
                   config_area(), config_node_id());

    NewNetwork(&s_net, MQTT_SOCKET);
    if (ConnectNetwork(&s_net, BROKER_IP, BROKER_PORT) != 1)
    {
        log_line("mqtt: TCP connect failed");
        s_connected = false;
        return false;
    }

    MQTTClientInit(&s_client, &s_net, CMD_TIMEOUT_MS,
                   s_sendbuf, sizeof s_sendbuf,
                   s_readbuf, sizeof s_readbuf);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion       = 4;                        /* 4 = MQTT 3.1.1 */
    data.clientID.cstring  = (char *)config_node_id();
    data.keepAliveInterval = KEEPALIVE_S;
    data.cleansession      = 1;

    if (MQTTConnect(&s_client, &data) != 0)
    {
        log_line("mqtt: CONNECT rejected");
        s_connected = false;
        return false;
    }

    log_line("mqtt: connected");
    s_connected = true;
    return true;
}

void net_mqtt_task(void)
{
    static uint32_t t_last;
    static bool     started;
    static uint32_t t_retry;
    static bool     tried_once;

    if (!s_connected)
    {
        /* Lazy connect + retry forever, from inside the superloop so the WDT
         * is petted around each (now fast-failing) blocking connect attempt.
         * This is what lets the node ride out a broker that's off at boot or
         * that disappears mid-run: it just keeps retrying and reconnects the
         * moment the broker is reachable again. */
        uint32_t tnow = timebase_ms();
        if (tried_once && ((tnow - t_retry) < RECONNECT_PERIOD_MS))
        {
            return;             /* not yet time to retry; keep looping */
        }
        t_retry    = tnow;
        tried_once = true;

        if (net_mqtt_connect())
        {
            started = false;    /* publish immediately on the next pass */
        }
        return;
    }

    uint32_t now = timebase_ms();
    if (started && ((now - t_last) < PUBLISH_PERIOD_MS))
    {
        return;
    }
    t_last  = started ? (t_last + PUBLISH_PERIOD_MS) : now;
    started = true;

    char payload[TELEMETRY_MAX_LEN];
    size_t len = telemetry_build(payload, sizeof payload);
    if (len == 0U)
    {
        return;                 /* payload didn't fit; skip this tick */
    }

    MQTTMessage msg;
    msg.qos        = QOS0;      /* fire-and-forget, per the data contract */
    msg.retained   = 0;
    msg.dup        = 0;
    msg.id         = 0;
    msg.payload    = payload;
    msg.payloadlen = len;

    if (MQTTPublish(&s_client, s_topic, &msg) != 0)
    {
        /* Broker/cable dropped. Mark down; the loop keeps running and P5 will
         * own the reconnect. Avoid logging every tick -- log once on the edge. */
        log_line("mqtt: publish failed, disconnected");
        s_connected = false;
        return;
    }

    /* Service keepalive / incoming control packets. Zero timeout = don't block
     * the superloop; just process whatever is already buffered. */
    (void)MQTTYield(&s_client, 0);
}

bool net_mqtt_is_connected(void)
{
    return s_connected;
}
