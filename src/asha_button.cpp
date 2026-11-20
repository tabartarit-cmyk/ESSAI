#include <hardware/gpio.h>
#include <hardware/regs/io_qspi.h>
#include <hardware/structs/ioqspi.h>
#include <hardware/structs/sio.h>
#include <hardware/sync.h>
#include <pico/time.h>

#include "asha_button.hpp"
#include "asha_bt.hpp"
#include "hearing_aid.hpp"

namespace asha::button
{

constexpr uint32_t poll_interval_ms = 10;
constexpr uint8_t  debounce_samples = 3;     // 3 x 10 ms identiques = état stable
constexpr uint32_t blink_step_ms    = 120;   // durée d'un pas du double clignotement

// Lecture du bouton BOOTSEL (il partage la broche CS de la flash QSPI).
// La fonction DOIT s'exécuter en RAM : pendant la lecture la flash est
// désélectionnée. Le firmware est lié en "copy_to_ram" et cette fonction est
// appelée uniquement depuis le core 1 (le seul qui touche à la flash via le
// TLV de BTstack), ce qui évite tout accès concurrent à la flash.
static bool __no_inline_not_in_flash_func(bootsel_pressed)()
{
    constexpr uint cs_pin_index = 1;
    uint32_t flags = save_and_disable_interrupts();

    hw_write_masked(&ioqspi_hw->io[cs_pin_index].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Laisse le temps à la ligne de se stabiliser
    for (volatile int i = 0; i < 1000; ++i) { }

#if PICO_RP2040
    constexpr uint32_t cs_bit = (1u << 1);
#else
    constexpr uint32_t cs_bit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    bool pressed = !(sio_hw->gpio_hi_in & cs_bit);

    hw_write_masked(&ioqspi_hw->io[cs_pin_index].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return pressed;
}

/* ---- Double clignotement (action a) ---- */

static bool     blinking   = false;
static uint8_t  blink_step = 0;
static uint32_t blink_next = 0;

static void start_double_blink(uint32_t now)
{
    blinking = true;
    blink_step = 0;
    blink_next = now;
}

static void blink_service(uint32_t now)
{
    if (!blinking || (int32_t)(now - blink_next) < 0) { return; }
    switch (blink_step) {
        case 0: led_mgr.set_led(LEDManager::State::Off); break;
        case 1: led_mgr.set_led(LEDManager::State::On);  break;
        case 2: led_mgr.set_led(LEDManager::State::Off); break;
        case 3: led_mgr.set_led(LEDManager::State::On);  break;
        default:
            blinking = false;
            HearingAid::refresh_led();   // retour à l'état normal de la LED
            return;
    }
    ++blink_step;
    blink_next = now + blink_step_ms;
}

/* ---- Actions ---- */

static void on_short_press(uint32_t now)
{
    // a) Coupe puis relance la liaison audio (les deux oreilles redémarrent
    //    ensemble), avec double clignotement de la LED.
    if (HearingAid::request_audio_restart()) {
        start_double_blink(now);
    }
}

static void on_toggle_press()
{
    blinking = false;
    // b) Selon l'état : déconnecte les appareils (ACP Stop puis déconnexion,
    //    LED éteinte) ou les reconnecte (scan puis ACP Start automatique,
    //    LED rallumée).
    if (HearingAid::get_connections_allowed()) {
        HearingAid::request_disconnect_all();
    } else {
        HearingAid::set_connections_allowed(true);
    }
}

static void on_repair_press()
{
    blinking = false;
    // c) Efface les appairages et repart en mode appairage.
    HearingAid::start_pairing();
}

void service()
{
    static bool     pressed       = false;
    static uint8_t  change_count  = 0;
    static uint32_t press_start   = 0;
    static uint32_t last_poll     = 0;
    static bool     cue_shown     = false;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    blink_service(now);

    if ((now - last_poll) < poll_interval_ms) { return; }
    last_poll = now;

    bool raw = bootsel_pressed();
    if (raw != pressed) {
        if (++change_count >= debounce_samples) {
            change_count = 0;
            pressed = raw;
            if (pressed) {
                press_start = now;
                cue_shown = false;
            } else {
                uint32_t held = now - press_start;
                if (held >= repair_press_ms)      { on_repair_press(); }
                else if (held >= toggle_press_ms) { on_toggle_press(); }
                else                              { on_short_press(now); }
            }
        }
    } else {
        change_count = 0;
    }

    // Repère visuel : à 4 s la LED clignote vite, on peut relâcher pour
    // lancer le nouvel appairage.
    if (pressed && !cue_shown && (now - press_start) >= repair_press_ms) {
        cue_shown = true;
        blinking = false;
        static LEDManager::Pattern repair_cue = {
            .len = 2, .interval_ms = 60, .delay_ms = 0, .pattern = 0b10
        };
        led_mgr.set_led_pattern(repair_cue);
    }
}

} // namespace asha::button
