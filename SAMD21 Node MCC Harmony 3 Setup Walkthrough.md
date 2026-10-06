# SAMD21 Node: MCC / Harmony 3 Setup Walkthrough

Oct 5, 2026 · @Alejandro

This takes an empty MPLAB X project to a building, printing SAMD21 project with every peripheral the encoder node needs, using the pin map from the wiring doc.

## Before you start

You need three installs and one empty git repo; Harmony itself is downloaded from inside MCC.

1. MPLAB X IDE, current release. MCC ships as a built-in plugin; update it from Tools → Plugins if MPLAB X offers an update.
2. MPLAB XC32 compiler, current release (the free license is fine).
3. Git, plus a terminal you'll later run Claude Code from.
4. Create the repo with this layout before opening MPLAB X:

```
encoder-node/
  firmware/            <- MPLAB X project lives here
    src/
      config/default/  <- MCC-generated, never hand-edited
      app/             <- app.c, state machine
      drivers/         <- encoder_ky040.c, lcd_hd44780.c, w5500_port.c
      services/        <- metrics, net_mqtt, display, config
    third_party/
      ioLibrary_Driver/
  tests/               <- host-side unit tests (gcc + Unity)
  docs/                <- wiring doc export, MQTT contract
```

Record the MPLAB X, XC32 and Harmony package versions in a README as you install them. A project that regenerates differently on a colleague's machine is almost always a version mismatch.

## Create the project and load Harmony

The project targets the ATSAMD21G17D with the Curiosity Nano's on-board debugger, and MCC pulls three Harmony packages: csp, core and dev\_packs.

1. File → New Project → Microchip Embedded → Application Project(s).
2. Device: **ATSAMD21G17D**. Tool: select the connected SAMD21 Curiosity Nano (it shows by serial number; plug it in first).
3. Compiler: XC32. Name the project `encoder_node` and set its location to `encoder-node/firmware`.
4. Open MCC (the MCC button in the toolbar). When asked for content type, choose **MPLAB Harmony**.
5. In the Content Manager, select the latest `csp`, `core` and `dev_packs` and click Finish. You do not need `net`: the W5500 uses WIZnet's library, not the Harmony TCP/IP stack.
6. MCC opens with the Project Graph, Device Resources and Pin Configuration panes. Leave the default `config/default` configuration name.

Menu names shift slightly between MCC releases. If a step's wording doesn't match, the Harmony "Create your first application" guide for SAM D21 shows the same flow.

## Clocks

Run the CPU and SERCOMs from a 48 MHz GCLK0, and feed the EIC from a separate slow clock so its filter actually rejects contact bounce.

1. Open Plugins → Clock Configuration.
2. **GCLK0 = 48 MHz from the DFLL48M.** Check the SAMD21 Curiosity Nano user guide for a 32.768 kHz crystal. If it is fitted, enable XOSC32K and run the DFLL closed-loop from it, which keeps the 115200 baud UART accurate. If not, open-loop is acceptable for the POC; verify the UART output is clean.
3. **Flash wait states.** In NVMCTRL, set read wait states to 1 for 48 MHz at 3.3V. Zero wait states at 48 MHz gives random hard faults that look like firmware bugs.
4. **A slow generator for the EIC.** Configure a spare generator (e.g. GCLK2) from OSCULP32K, about 32 kHz, and route the EIC peripheral clock to it. The EIC filter takes a majority vote over 3 samples of its clock. At 48 MHz that only rejects nanosecond glitches; at 32 kHz it rejects bounce shorter than about 60 µs.
5. Peripheral clock channels: enable GCLK0 for SERCOM1, SERCOM5, TC3 and TC4. On the SAMD21, TC4 and TC5 share a clock channel, and TC3 shares one with TCC2.

## Pin Manager

Set all 17 pins in Pin Configuration → Pin Settings (table view) so the generated macros carry your names, e.g. `ETH_CS_Clear()` and `LCD_E_Set()`.

