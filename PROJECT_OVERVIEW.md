# Extrusion Line-Speed Monitor — Project Overview

> Purpose, architecture and key decisions behind this repo. The firmware lives in
> `SAMD21_Project/`; wiring and MCC setup are in the two reference docs at the repo root.

## 1. The problem

A metal extrusion line is pulled by a chain-and-sprocket drive off a right-angle
gearbox (about a 1" output shaft). We want to know **how many feet of material per
second each machine is producing**, live, from an office, across several machines
in an older building — without buying a $1–2k EtherNet/IP encoder for every line.

Expected speed: **about 7 ft/s (420 ft/min)**.

## 2. The approach

Put a low-cost incremental encoder on the drive shaft, and give each machine a small
microcontroller "node" that turns encoder pulses into speed and publishes it over
Ethernet. All nodes report to one MQTT broker; any computer on the network can view
the data.

```
 Encoder on 1" shaft ──A/B──► SAMD21 node ──SPI──► W5500 ──Cat6──► Field switch ─┐
                               │  16x2 LCD (local readout)                        │
                                                                                 trunk
 Office:  Raspberry Pi 4 (Mosquitto broker) ◄── Office switch ◄───────────────────┘
          └─► dashboards on any PC/laptop (Node-RED or Python GUI)
```

Why this split:

| Choice | Reason |
|---|---|
| Incremental quadrature encoder (not a tachometer) | Tachometers give 1 pulse/rev, or an analog signal; optical ones get blinded by grease and mist. An encoder gives hundreds of pulses/rev, so speed updates quickly even at low RPM. |
| MCU node instead of an Ethernet encoder | Roughly $100–250 per machine instead of $1,000–2,000; full control over the math and the payload. |
| W5500 | It has a hardware TCP/IP stack, so the MCU only runs MQTT (a few KB) — no lwIP. |
| MQTT + JSON | Human-readable, easy to debug (MQTT Explorer), and any number of viewers can subscribe without loading the nodes. |

## 3. Measurement math

```
counts/rev  = PPR × 4                     (full quadrature decoding)
RPM         = (Δcounts / counts_per_rev) / Δt_min
speed_ft_s  = rev/s × FEET_PER_REV
```

Worked example (assumes a #40 chain, 0.5" pitch, 24-tooth sprocket — **not yet
verified on the machine**):

| Quantity | Value |
|---|---|
| Pitch diameter = pitch / sin(180°/teeth) | 3.83 in |
| Feet per shaft revolution | 1.003 ft |
| Shaft speed at 7 ft/s | about 419 RPM (7 rev/s) |
| 600 PPR, 4× decoding | 2,400 counts/rev → **about 16.8 k counts/s** |
| Resolution | about 0.005 in of material per count |

**Calibrate FEET_PER_REV on the real machine; don't rely on the sprocket formula.**
The chain drives a puller, roller or capstan downstream, so the ratio between shaft
turns and material feet depends on that geometry (and any slip). Procedure: mark the
material, run N shaft revolutions, measure the feet, and store `feet_per_rev` as a
per-node configuration value (non-volatile, settable over the UART CLI — phase P5).

If "how much metal" eventually means mass:
`kg/s = speed × cross-section area × density`. That's a downstream calculation; the
node only needs to report linear speed.

## 4. Encoders

| Stage | Part | Interface notes |
|---|---|---|
| **Proof of concept (now)** | KY-040 hand knob, 20 detents, 80 counts/rev | 3.3 V, on-board pull-ups. Bench use only — mechanical, short life, not meant to be motor-driven. |
| Bench motor tests | LPD3806-600BM (about $16, 600 PPR) | Open-collector NPN outputs: needs pull-ups to **3.3 V**. Don't pull up to its 5–24 V supply, because SAMD21 pins are not 5 V tolerant. |
| Production candidate | AutomationDirect TRD-SHR600V5D (about $110, 600 PPR, 8 mm hollow shaft) | Differential line-driver outputs: needs an RS-422 receiver (e.g., a 3.3 V AM26LV32-class part). |
| Production candidate | 1" hollow-bore IP65/IP67 600 PPR encoder with a torque arm | Usually 10–30 V HTL push-pull: needs level shifting or optocouplers to 3.3 V. |

**Why 600 PPR and not 5,000:** at about 420 RPM, 600 PPR already resolves about
0.005 in of material per count. Higher PPR only adds interrupt load. Shaft speed is
far below any encoder's mechanical rating.

**Mounting:** either a hollow-bore encoder slid over the shaft end with a torque arm
tied to the frame, or a solid-shaft encoder on a bracket with a flexible coupling.
The environment has heavy grease, metal grime and vibration, so the encoder must be
**IP65 or better with sealed bearings**.

## 5. Node hardware (current proof of concept)

- SAM D21 Curiosity Nano (ATSAMD21G17D, Cortex-M0+, 48 MHz, 128 KB Flash / 16 KB RAM) on the Curiosity Nano Base for Click boards
- ETH WIZ Click (W5500) in socket 1 — added in phase P3
- 1602A HD44780 LCD, 4-bit mode, write-only
- Debug and telemetry over the on-board virtual COM port (115200 8N1)

The pin map and wiring are in *Encoder Telemetry Node Wiring & Pin Map.md*.

### Is the SAMD21 fast enough?

The SAMD21 has **no hardware quadrature decoder** (that peripheral, PDEC, exists on
the SAMD5x/E5x), so decoding is done by EIC interrupts plus a state table.

| Case | Edge rate | CPU load (about 4 µs per edge) |
|---|---|---|
| 600 PPR at 7 ft/s (420 RPM) | about 16.8 k/s | about 7% |
| 600 PPR at 2× headroom (840 RPM) | about 33.5 k/s | about 13% |
| 1000 PPR at 3,000 RPM | 200 k/s | not feasible: use an LS7366R counter or a SAME5x part with PDEC |

So the target application fits. The EIC runs from a 1 MHz clock (filter rejects
glitches shorter than about 3 µs). Contact bounce is handled by the quadrature state
table, not by the filter.

## 6. Network architecture

**Topology:** distributed star. Machines plug into a nearby DIN-rail industrial switch;
one trunk runs from each field switch to an office switch. No home-run cable per machine.

- **Switches, not a router.** Everything sits on one local subnet. A router (or
  firewall) is only needed to bridge to the corporate IT network or the internet.
- **Cable:** shielded Cat6 (STP/FTP). Keep it at least 12" from motor and VFD power
  wiring, and cross power cables at 90°.
- **Trunk length:** copper is fine up to 100 m (328 ft). Beyond that, or through
  severe EMI, use fiber with SFP or media converters.
- **Field switches:** unmanaged industrial switches (metal housing, 24 VDC, wide
  temperature range — Moxa EDS, Phoenix Contact, Siemens SCALANCE class) in an
  enclosure near the machines.
- **Shield grounding:** follow the plant's grounding and equipotential-bonding
  practice; it differs between sites. Ethernet magnetics already give about 1.5 kV of
  isolation at each port.
- **IT coordination:** in an existing building, agree with IT on whether this is an
  isolated network, a VLAN on their switches, or a separate network with a firewall.

### IP plan (static, proof-of-concept proposal)

| Device | IP |
|---|---|
| Raspberry Pi 4 — Mosquitto broker | 192.168.1.10 |
| Office viewing PCs | 192.168.1.11–.49 |
| Encoder nodes | 192.168.1.51+ |

Nodes use static IPs (or DHCP reservations) so a power cycle never changes which
address belongs to which machine. Each node's ID and IP will be stored in NVM (phase P5).

## 7. Data contract (MQTT)

| Topic | Content | QoS / flags |
|---|---|---|
| `bldg/<area>/<node_id>/telemetry` | JSON, about 10 Hz | QoS 0 |
| `bldg/<area>/<node_id>/status` | `"online"` / `"offline"` | retained; last-will message `"offline"` |

Current payload (P1, sent over the UART in the same shape the MQTT payload will use):

```json
{"count":1234,"det":308,"rev":15,"dir":1,"rpm":42.5,"btn":0}
```

Planned additions for the production encoder: `node_id`, `speed_ft_s`, `feet_per_rev`, `uptime_s`.

The JSON is built with `snprintf` using fixed-point integers (no `%f`, which drags a
large float-formatting library into XC32 builds).

## 8. Office side

- **Broker:** Mosquitto on the Raspberry Pi 4. Proof of concept:
  `listener 1883` + `allow_anonymous true`. Add usernames and passwords (and ACLs)
  before production.
- **Viewer, fastest path:** Node-RED dashboard (an `mqtt in` node feeding gauges and charts).
- **Viewer, custom:** Python + paho-mqtt + Tkinter/PySide.
- **History, later:** InfluxDB + Grafana.
- Any number of PCs can view at once; the broker fans the data out.

## 9. Phase plan

| Phase | Scope | Status |
|---|---|---|
| P0 | Clock, UART, LED bring-up | done |
| P2a | LCD driver + tick test (done early to confirm the display) | done |
| P1 | Encoder: EIC quadrature decode, count/detents/revs/direction, debounced button, RPM with timeout; UART JSON | next |
| P1b | Optional TC4 + EVSYS edge-period capture for smooth low-speed RPM | optional |
| P2 | Encoder values on the LCD without starving the encoder | |
| P3 | W5500 bring-up (ioLibrary_Driver as a submodule), Pi can ping the node | |
| P4 | MQTT telemetry + status/last-will | |
| P5 | Robustness: reconnect, watchdog, NVM config (node_id, IP, feet_per_rev), UART CLI | |
| P6 | 24 h soak test | |
| Later | Industrial 600 PPR encoder + interface circuit; enclosure; field install | |

## 10. Open questions

1. Sprocket tooth count and chain pitch — and what the chain actually drives (puller, capstan?). This sets the measured `feet_per_rev`.
2. Final encoder model: hollow-bore 1" or solid shaft with coupling? Output type (open collector, line driver or HTL)?
3. How many machines, and cable distances from each cluster to the office (copper or fiber)?
4. Plant network policy: isolated network, IT VLAN, or firewall?
5. Is linear speed (ft/s) enough, or is throughput by mass (lb/s) needed too?
6. Power at the machine: 24 VDC from the control cabinet? This decides the node's power supply and enclosure.
