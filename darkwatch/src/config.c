/**
 * Darkwatch Recompiled - options / configuration implementation
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(_WIN32) && !defined(setenv)
/* MSVC has no setenv; _putenv_s is the same thing without the overwrite flag. */
static int dw_setenv(const char *name, const char *value, int overwrite)
{
    if (!overwrite && getenv(name)) return 0;
    return _putenv_s(name, value);
}
#define setenv dw_setenv
#endif

/* ── helpers ─────────────────────────────────────────────────────────── */

static int file_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && (st.st_mode & S_IFREG);
}

static int dir_exists(const char *path)
{
    struct stat st;
    return path && stat(path, &st) == 0 && (st.st_mode & S_IFDIR);
}

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = 0;
    return s;
}

static int parse_bool(const char *v)
{
    if (!v) return 0;
    if (!strcasecmp(v, "1") || !strcasecmp(v, "true") ||
        !strcasecmp(v, "yes") || !strcasecmp(v, "on"))
        return 1;
    return 0;
}

static void set_string(char *dst, size_t cap, const char *v)
{
    if (!v) return;
    snprintf(dst, cap, "%s", v);
}

/* ── defaults ────────────────────────────────────────────────────────── */

static void set_defaults(DarkwatchConfig *c)
{
    memset(c, 0, sizeof(*c));
    c->window_width     = 0;
    c->window_height    = 0;
    c->resolution_scale = 1;
    c->widescreen       = 0;
    c->fov_scale        = 1.0f;
    c->fullscreen       = 0;
    c->vsync            = 1;
    c->fps_limit        = 0;
    c->audio_enabled    = 1;
    c->audio_volume     = 100;
    c->keyboard         = 1;
    c->deadzone         = 10;
    c->verbose          = 0;
    c->trap_null        = 0;
    c->watchdog_seconds = 0;
    c->game_dir[0]      = 0;
}

/* ── key=value file ──────────────────────────────────────────────────── */

static void apply_key(DarkwatchConfig *c, const char *key, const char *val)
{
    if      (!strcasecmp(key, "window_width"))     c->window_width = atoi(val);
    else if (!strcasecmp(key, "window_height"))    c->window_height = atoi(val);
    else if (!strcasecmp(key, "resolution_scale")) c->resolution_scale = atoi(val);
    else if (!strcasecmp(key, "widescreen"))       c->widescreen = parse_bool(val);
    else if (!strcasecmp(key, "fov_scale"))        c->fov_scale = (float)atof(val);
    else if (!strcasecmp(key, "fullscreen"))       c->fullscreen = parse_bool(val);
    else if (!strcasecmp(key, "vsync"))            c->vsync = parse_bool(val);
    else if (!strcasecmp(key, "fps_limit"))        c->fps_limit = atoi(val);
    else if (!strcasecmp(key, "audio"))            c->audio_enabled = parse_bool(val);
    else if (!strcasecmp(key, "audio_volume"))     c->audio_volume = atoi(val);
    else if (!strcasecmp(key, "keyboard"))         c->keyboard = parse_bool(val);
    else if (!strcasecmp(key, "deadzone"))         c->deadzone = atoi(val);
    else if (!strcasecmp(key, "verbose"))          c->verbose = parse_bool(val);
    else if (!strcasecmp(key, "trap_null"))        c->trap_null = parse_bool(val);
    else if (!strcasecmp(key, "watchdog_seconds")) c->watchdog_seconds = atoi(val);
    else if (!strcasecmp(key, "game_dir"))         set_string(c->game_dir, sizeof(c->game_dir), val);
}

static int load_file(DarkwatchConfig *c, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    char line[1024];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *s = trim(line);
        if (*s == 0 || *s == '#' || *s == ';' || *s == '[') continue;
        char *eq = strchr(s, '=');
        if (!eq) {
            fprintf(stderr, "[config] %s:%d: no '=' -- ignored\n", path, lineno);
            continue;
        }
        *eq = 0;
        char *key = trim(s);
        char *val = trim(eq + 1);
        /* strip trailing comment */
        char *hash = strchr(val, '#');
        if (hash) { *hash = 0; val = trim(val); }
        apply_key(c, key, val);
    }
    fclose(f);
    fprintf(stderr, "[config] loaded %s\n", path);
    return 1;
}

