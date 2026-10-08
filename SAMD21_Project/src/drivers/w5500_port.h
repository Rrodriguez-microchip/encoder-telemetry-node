/* w5500_port.h - glue between WIZnet ioLibrary_Driver and this board.
 *
 * The W5500 (ETH WIZ Click, mikroBUS socket 1) runs the whole TCP/IP stack in
 * hardware; the SAMD21 only pokes its registers over SERCOM1 SPI. ioLibrary
 * stays hardware-agnostic by calling back into four function pointers we
 * register here: chip-select assert/deassert and SPI read-byte/write-byte.
 * This file wires those to the generated SERCOM1 SPI PLIB and the ETH_* GPIOs
 * (CLAUDE.md s5 pin map; wiring doc socket 1).
 *
 * P3 scope: bring the chip up and prove the link.
 *   w5500_port_init()  -> hard-reset, register callbacks, wizchip_init()
 *   w5500_version()    -> read VERSIONR; must be 0x04 (SPI + chip sanity check)
 *   w5500_net_up()     -> load a static IP/MAC so the Pi can ping the node
 *
 * Requires the preprocessor symbol  _WIZCHIP_=W5500  (project-level define;
 * the submodule header defaults to W6300 under #ifndef). Build note in
 * CLAUDE.md s4.
 */
#ifndef W5500_PORT_H
#define W5500_PORT_H

#include <stdint.h>
#include <stdbool.h>

/* Reset the W5500, register the SPI/CS callbacks, and run wizchip_init().
 * Call once after SYS_Initialize(). Returns true on success. */
bool    w5500_port_init(void);

/* Read the VERSIONR register. On a healthy W5500 this is always 0x04; any
 * other value means the SPI link or the chip is wrong (P3 milestone 1). */
uint8_t w5500_version(void);

/* Apply the static network config (MAC/IP/subnet/gateway from services/config)
 * so the node answers ARP/ping. Returns true on success (P3 milestone 2). */
bool    w5500_net_up(void);

#endif /* W5500_PORT_H */
