#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/systm.h>

#include <dev/i2c/i2cvar.h>

#include <sys/ioctl.h>
#include <sys/radioio.h>
#include <dev/radio_if.h>

#include "tea5767reg.h"

/**
 * Radio hw if functions' prototype
 */
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

struct tea5767_tune_{
    int mute;
    uint32_t freq;
    int stereo;
    int is_pll_set;
    int is_xtal_set;
    int fm_band; /* set = JAPAN */
};
typedef struct tea5767_tune_ tea5767_tune;

struct tea5767_softc_{
    device_t sc_dev;
    /* TUNABLE PROPERTIES */
    tea5767_tune tune;
    /* I2C bus controller */
    i2c_tag_t sc_i2c_tag;
    /* Device addr on I2C */
    i2c_addr_t sc_addr;     //uint16_t => i2c_addr_t
};
typedef struct tea5767_softc_ tea5767_softc;

CFATTACH_DECL_NEW(tea5767radio,
                  sizeof(tea5767_softc),
                  tea5767_match,
                  tea5767_attach,
                  NULL,
                  NULL);

static int
tea5767_match(device_t parent, cfdata_t cf, void *aux)
{
    struct i2c_attach_args *ia = aux;

    if (ia->ia_addr == TEA5767_ADDR)
        return I2C_MATCH_ADDRESS_ONLY;
    return 0;
}

static void
tea5767_attach(device_t parent, device_t self, void *aux)
{
    tea5767_softc *sc = device_private(self);
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

    if (tea5767_flags & TEA5767_PLL_FLAG)
        sc->tune.is_pll_set = 1;
    if (tea5767_flags & TEA5767_XTAL_FLAG)
        sc->tune.is_xtal_set = 1;
    if (tea5767_flags & TEA5767_JAPAN_FM_FLAG)
        sc->tune.fm_band = 1;

    radio_attach_mi(&tea5767_hw_if, sc, self); 
}

static int
tea5767_get_info(void *v, struct radio_info *ri)
{
    tea5767_softc *sc = v;
    ri->mute = sc->tune.mute;
    ri->stereo = sc->tune.stereo;
    ri->freq = sc->tune.freq;

    /* *VERIFY* */
    ri->caps = RADIO_CAPS_DETECT_STEREO|
               RADIO_CAPS_SET_MONO|
               RADIO_CAPS_HW_SEARCH;
    /** TODO
     * info function
     * Read the regs
     */

    return 0;
}

static void
tea5767_write(tea5767_softc *sc, uint8_t *reg)
{
    if (iic_acquire_bus(sc->sc_i2c_tag, I2C_F_POLL))
    {
        device_printf(sc->sc_dev, "Bus Acquiration failed \n");
        return;
    }
    int exec_result = iic_exec(sc->sc_i2c_tag,
                               I2C_OP_WRITE_WITH_STOP,
                               sc->sc_addr<<1,
                               NULL,
                               0,
                               reg,
                               5,
                               I2C_F_POLL);
    if (exec_result)
    {
        iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
        device_printf(sc->sc_dev, "write operation failed");
    }
    iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
}
/*
static void
tea5767_read(tea5767_softc *sc, uint8_t *reg)
{
    if (iic_acquire_bus(sc->sc_i2c_tag, I2C_F_POLL))
    {
        device_printf(sc->sc_dev, "Bus Acquiration failed \n");
        return;
    }
    int exec_result = iic_exec(sc->sc_i2c_tag,
                               I2C_OP_READ_WITH_STOP,
                               ((sc->sc_addr)<<1) | 1,
                               NULL,
                               0,
                               reg,
                               5,
                               I2C_F_POLL);
    if (exec_result)
    {
        iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
        device_printf(sc->sc_dev, "read operation failed");
    }
    iic_release_bus(sc->sc_i2c_tag, I2C_F_POLL);
}
*/
static void
tea5767_set_properties(tea5767_softc *sc, uint8_t *reg)
{
    /**
     *  set the PLL word
     * TODO:
     * Extend this PLL word calc to  High side injection as well (Add fields to tea5767_tune)
     */
    reg[0] = 0;
    uint16_t pll_word = 0;

    reg[4] = 0; // XTAL = 0
    reg[5] = 0; //PLLREF = 0

    if (!sc->tune.is_pll_set)
        switch(sc->tune.is_xtal_set)
        {
            case 0: /*Clk freq = 13MHz */
                    pll_word = 4 * (sc->tune.freq * 1000 - 225) * 1000 / 50000;
                    break;
            case 1: /*Clk freq = 32.768 MHz */
                    pll_word = 4 * (sc->tune.freq * 1000 - 225) * 1000 / 32768;
                    reg[4] = TEA5767_XTAL;
                    break;
        }
    else
    {
        pll_word = 4 * (sc->tune.freq * 1000 - 225) * 1000 / 50000;
        reg[5] |= TEA5767_PLLREF;
    }

    reg[1] = pll_word & 0xff;
    reg[0] = (pll_word>>8) & 0x3f;

    if (sc->tune.mute)
        reg[0] |= TEA5767_MUTE;

    reg[3] = TEA5767_SNC;
    if (sc->tune.stereo == 0)
        reg[3] |= TEA5767_MONO;
    if(sc->tune.fm_band)
        reg[3] |= TEA5767_FM_BAND;
}

static int
tea5767_set_info(void *v, struct radio_info *ri)
{
    tea5767_softc *sc = v;
    sc->tune.mute = ri->mute;
    sc->tune.stereo = ri->stereo;
    sc->tune.freq = ri->freq;

    uint8_t reg[5];
    tea5767_set_properties(sc,reg);
    tea5767_write(sc,reg);
    return 1;
}

static int
tea5767_search(void *v, int dir)
{
    tea5767_softc *sc = v;
    uint8_t reg[5];
    tea5767_set_properties(sc, reg);
    /**
     * search activated
     * if dir 1 => search up
     * else : search down
     */
    reg[0] = TEA5767_SEARCH;

    if (dir)
        reg[3] |= TEA5767_SUD;

    tea5767_write(sc, reg);
    return 1;
}

/**
static int
tea5767_set_standby(tea5767_softc *sc)
{
    uint8_t reg[5];

    tea5767_set_properties(sc, reg);
    reg[3] |= TEA5767_STANDBY;

    tea5767_write(sc, reg);
    return 1;
}
*/