/* ── environment overrides ───────────────────────────────────────────── */

static void apply_env(DarkwatchConfig *c)
{
    const char *v;
    if ((v = getenv("DARKWATCH_WINDOW_WIDTH")))     c->window_width = atoi(v);
    if ((v = getenv("DARKWATCH_WINDOW_HEIGHT")))    c->window_height = atoi(v);
    if ((v = getenv("DARKWATCH_RESOLUTION_SCALE"))) c->resolution_scale = atoi(v);
    if ((v = getenv("DARKWATCH_WIDESCREEN")))       c->widescreen = parse_bool(v);
    if ((v = getenv("DARKWATCH_FOV_SCALE")))        c->fov_scale = (float)atof(v);
    if ((v = getenv("DARKWATCH_FULLSCREEN")))       c->fullscreen = parse_bool(v);
    if ((v = getenv("DARKWATCH_VSYNC")))            c->vsync = parse_bool(v);
    if ((v = getenv("DARKWATCH_FPS_LIMIT")))        c->fps_limit = atoi(v);
    if ((v = getenv("DARKWATCH_AUDIO")))            c->audio_enabled = parse_bool(v);
    if ((v = getenv("DARKWATCH_GAME_DIR")))         set_string(c->game_dir, sizeof(c->game_dir), v);
    if ((v = getenv("DARKWATCH_VERBOSE")))          c->verbose = parse_bool(v);
    if ((v = getenv("DARKWATCH_WATCHDOG")))         c->watchdog_seconds = atoi(v);

    /* The toolkit's own switches are honoured by the runtime directly; the
     * config file only has to be able to set the common ones. */
    if (c->trap_null && !getenv("RECOMP_TRAP_NULL"))   setenv("RECOMP_TRAP_NULL", "1", 1);
    if (c->watchdog_seconds > 0 && !getenv("RECOMP_WATCHDOG_SECS")) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", c->watchdog_seconds);
        setenv("RECOMP_WATCHDOG_SECS", buf, 1);
    }
}

/* ── game directory discovery ────────────────────────────────────────── */

static int try_dir(DarkwatchConfig *c, const char *dir, const char *label)
{
    char probe[1024];
    if (!dir || !dir[0]) return 0;
    snprintf(probe, sizeof(probe), "%s/default.xbe", dir);
    if (!file_exists(probe)) return 0;
    set_string(c->game_dir, sizeof(c->game_dir), dir);
    fprintf(stderr, "[config] game data: %s (%s)\n", dir, label);
    return 1;
}

static void discover_game_dir(DarkwatchConfig *c, const char *exe_dir)
{
    char buf[1024];

    if (c->game_dir[0] && try_dir(c, c->game_dir, "configured"))
        return;

    if (try_dir(c, "game", "cwd"))          return;
    if (try_dir(c, "game_files", "cwd"))    return;
    if (try_dir(c, "../game_files", "cwd/..")) return;

    if (exe_dir && exe_dir[0]) {
        snprintf(buf, sizeof(buf), "%s/game", exe_dir);
        if (try_dir(c, buf, "exe dir")) return;
        snprintf(buf, sizeof(buf), "%s/../game_files", exe_dir);
        if (try_dir(c, buf, "exe dir/..")) return;
    }

    /* Nothing found: keep whatever was configured (even if empty) and let
     * the bootstrap fail with a message that says where it looked. */
    fprintf(stderr, "[config] no game data found (looked in game/, "
                    "game_files/, ../game_files and next to the executable)\n");
}

/* ── public ──────────────────────────────────────────────────────────── */

