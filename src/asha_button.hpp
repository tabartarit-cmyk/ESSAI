#pragma once

#include <cstdint>

namespace asha::button
{

// Durées d'appui (en ms) qui distinguent les actions. L'action est déclenchée
// au RELÂCHEMENT du bouton, car un appui de 4 s passe forcément par 2 s.
constexpr uint32_t toggle_press_ms = 2000;  // 2 s <= appui < 4 s : (dé)connexion
constexpr uint32_t repair_press_ms = 4000;  // appui >= 4 s       : nouvel appairage

// À appeler régulièrement (toutes les ms) depuis le thread Bluetooth (core 1).
// Lit le BOOTSEL (toutes les 10 ms), filtre les rebonds et lance les actions.
void service();

} // namespace asha::button
