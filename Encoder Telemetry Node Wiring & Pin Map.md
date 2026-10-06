# Encoder Telemetry Node: Wiring & Pin Map

Oct 5, 2026 · @Alejandro

## Overview

The ETH WIZ Click sits in socket 1; the encoder and LCD use the empty socket 2 and 3 headers, so no soldering is needed.

&#91;embedded content: system topology · node, network, broker\]

The base board routes every socket to the SAMD21. The Ethernet link goes straight to the Pi for the POC; the switch and PIC32CZ join later.

## Pin map

Every signal maps to a unique SAMD21 pin, and every interrupt input sits on its own EXTINT line (2, 3, 4, 6). Use the Signal names below as the custom pin names in the MCC Pin Manager.

| Signal | SAMD21 pin | Base-board net (where to wire) | Function | EXTINT |
| --- | --- | --- | --- | --- |
| ETH\_SCK | PA17 | SCK (socket 1) | SERCOM1 PAD1 | — |
| ETH\_MOSI | PA16 | MOSI (socket 1) | SERCOM1 PAD0 | — |
| ETH\_MISO | PA19 | MISO (socket 1) | SERCOM1 PAD3 | — |
| ETH\_CS | PA18 | CS1 (socket 1) | GPIO output, idle high | — |
| ETH\_RST | PA07 | RST1 (socket 1) | GPIO output, active low | — |
| ETH\_INT | PA06 | INT1 (socket 1) | GPIO input / EIC, active low | 6 |
| ENC\_A | PB02 | INT2 (socket 2 header) | EIC, both edges | 2 |
| ENC\_B | PA04 | PWM2 (socket 2 header) | EIC, both edges | 4 |
| ENC\_SW | PB03 | RST2 (socket 2 header) | GPIO input, pull-up | 3 (optional) |
| LCD\_RS | PB08 | AN3 (socket 3 header) | GPIO output | — |
| LCD\_E | PB23 | RST3 (socket 3 header) | GPIO output | — |
| LCD\_D4 | PA28 | CS3 (socket 3 header) | GPIO output | — |
| LCD\_D5 | PA05 | PWM3 (socket 3 header) | GPIO output | — |
| LCD\_D6 | PA23 | INT3 (socket 3 header) | GPIO output | — |
| LCD\_D7 | PA03 | AN2 (socket 2 header) | GPIO output | — |
| DBG\_TX | PA22 | On-board to debugger CDC RX | SERCOM5 PAD0 | — |
| DBG\_RX | PB22 | On-board from debugger CDC TX | SERCOM5 PAD2 | — |

Left free for later: TX1/RX1 (PA08/PA09, SERCOM0), SDA/SCL (PA12/PA13, SERCOM4, the natural home for a 24AA02E48 MAC-address EEPROM), TX2/RX2 (PA20/PA21, SERCOM3, e.g. RS-485), AN1 (PA02, also the DAC), PWM1 (PB09) and CS2 (PA27).

Source: SAMD21G17D Curiosity Nano pinout and Curiosity Nano Base for Click boards Figure 3-1 / adapter schematic, as supplied by Ramon.

## Per-part wiring

The ETH WIZ Click plugs straight into socket 1; the encoder and LCD connect with jumper wires into the empty socket 2 and socket 3 headers.

**ETH WIZ Click:** plug into mikroBUS socket 1. It runs from the socket's 3.3V pin; nothing else to wire.

**KY-040 encoder** (5 pins):

| Encoder pin | Wire to | SAMD21 |
| --- | --- | --- |
| CLK | INT2 (socket 2) | PB02, ENC\_A |
| DT | PWM2 (socket 2) | PA04, ENC\_B |
| SW | RST2 (socket 2) | PB03, ENC\_SW |
| + | +3.3V (socket 2) | — |
| GND | GND (socket 2) | — |

Optional: 10 nF from CLK and from DT to GND. With the module's 10k pull-ups that is about a 100 µs filter against contact bounce.

**1602A LCD** (16 pins, 4-bit mode):

| LCD pin | Name | Wire to |
| --- | --- | --- |
| 1 | VSS | GND |
| 2 | VDD | +5V (socket 3) |
| 3 | V0 | 10k pot wiper; pot ends to +5V and GND |
| 4 | RS | AN3 (socket 3), PB08 |
| 5 | RW | GND, always |
| 6 | E | RST3 (socket 3), PB23 |
| 7–10 | D0–D3 | Not connected |
| 11 | D4 | CS3 (socket 3), PA28 |
| 12 | D5 | PWM3 (socket 3), PA05 |
| 13 | D6 | INT3 (socket 3), PA23 |
| 14 | D7 | AN2 (socket 2), PA03 |
| 15 | A | +5V; add about 100 Ω in series if the module has no backlight resistor |
| 16 | K | GND |

Bring-up check before any firmware: with power only, turning the pot should show a row of solid blocks on line 1. No blocks at any pot position means a power or V0 problem, not a code problem.

## Peripheral assignments for MCC