void darkwatch_config_load(DarkwatchConfig *cfg, const char *exe_dir)
{
    set_defaults(cfg);

    char path[1024];

    const char *env_cfg = getenv("DARKWATCH_CONFIG");
    if (env_cfg && env_cfg[0]) {
        load_file(cfg, env_cfg);
    } else {
        if (file_exists("darkwatch.ini")) {
            load_file(cfg, "darkwatch.ini");
        } else if (exe_dir && exe_dir[0]) {
            snprintf(path, sizeof(path), "%s/darkwatch.ini", exe_dir);
            if (file_exists(path)) load_file(cfg, path);
        }
    }

    apply_env(cfg);
    discover_game_dir(cfg, exe_dir);

    if (cfg->resolution_scale < 1) cfg->resolution_scale = 1;
    if (cfg->resolution_scale > 8) cfg->resolution_scale = 8;
    if (cfg->deadzone < 0)   cfg->deadzone = 0;
    if (cfg->deadzone > 50)  cfg->deadzone = 50;
    if (cfg->audio_volume < 0)   cfg->audio_volume = 0;
    if (cfg->audio_volume > 100) cfg->audio_volume = 100;
}

/* Runtime behaviour Darkwatch cannot boot without. Each one was a bring-up
 * switch in scripts/run_bringup.sh; the binary is meant to run on its own,
 * so they are the title's defaults now. An explicit value in the
 * environment still wins (setenv with overwrite = 0).
 *
 *   RECOMP_PB_EXEC        execute the title's pushbuffers (it drives NV2A).
 *   RECOMP_VBLANK         vblank interrupt; the D3D8 present path waits on it.
 *   RECOMP_TITLE_KEVENTS  events the title builds by hand in its own memory.
 *   RECOMP_AC97_READY     emulated APU + AC'97. Without it DirectSoundCreate
 *                         fails with DSERR_NODRIVER and XACT goes on to use
 *                         the NULL device -- a crash, not silence.
 *   RECOMP_HEAP_RECLAIM   reuse freed heap blocks. The intro movie streams
 *                         through 2 MB buffers and exhausts a grow-only heap.
 *   RECOMP_USB            the OHCI controllers XPP finds the gamepad on
 *   RECOMP_GUEST_LOCK     one guest thread runs at a time, as on the console's
 *                         single CPU. CRI schedules its workers with
 *                         NtSuspendThread; suspension is exact only with it.
 *   RECOMP_FF_TNL         fixed-function batches (77% of a level frame) go
 *                         through the transform-and-lighting unit the level
 *                         was brought up on, not the toolkit's default path.
 *   RECOMP_FB_WINDOW      show the frames in an SDL window. RECOMP_HEADLESS=1
 *                         runs headless (scripted runs, frame dumps only).
 *
 * Must run before xbox_MemoryLayoutInit, which reads RECOMP_AC97_READY. */
void darkwatch_config_title_defaults(void)
{
    setenv("RECOMP_PB_EXEC", "1", 0);
    setenv("RECOMP_VBLANK", "1", 0);
    setenv("RECOMP_TITLE_KEVENTS", "1", 0);
    setenv("RECOMP_AC97_READY", "1", 0);
    setenv("RECOMP_HEAP_RECLAIM", "1", 0);
    setenv("RECOMP_GUEST_LOCK", "1", 0);
    setenv("RECOMP_USB", "1", 0);
    setenv("RECOMP_FF_TNL", "1", 0);
    setenv("RECOMP_FB_WINDOW", "1", 0);
}

void darkwatch_config_apply_env(const DarkwatchConfig *cfg)
{
    char buf[32];

    setenv("DARKWATCH_WIDESCREEN", cfg->widescreen ? "1" : "0", 1);

    if (!getenv("RECOMP_KEYBOARD"))                 setenv("RECOMP_KEYBOARD", cfg->keyboard ? "1" : "0", 1);
    if (!getenv("RECOMP_PAD_DEADZONE"))             { snprintf(buf, sizeof(buf), "%d", cfg->deadzone); setenv("RECOMP_PAD_DEADZONE", buf, 1); }
    if (!cfg->audio_enabled && !getenv("RECOMP_NO_AUDIO")) setenv("RECOMP_NO_AUDIO", "1", 1);
    if (cfg->verbose && !getenv("RECOMP_KERNEL_LOG_BUDGET")) setenv("RECOMP_KERNEL_LOG_BUDGET", "200", 1);
}
