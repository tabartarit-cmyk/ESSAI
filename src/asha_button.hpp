#pragma once

#include <cstdint>

namespace asha::button
{

// Comportement du bouton BOOTSEL. Les actions sont déclenchées au RELÂCHEMENT
// (sauf le repère visuel de l'appui long) :
//   - 1 appui court                : ACP Stop puis relance immédiate du flux audio
//   - 2 appuis courts successifs   : bascule déconnexion / reconnexion des aids
//   - appui >= repair_press_ms     : efface les appairages, nouvel appairage
constexpr uint32_t repair_press_ms      = 2000; // appui long
constexpr uint32_t double_click_gap_ms  = 400;  // délai max avant le 2e appui

// À appeler régulièrement (toutes les ms) depuis le thread Bluetooth (core 1).
// Lit le BOOTSEL (toutes les 10 ms), filtre les rebonds et lance les actions.
void service();

} // namespace asha::button