The whole node needs only these Harmony PLIBs: SERCOM1 (SPI), SERCOM5 (UART), EIC, two TCs, plus PORT, WDT and NVMCTRL.

| Peripheral | Instance | Settings | Notes |
| --- | --- | --- | --- |
| SPI host | SERCOM1 | DOPO: DO PAD0, SCK PAD1, SS PAD2; DIPO: PAD3; mode 0; start at 2 MHz | Hardware SS off; ETH\_CS driven as GPIO so it stays low across a whole W5500 frame |
| USART | SERCOM5 | TXPO PAD0, RXPO PAD2, 115200 8N1 | Shows up as the Curiosity Nano virtual COM port |
| EIC | EXTINT2, EXTINT4 | Both edges, filter on, interrupt enabled, event output enabled on EXTINT2 | Encoder A/B |
| EIC | EXTINT3, EXTINT6 | Optional: button (falling) and W5500 INT (falling) | Start by polling both |
| TC | TC3 | 1 ms periodic interrupt | System tick for the superloop |
| TC + EVSYS | TC4 | Capture mode, fed by the EXTINT2 event | Edge-period timing for RPM (phase P2b) |
| PORT | — | Pin names from the pin map; ETH\_CS and ETH\_RST start high; ENC\_SW pull-up on | LCD pins plain outputs |
| WDT | — | Enable in phase P5 | Off during early debugging |
| NVMCTRL | — | Reserve one flash row for config | Node ID, IP mode, broker IP |

W5500 reset at boot: hold ETH\_RST low for at least 500 µs, release, then wait a couple of milliseconds before the first SPI access. A first SPI read of the VERSIONR register should return 0x04.

## Checks and pitfalls

The one wiring mistake that can damage the board is putting 5V on a SAMD21 pin; everything else here is a debugging-time issue.

- **5V exposure.** SAMD21 I/O is not 5V tolerant. Only LCD VDD, the backlight and the contrast pot touch 5V. LCD RW must be tied to GND before first power-up, and the encoder's + pin goes to 3.3V, because its pull-ups follow that pin.
- **3.3V logic into a 5V LCD.** Most HD44780-compatible controllers read 3.3V as high, but it varies by clone. If text is garbled or random, add a 74HCT245 buffer on the six LCD lines.
- **EXTINT sharing.** PA03 (LCD\_D7) shares EXTINT3 with PB03 (ENC\_SW), and PA20 shares EXTINT4 with PA04 (ENC\_B). That is fine because PA03 is an output and PA20 is unused; just never enable EIC on both pins of a pair.
- **PA08 is the NMI pin** on the SAMD21. It is on TX1 and unused here; avoid it for interrupts later.
- **PA03 is also VREFA.** Do not select the external VREFA reference for the ADC while PA03 drives LCD\_D7.
- **Shared base-board nets.** The SPI bus reaches all three sockets, and TX1/RX1 are shared between sockets 1 and 3. A future Click in socket 3 that needs CS3 or the UART will collide with the LCD wiring.
- **Power budget.** The W5500 draws roughly 130 mA with a 100 Mbit link, plus the LCD backlight. That is well within USB power through the Curiosity Nano.
- **Encoder wires.** Keep them short for the POC. A long unshielded run next to motors is how phantom counts appear, which is one reason the industrial encoder will use differential outputs.

## Network side

A Raspberry Pi 4 Model B works well as the broker and GUI host. It runs Mosquitto easily, has Gigabit Ethernet, and draws only a few watts running 24/7.

- **Isolation.** Use the Pi's Wi-Fi for SSH and updates, and its Ethernet port only for the sensor network. The nodes then never touch the building LAN.
- **Addressing.** Start with the static plan below. Later the Pi can run DHCP for the sensor network (dnsmasq), so new nodes need no per-device setup.
- **Static IP on the Pi.** Raspberry Pi OS (Bookworm) manages networking with NetworkManager, so set the static address with `nmcli` rather than the older `dhcpcd.conf`.
- **GUI options, decided later.** A desktop app on the Pi with a monitor, or a web dashboard the Pi serves so any PC in the building can view it.
- **Reliability.** SD cards can corrupt on power cuts. Use a good card or boot from a USB SSD, and plan a clean-shutdown or UPS story before deployment. The Pi is also the single point of failure for monitoring.

Mosquitto 2.x only listens on localhost by default. For the POC, add this as `/etc/mosquitto/conf.d/poc.conf`, then add passwords and ACLs before deployment:

```
listener 1883 0.0.0.0
allow_anonymous true
```

| Device | IP address | Notes |
| --- | --- | --- |
| Raspberry Pi (broker) | 192.168.50.1/24 | Static on eth0 |
| SAMD21 node 01 | 192.168.50.11 | Static for the POC |
| PIC32CZ demo board | 192.168.50.20 | Static |
| Future nodes | 192.168.50.12–.99 | Static or DHCP from the Pi |

For the POC, a single cable from the WIZ Click to the Pi is enough. Add an unmanaged switch when the PIC32CZ joins the network.
