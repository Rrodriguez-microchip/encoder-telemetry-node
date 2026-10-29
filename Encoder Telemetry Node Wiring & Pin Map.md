# Encoder Telemetry Node: Wiring & Pin Map

Oct 5, 2026 · @Alejandro

## Overview

The ETH WIZ Click sits in socket 1; the encoder uses the empty socket 2 header, and the LCD hangs off the I2C bus (SDA/SCL) through an Adafruit #292 backpack, so no soldering is needed.

> **Changed 2026-10-29:** the LCD moved from six parallel GPIOs to the I2C backpack (one 4-wire cable). PB08, PB23, PA28, PA05, PA23 and PA03 are free again.

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
| LCD\_SDA | PA12 | SDA | SERCOM4 PAD0 (I2C) | — |
| LCD\_SCL | PA13 | SCL | SERCOM4 PAD1 (I2C) | — |
| DBG\_TX | PA22 | On-board to debugger CDC RX | SERCOM5 PAD0 | — |
| DBG\_RX | PB22 | On-board from debugger CDC TX | SERCOM5 PAD2 | — |

The I2C bus (PA12/PA13) is shared: a 24AA02E48 MAC-address EEPROM or any Qwiic device can join it at an address other than 0x20.

Left free for later: TX1/RX1 (PA08/PA09, SERCOM0), the old LCD pins (PB08, PB23, PA28, PA05, PA23, PA03), TX2/RX2 (PA20/PA21, SERCOM3, e.g. RS-485), AN1 (PA02, also the DAC), PWM1 (PB09) and CS2 (PA27).

Source: SAMD21G17D Curiosity Nano pinout and Curiosity Nano Base for Click boards Figure 3-1 / adapter schematic, as supplied by Ramon.

## Per-part wiring

The ETH WIZ Click plugs straight into socket 1; the encoder connects with jumper wires into the empty socket 2 header, and the LCD backpack connects to SDA/SCL.

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

**1602A LCD + Adafruit #292 I2C backpack:** the backpack solders onto the LCD's 16-pin header and drives RS, E, D4–D7, contrast (its own pot) and the backlight. Address jumpers open (0x20), SPI-enable jumper open, 3-5V jumper intact.

Bench, using the backpack's 5-pin terminal block:

| Terminal block | Signal | Wire to |
| --- | --- | --- |
| GND | Ground | GND (common with encoder and W5500) |
| 5V | VIN, 3–5 V in | **VTG / +3.3V, not 5 V** |
| DAT | SDA | SDA, PA12 |
| CLK | SCL | SCL, PA13 |
| LAT | SPI latch | Not connected (SPI mode only) |

Final, using the STEMMA QT / Qwiic 4-pin cable (same four signals; use the cable or the terminal block, not both):

| Wire | Signal | Wire to |
| --- | --- | --- |
| Black | GND | GND |
| Red | V+ | VTG / +3.3V |
| Blue | SDA | PA12 |
| Yellow | SCL | PA13 |

Why 3.3 V on VIN: the backpack's 10k SDA/SCL pull-ups go to VIN, and the SAMD21 is not 5 V tolerant. The onboard boost still gives the LCD the 5 V VDD it needs.

Bring-up check: if the backpack is missing or SDA/SCL are swapped, the UART prints `lcd: I2C backpack not found at 0x20 -- display disabled` at boot and the node runs on without a display. Backlight on but blank screen = turn the backpack's contrast pot.

## Peripheral assignments for MCC

The whole node needs only these Harmony PLIBs: SERCOM1 (SPI), SERCOM4 (I2C), SERCOM5 (UART), EIC, two TCs, plus PORT, WDT and NVMCTRL.

| Peripheral | Instance | Settings | Notes |
| --- | --- | --- | --- |
| SPI host | SERCOM1 | DOPO: DO PAD0, SCK PAD1, SS PAD2; DIPO: PAD3; mode 0; start at 2 MHz | Hardware SS off; ETH\_CS driven as GPIO so it stays low across a whole W5500 frame |
| I2C host | SERCOM4 | I2C master, 400 kHz, SDA hold default; interrupt on, priority 3 | SDA/SCL are fixed to PAD0/PAD1 in I2C mode (no pinout options); pick PA12/PA13 in Pin Manager. Internal pull-ups off (backpack has 10k) |
| USART | SERCOM5 | TXPO PAD0, RXPO PAD2, 115200 8N1 | Shows up as the Curiosity Nano virtual COM port |
| EIC | EXTINT2, EXTINT4 | Both edges, filter on, interrupt enabled, event output enabled on EXTINT2 | Encoder A/B |
| EIC | EXTINT3, EXTINT6 | Optional: button (falling) and W5500 INT (falling) | Start by polling both |
| TC | TC3 | 1 ms periodic interrupt | System tick for the superloop |
| TC + EVSYS | TC4 | Capture mode, fed by the EXTINT2 event | Edge-period timing for RPM (phase P2b) |
| PORT | — | Pin names from the pin map; ETH\_CS and ETH\_RST start high; ENC\_SW pull-up on | — |
| WDT | — | Enable in phase P5 | Off during early debugging |
| NVMCTRL | — | Reserve one flash row for config | Node ID, IP mode, broker IP |

W5500 reset at boot: hold ETH\_RST low for at least 500 µs, release, then wait a couple of milliseconds before the first SPI access. A first SPI read of the VERSIONR register should return 0x04.

## Checks and pitfalls

The one wiring mistake that can damage the board is putting 5V on a SAMD21 pin; everything else here is a debugging-time issue.

- **5V exposure.** SAMD21 I/O is not 5V tolerant. The LCD backpack's VIN and the encoder's + pin both go to 3.3V, because their pull-ups follow those pins. The backpack boosts its own 5 V for the LCD.
- **EXTINT sharing.** PA03 shares EXTINT3 with PB03 (ENC\_SW), and PA20 shares EXTINT4 with PA04 (ENC\_B). Both partners are unused; just never enable EIC on both pins of a pair.
- **PA08 is the NMI pin** on the SAMD21. It is on TX1 and unused here; avoid it for interrupts later.
- **Shared base-board nets.** The SPI bus reaches all three sockets, TX1/RX1 are shared between sockets 1 and 3, and SDA/SCL reach every socket. An I2C Click added later shares the bus with the LCD; it just needs an address other than 0x20.
- **Power budget.** The W5500 draws roughly 130 mA with a 100 Mbit link, plus the LCD backlight, which the backpack's boost draws from 3.3 V (more current than at 5 V). That is well within USB power through the Curiosity Nano.
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