| Pin | Function | Custom name | Direction | Initial latch | Pull |
| --- | --- | --- | --- | --- | --- |
| PA16 | SERCOM1\_PAD0 | ETH\_MOSI | — | — | — |
| PA17 | SERCOM1\_PAD1 | ETH\_SCK | — | — | — |
| PA19 | SERCOM1\_PAD3 | ETH\_MISO | — | — | — |
| PA18 | GPIO | ETH\_CS | Out | High | — |
| PA07 | GPIO | ETH\_RST | Out | High | — |
| PA06 | GPIO | ETH\_INT | In | — | Up |
| PB02 | EIC\_EXTINT2 | ENC\_A | — | — | — |
| PA04 | EIC\_EXTINT4 | ENC\_B | — | — | — |
| PB03 | GPIO | ENC\_SW | In | — | Up |
| PB08 | GPIO | LCD\_RS | Out | Low | — |
| PB23 | GPIO | LCD\_E | Out | Low | — |
| PA28 | GPIO | LCD\_D4 | Out | Low | — |
| PA05 | GPIO | LCD\_D5 | Out | Low | — |
| PA23 | GPIO | LCD\_D6 | Out | Low | — |
| PA03 | GPIO | LCD\_D7 | Out | Low | — |
| PA22 | SERCOM5\_PAD0 | DBG\_TX | — | — | — |
| PB22 | SERCOM5\_PAD2 | DBG\_RX | — | — | — |

Also add the on-board LED0 as a GPIO output named `LED0`, using the pin from the Curiosity Nano user guide.

- **PA18 stays GPIO,** not SERCOM1\_PAD2. The W5500 needs CS held low for a whole multi-byte frame, and software control guarantees that.
- **ETH\_INT starts as polled GPIO.** Switch it to EIC\_EXTINT6 later if you want interrupt-driven networking.
- **ENC\_A/B pull-ups** come from the KY-040 module. Turn on the internal pull-ups too if your module turns out to have none fitted.

## Components in the Project Graph

Add each component from Device Resources (Peripherals unless noted), then set it in the Configuration Options pane. Start with blocking (non-interrupt) SPI; it makes the ioLibrary callbacks trivial.

| Component | Setting | Value |
| --- | --- | --- |
| SERCOM1 | Operating mode | SPI Master |
| SERCOM1 | Data out pinout (DOPO) | DO on PAD0, SCK on PAD1, SS on PAD2 |
| SERCOM1 | Data in pinout (DIPO) | PAD3 |
| SERCOM1 | Clock polarity / phase | Idle low, sample on leading edge (SPI mode 0) |
| SERCOM1 | Baud rate | 2 MHz to start; up to 12 MHz once stable |
| SERCOM1 | Hardware slave select | Disabled |
| SERCOM1 | Interrupts | Disabled (blocking mode) |
| SERCOM5 | Operating mode | USART with internal clock |
| SERCOM5 | TX / RX pinout | TX on PAD0, RX on PAD2 |
| SERCOM5 | Baud / format | 115200, 8N1 |
| SERCOM5 | Operating mode (PLIB) | Ring buffer, for the later UART config CLI |
| STDIO (System Services) | Connected to | SERCOM5, so `printf` goes to the virtual COM port |
| EIC | EXTINT2, EXTINT4 | Enabled, sense both edges, filter on, interrupt on |
| EIC | EXTINT2 event output | On (feeds TC4 capture later) |
| EIC | EXTINT3, EXTINT6 | Off for now |
| TC3 | Mode | 16-bit timer, period 1 ms, overflow interrupt on |
| WDT | Enable | Add but leave disabled until phase P5 |
| NVMCTRL | Wait states | 1 (see Clocks) |

Defer for phase P2b, edge-period RPM:

- **TC4 in 32-bit capture mode** (TC4 paired with TC5), with the capture input from an EVSYS channel whose generator is EIC EXTINT2.
- **Why 32-bit:** at 20 pulses per revolution, a slow turn of 1 RPM gives an edge every 3 seconds. A 16-bit counter at any useful resolution wraps long before that.

## Generate, build and bring-up test

Phase P0 is done when LED0 blinks once per second and a counter prints over the virtual COM port.

1. Click **Generate** in MCC. On later regenerations, review each file in the merge dialog; you should only ever see changes inside `config/default`.
2. Build (hammer icon). Fix nothing in generated files; a build error there means a configuration setting is wrong.
3. In `main.c`, after `SYS_Initialize(NULL);`, add a temporary test loop:

```c
uint32_t n = 0;
while (true)
{
    LED0_Toggle();
    printf("tick %lu\r\n", (unsigned long)n++);
    for (volatile uint32_t d = 0; d < 2000000; d++) { }  // crude delay, replaced by TC3 tick later
    SYS_Tasks();
}
```

