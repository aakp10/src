#include <sys/cdefs.h>

#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <dev/i2c/i2cvar.h>

#include <sys/ioctl.h>
#include <sys/radioio.h>
#include <dev/radio_if.h>

#include "tea5767reg.h"


static int tea5767_get_info(void *, struct radio_info *);
static int tea5767_set_info(void *, struct radio_info *);
static int tea5767_search(void *, int);

static int tea5767_match(device_t, cfdata_t, void *);
static void tea5767_attach(device_t, device_t, void *);

static const struct radio_hw_if tea5767_hw_if = {
    NULL,
    NULL,
    tea5767_get_info,
    tea5767_set_info,
    tea5767_search
};

struct tea5767_tune{
    int mute;
    uint32_t freq;
    int stereo;
    int is_pll_set;
    int is_xtal_set;
    int fm_band; /* set = JAPAN */
    uint8_t read_pll[2];
    bool is_search_complete;
};
struct tea5767_softc{
    device_t sc_dev;
    /* TUNABLE PROPERTIES */
    struct tea5767_tune tune;
    /* I2C bus controller */
    i2c_tag_t sc_i2c_tag;
    /* Device addr on I2C */
    i2c_addr_t sc_addr;
};

CFATTACH_DECL_NEW(tea5767radio, sizeof(struct tea5767_softc), tea5767_match,
    tea5767_attach, NULL, NULL);

static int
tea5767_match(device_t parent, cfdata_t cf, void *aux)
{
    struct i2c_attach_args *ia = aux;

    if (ia->ia_addr == TEA5767_ADDR)
        return 1;
    return 0;
}

static void
tea5767_attach(device_t parent, device_t self, void *aux)
{
    struct tea5767_softc *sc = device_private(self);
    int tea5767_flags = device_cfdata(self)->cf_flags;
    struct i2c_attach_args *ia = aux;

    aprint_normal(": TEA5767 Radio\n");

    sc->sc_dev = self;
    sc->tune.mute = 0;
    sc->tune.freq = MIN_FM_FREQ;
    sc->tune.stereo = 1;
    sc->tune.is_pll_set = 0;
    sc->tune.is_xtal_set = 0;
    sc->sc_i2c_tag = ia->ia_tag;
    sc->sc_addr = ia->ia_addr;
    memset(sc->tune.read_pll,0,2);

    if (tea5767_flags & TEA5767_PLL_FLAG)
        sc->tune.is_pll_set = 1;
    if (tea5767_flags & TEA5767_XTAL_FLAG)
        sc->tune.is_xtal_set = 1;
    if (tea5767_flags & TEA5767_JAPAN_FM_FLAG)
        sc->tune.fm_band = 1;

    radio_attach_mi(&tea5767_hw_if, sc, self);
}

static int
tea5767_write(struct tea5767_softc *sc, uint8_t *reg)
{
    if (iic_acquire_bus(sc->sc_i2c_tag, I2C_F_POLL)) {
        device_printf(sc->sc_dev, "Bus Acquiration failed \n");
        return 1;
    }
    int exec_result = iic_exec(sc->sc_i2c_tag, I2C_OP_WRITE_WITH_STOP,
                        sc->sc_addr, NULL, 0, reg, 5, I2C_F_POLL);
    if (exec_result) {
        iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
        device_printf(sc->sc_dev, "write operation failed %d \n",exec_result);
        return 1;
    }
    iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
    return 0;
}


