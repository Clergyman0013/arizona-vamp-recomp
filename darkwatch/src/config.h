/**
 * Darkwatch Recompiled - options / configuration
 *
 * Reads a simple INI-ish key=value file (darkwatch.ini) and lets command
 * line / environment variables override individual keys. The values are
 * consumed by the host bootstrap (main_*.c) and by the display layer.
 */
#ifndef DARKWATCH_CONFIG_H
#define DARKWATCH_CONFIG_H

typedef struct DarkwatchConfig {
    /* Video ------------------------------------------------------------ */
    int   window_width;        /* 0 = follow the game's backbuffer size   */
    int   window_height;
    int   resolution_scale;    /* 1..4: window = backbuffer * scale.
                                * 0 = stretch window_width/height          */
    int   widescreen;          /* 1 = widen the projection to 16:9 (or
                                * the window aspect), 0 = original 4:3    */
    float fov_scale;           /* extra FOV multiplier, 1.0 = untouched   */
    int   fullscreen;          /* 0 windowed, 1 borderless fullscreen     */
    int   vsync;               /* 0 off, 1 on                             */
    int   fps_limit;           /* 0 = uncapped                            */

    /* Audio ------------------------------------------------------------ */
    int   audio_enabled;
    int   audio_volume;        /* 0..100 */

    /* Input ------------------------------------------------------------ */
    int   keyboard;            /* 1 = keyboard mapping for pad           */
    int   deadzone;            /* percent, 0..50                          */

    /* Debug ------------------------------------------------------------ */
    int   verbose;             /* mirrors RECOMP_* diagnostics            */
    int   trap_null;           /* RECOMP_TRAP_NULL                        */
    int   watchdog_seconds;    /* 0 = off                                 */

    /* Game data -------------------------------------------------------- */
    char  game_dir[512];       /* where default.xbe + *.pck live          */
} DarkwatchConfig;

/* Fills `cfg` with built-in defaults, then overrides from the config file,
 * then from the environment. Never fails: a missing or malformed file is
 * reported and the defaults stand. */
void darkwatch_config_load(DarkwatchConfig *cfg, const char *exe_dir);

/* Apply the values the runtime reads from the environment (RECOMP_*).
 * Called by the bootstrap before the guest starts. */
void darkwatch_config_title_defaults(void);
void darkwatch_config_apply_env(const DarkwatchConfig *cfg);

#endif /* DARKWATCH_CONFIG_H */
