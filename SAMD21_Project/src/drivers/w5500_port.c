/* w5500_port.c - see w5500_port.h.
 *
 * Bridges ioLibrary_Driver to SERCOM1 SPI + the ETH_* GPIOs. ioLibrary calls
 * our callbacks for every register access; we keep CS low across a whole frame
 * (wiring doc: hardware SS is off, CS is a plain GPIO) and move one byte at a
 * time through the blocking SERCOM1 PLIB -- fine for 2 MHz bring-up.
 */
#include "drivers/w5500_port.h"
#include "services/delay.h"

/* Generated headers (MCC). Pin macros ETH_CS/ETH_RST live in plib_port.h;
 * the SPI byte mover is in the SERCOM1 PLIB. */
#include "config/default/definitions.h"

/* ioLibrary (submodule). Needs _WIZCHIP_=W5500 defined project-wide. */
#include "drivers/ioLibrary_Driver/Ethernet/wizchip_conf.h"

/* ---- static network config (P3) --------------------------------------
 * Hardcoded for bring-up so the Pi can ping us. P5 moves these into
 * services/config (NVM + UART CLI), same as node_id/feet_per_rev.
 *
 * MAC is locally-administered (bit 1 of the first octet set, bit 0 clear):
 * the W5500 has no MAC of its own and there is no MAC EEPROM yet (the wiring
 * doc reserves SERCOM4/24AA02E48 for that later). Make this unique per node
 * before deploying more than one on the same LAN. */
static const uint8_t  NODE_MAC[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
static const uint8_t  NODE_IP[4]  = { 192, 168, 1, 50 };
static const uint8_t  NODE_SUBNET[4] = { 255, 255, 255, 0 };
static const uint8_t  NODE_GATEWAY[4] = { 192, 168, 1, 1 };

/* ---- ioLibrary callbacks --------------------------------------------- */

static void cs_select(void)   { ETH_CS_Clear(); }   /* CS active low */
static void cs_deselect(void) { ETH_CS_Set();   }   /* idle high */

static uint8_t spi_read_byte(void)
{
    uint8_t tx = 0xFFU;   /* clock out a dummy byte to read one in */
    uint8_t rx = 0x00U;
    (void)SERCOM1_SPI_WriteRead(&tx, 1U, &rx, 1U);
    return rx;
}

static void spi_write_byte(uint8_t b)
{
    (void)SERCOM1_SPI_Write(&b, 1U);
}

/* ---- bring-up --------------------------------------------------------- */

bool w5500_port_init(void)
{
    /* Idle CS high before anything talks on the bus. */
    ETH_CS_Set();

    /* Hard reset: the W5500 needs RST low for >= 500 us, then >= 1 ms to come
     * back up (datasheet t_RC). Generous margins here; this runs once. */
    ETH_RST_Clear();
    delay_us(600U);
    ETH_RST_Set();
    delay_ms(2U);

    /* Hand ioLibrary our CS + per-byte SPI callbacks. We use the single-byte
     * (not burst) path: simplest, and plenty for register access at 2 MHz. */
    reg_wizchip_cs_cbfunc(cs_select, cs_deselect);
    reg_wizchip_spi_cbfunc(spi_read_byte, spi_write_byte);

    /* wizchip_init() sets the socket RX/TX buffer sizes. {2,2,2,2,...} KB per
     * socket is the default 2 KB layout; NULL would also work. Returns 0 on
     * success. We keep it simple -- one socket is enough for MQTT later. */
    uint8_t bufsize[2][8] = { {2,2,2,2,2,2,2,2}, {2,2,2,2,2,2,2,2} };
    if (wizchip_init(bufsize[0], bufsize[1]) != 0)
    {
        return false;
    }
    return true;
}

uint8_t w5500_version(void)
{
    return getVERSIONR();   /* always 0x04 on a healthy W5500 */
}

bool w5500_net_up(void)
{
    wiz_NetInfo info = { 0 };
    for (int i = 0; i < 6; i++) { info.mac[i] = NODE_MAC[i]; }
    for (int i = 0; i < 4; i++)
    {
        info.ip[i]  = NODE_IP[i];
        info.sn[i]  = NODE_SUBNET[i];
        info.gw[i]  = NODE_GATEWAY[i];
    }
    info.dhcp = NETINFO_STATIC;

    wizchip_setnetinfo(&info);

    /* Shorten the TCP retransmission window so a failed connect() gives up
     * fast. Defaults (RTR=2000=200ms, RCR=8) take ~1.8 s to time out -- right
     * at the 1.9 s WDT period, so a dead broker would reset the node mid-
     * handshake. 200 ms x 3 retries ~= 0.8 s is safely under the WDT, so an
     * unreachable broker returns SOCKERR_TIMEOUT cleanly and the superloop
     * (which pets the dog every pass) keeps running and simply retries later.
     * Unit of RTR is 100 us; RCR is a retry count. */
    setRTR(2000U);
    setRCR(3U);

    /* Read it back and confirm the IP stuck -- cheap proof the config write
     * reached the chip, before we go chasing a missing ping at the Pi. */
    wiz_NetInfo check = { 0 };
    wizchip_getnetinfo(&check);
    for (int i = 0; i < 4; i++)
    {
        if (check.ip[i] != NODE_IP[i])
        {
            return false;
        }
    }
    return true;
}