4. Program the board (Make and Program). Open the Curiosity Nano's COM port at 115200 8N1 in any terminal.
5. Commit: everything under `firmware/` except `build/`, `dist/` and `nbproject/private/`, including the MCC configuration file. That commit is your known-good baseline.

Garbled characters at the right baud rate point to clock accuracy (see Clocks, step 2). Nothing at all usually means the wrong SERCOM pad or that STDIO isn't connected to SERCOM5.

## Adding WIZnet ioLibrary

ioLibrary talks to the W5500 through four callbacks you register (chip select, deselect, read byte, write byte); a small `w5500_port.c` maps them onto the Harmony SPI PLIB and pin macros.

1. From the repo root: `git submodule add https://github.com/Wiznet/ioLibrary_Driver firmware/third_party/ioLibrary_Driver`. A submodule pins the exact library version.
2. In MPLAB X, add to the project: `Ethernet/wizchip_conf.c`, `Ethernet/socket.c`, `Ethernet/W5500/w5500.c`. Add `Internet/DHCP` and `Internet/MQTT` (including `MQTTPacket/src`) when you reach phase P4.
3. Project Properties → xc32-gcc → Preprocessing: add include paths for `Ethernet`, `Ethernet/W5500` and, later, the `Internet` folders.
4. In `wizchip_conf.h`, confirm `_WIZCHIP_` is `W5500` and the I/O mode is SPI variable-data-length (`_WIZCHIP_IO_MODE_SPI_VDM_`).
5. Create `src/drivers/w5500_port.c`:

```c
#include "definitions.h"   // Harmony: SERCOM1 SPI, pin macros
#include "wizchip_conf.h"
#include "w5500.h"

static void cs_sel(void)   { ETH_CS_Clear(); }
static void cs_desel(void) { ETH_CS_Set(); }

static uint8_t spi_rb(void)
{
    uint8_t tx = 0xFF, rx = 0;
    SERCOM1_SPI_WriteRead(&tx, 1, &rx, 1);
    return rx;
}

static void spi_wb(uint8_t b)
{
    SERCOM1_SPI_Write(&b, 1);
}

bool w5500_port_init(void)
{
    ETH_RST_Clear();
    delay_us(600);            // >= 500 us reset pulse; use your TC3-based delay
    ETH_RST_Set();
    delay_ms(2);

    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi_rb, spi_wb);

    return (getVERSIONR() == 0x04);   // W5500 identifies itself as 0x04
}
```

6. Call `w5500_port_init()` after `SYS_Initialize()` and print the result. True means SPI, CS, reset and wiring are all correct; this is the phase P3 gate.

`delay_us()` and `delay_ms()` are placeholders for helpers you'll build on the TC3 tick. When you add MQTT, ioLibrary's MQTT layer also needs `MilliTimer_Handler()` called from the 1 ms TC3 interrupt.

Once single-byte transfers work, register the burst callbacks with `reg_wizchip_spiburst_cbfunc()` for faster socket reads and writes.

## Pitfalls and hand-off to Claude Code

Most Harmony pain comes from editing generated code or from silent clock mistakes; avoid both and the rest is ordinary firmware work.

- **Never edit `config/default`.** Your code goes in `app/`, `drivers/` and `services/`. If a generated file must change, change the MCC setting and regenerate.
- **`main.c` is shared.** MCC generates it once and merges after that. Keep it to `SYS_Initialize()` plus a call into `app.c`.
- **Regenerate before you commit.** Commit the MCC config and the generated code together, so the repo always builds as checked in.
- **EIC needs its clock running.** If encoder interrupts never fire, first check that the EIC's GCLK generator is enabled.
- **Interrupt-mode SPI later.** Switching SERCOM1 to interrupt mode changes `SERCOM1_SPI_WriteRead()` to return immediately; you must then wait on `SERCOM1_SPI_IsBusy()` in the port layer.

When P0 runs, hand the repo to Claude Code with a `CLAUDE.md` at its root covering:

- The pin names and peripherals from this walkthrough.
- The folder rules above, starting with no edits under `config/default`.
- The encoder API and the MQTT topic contract.
- The phase list, so it works on one phase at a time.

That `CLAUDE.md` is the next document to write.
