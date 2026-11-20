#include <bitset>
#include <pico/cyw43_arch.h>

#include "asha_led.hpp"

namespace asha
{

LEDManager::LEDManager()
{
    led_worker.user_data = this;
    led_worker.do_work = &LEDManager::handle_led_work;
}

void LEDManager::set_led(LEDManager::State led_state)
{
    status_led_state = led_state;
    status_pattern = {};
    if (temporary_pattern) return;
    disable_curr_led_pattern();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (led_state == State::On) ? true : false);
}

void LEDManager::set_led_pattern(LEDManager::Pattern const& pattern)
{
    if (pattern.len > 32) return;
    status_led_state = State::Off;
    status_pattern = pattern;
    if (temporary_pattern || !led_ctx) return;
    disable_curr_led_pattern();
    curr_pattern = pattern;
    async_context_add_at_time_worker_in_ms(led_ctx, &led_worker, 0);
}

void LEDManager::set_led_blink(size_t count)
{
    if (count == 0 || count > 16 || !led_ctx) return;
    disable_curr_led_pattern();
    temporary_pattern = true;
    curr_pattern = {
        .len = count * 2,
        .interval_ms = 150,
        .delay_ms = 0,
        .pattern = 0,
        .pos = 0
    };
    for (size_t i = 0; i < count; ++i) {
        curr_pattern.pattern |= 1U << (i * 2);
    }
    async_context_add_at_time_worker_in_ms(led_ctx, &led_worker, 0);
}

void LEDManager::handle_led_work(async_context_t* ctx, async_at_time_worker_t *worker)
{
    LEDManager *this_ = (LEDManager*)worker->user_data;
    Pattern& p = this_->curr_pattern;
    std::bitset<32> pattern_bitset(p.pattern);
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, pattern_bitset[p.pos]);
    ++p.pos;
    if (p.pos >= p.len) {
        p.pos = 0;
        if (this_->temporary_pattern) {
            this_->temporary_pattern = false;
            if (this_->status_pattern.len > 0) {
                this_->curr_pattern = this_->status_pattern;
                Pattern& status = this_->curr_pattern;
                std::bitset<32> status_bits(status.pattern);
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, status_bits[status.pos]);
                ++status.pos;
                async_context_add_at_time_worker_in_ms(ctx, worker, status.interval_ms);
            } else {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, this_->status_led_state == State::On);
            }
            return;
        }
        async_context_add_at_time_worker_in_ms(ctx, worker, p.interval_ms + p.delay_ms);
    } else {
        async_context_add_at_time_worker_in_ms(ctx, worker, p.interval_ms);
    }
}

void LEDManager::disable_curr_led_pattern()
{
    if (led_ctx) {
        async_context_remove_at_time_worker(led_ctx, &led_worker);
    }
}

} // namespace asha