static void
tea5767_set_properties(struct tea5767_softc *sc, uint8_t *reg)
{
    /*
     * set the PLL word
     * TODO:
     * Extend this PLL word calc to  High side injection as well (Add fields to tea5767_tune)
     */
    memset(reg,0,5);
    uint16_t pll_word = 0;

    if(!sc->tune.is_search_complete) {
        if (!sc->tune.is_pll_set && !sc->tune.is_xtal_set)
            pll_word = 4 * (sc->tune.freq  - 225) * 1000 / 50000;
        else if (!sc->tune.is_pll_set && sc->tune.is_xtal_set) {
            pll_word = 4 * (sc->tune.freq - 225) * 1000 / 32768;
            reg[3] = TEA5767_XTAL;
        }
        else {
            pll_word = 4 * (sc->tune.freq - 225) * 1000 / 50000;
            reg[4] |= TEA5767_PLLREF;
        }
        reg[1] = pll_word & 0xff;
        reg[0] = (pll_word>>8) & 0x3f;
    }
    else {
        reg[1] = sc->tune.read_pll[1];
        reg[0] |= sc->tune.read_pll[0];
    }

    if (sc->tune.mute) {
        reg[0] |= TEA5767_MUTE;
        reg[2] |=TEA5767_MUTE_R | TEA5767_MUTE_L;
    }

    reg[3] |= TEA5767_SNC;
    if (sc->tune.stereo == 0)
        reg[2] |= TEA5767_MONO;
    if(sc->tune.fm_band)
        reg[3] |= TEA5767_FM_BAND;
}

static int
tea5767_get_info(void *v, struct radio_info *ri)
{
    struct tea5767_softc *sc = v;
    ri->mute = sc->tune.mute;
    ri->stereo = sc->tune.stereo;
    ri->freq = sc->tune.freq;

    ri->caps = RADIO_CAPS_DETECT_STEREO|
               RADIO_CAPS_SET_MONO|
               RADIO_CAPS_HW_SEARCH;
    return 0;
}

static int
tea5767_set_info(void *v, struct radio_info *ri)
{
    struct tea5767_softc *sc = v;
    sc->tune.mute = ri->mute;
    sc->tune.stereo = ri->stereo;
    if(sc->tune.freq != ri->freq)
        sc->tune.is_search_complete = false;
    sc->tune.freq = ri->freq;
    uint8_t reg[5];
    tea5767_set_properties(sc, reg);
    return tea5767_write(sc, reg);
}

static int
tea5767_read(struct tea5767_softc *sc, uint8_t *reg)
{
    if (iic_acquire_bus(sc->sc_i2c_tag, I2C_F_POLL)) {
        device_printf(sc->sc_dev, "Bus Acquiration failed \n");
        return 1;
    }

    int exec_result = iic_exec(sc->sc_i2c_tag, I2C_OP_READ_WITH_STOP, sc->sc_addr,
                        NULL, 0, reg, 5, I2C_F_POLL);

    if (exec_result) {
        iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
        device_printf(sc->sc_dev, "read operation failed");
        return 1;
    }

    iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
    return 0;
}

static int
tea5767_search(void *v, int dir)
{
    struct tea5767_softc *sc = v;
    uint8_t reg[5];
    tea5767_set_properties(sc, reg);
    /*
     * search activated
     * if dir 1 => search up
     * else : search down
     */
    reg[0] |= TEA5767_MUTE | TEA5767_SEARCH;

    if (dir)
        reg[2] |= TEA5767_SUD;
    reg[2] |= TEA5767_SSL_1; /* Stop level for search*/
    tea5767_write(sc, reg);

    uint8_t read_reg[5];
    memset(read_reg,0,5);
    tea5767_read(sc, read_reg);

    while (!(read_reg[0] & TEA5767_READY_FLAG)) {
        kpause("teasrch", true, 1 * hz, NULL);
        if (read_reg[0]>>6 & 1) {
            sc->tune.freq = MIN_FM_FREQ;
            tea5767_set_properties(sc, reg);
            tea5767_write(sc, reg);
            return 0;
        }

        memset(read_reg,0,5);
        tea5767_read(sc,read_reg);
    }
    device_printf(sc->sc_dev, "bandlimit reached : %d\n",(read_reg[0] & 0x40));
    sc->tune.read_pll[0] = 0;
    sc->tune.read_pll[1] = read_reg[1];
    sc->tune.read_pll[0] |= read_reg[0] & 0x3f;

    /* Reset the previous settings in the regs */

    sc->tune.is_search_complete = true;
    tea5767_set_properties(sc, reg);
    tea5767_write(sc, reg);
    return 0;
}