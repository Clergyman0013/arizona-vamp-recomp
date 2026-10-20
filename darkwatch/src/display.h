/**
 * Darkwatch Recompiled - display options bridge
 *
 * The options live in the game's config (darkwatch.ini); the D3D8 runtime
 * exposes them through xbox_D3D8SetDisplayOptions(). This is the one place
 * that translates between the two, so the Windows and POSIX bootstraps stay
 * identical.
 */
#ifndef DARKWATCH_DISPLAY_H
#define DARKWATCH_DISPLAY_H

#include "config.h"

void darkwatch_display_configure(const DarkwatchConfig *cfg);

#endif /* DARKWATCH_DISPLAY_H */
