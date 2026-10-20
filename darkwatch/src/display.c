/**
 * Darkwatch Recompiled - display options bridge
 */
#include "display.h"

#include <stdio.h>
#include <string.h>

#include <d3d8_xbox.h>

void darkwatch_display_configure(const DarkwatchConfig *cfg)
{
    XboxD3D8DisplayOptions o;

    memset(&o, 0, sizeof(o));
    o.window_width     = cfg->window_width;
    o.window_height    = cfg->window_height;
    o.resolution_scale = cfg->resolution_scale;
    o.fullscreen       = cfg->fullscreen;
    o.vsync            = cfg->vsync;
    o.widescreen       = cfg->widescreen;
    o.fov_scale        = cfg->fov_scale > 0.0f ? cfg->fov_scale : 1.0f;
    o.fps_limit        = cfg->fps_limit;

    xbox_D3D8SetWindowTitle("Darkwatch - Recompiled");
    xbox_D3D8SetDisplayOptions(&o);

    fprintf(stderr, "[display] requested: %dx%d scale %d %s vsync %d "
                    "widescreen %d fov %.2f fps_limit %d\n",
            o.window_width, o.window_height, o.resolution_scale,
            o.fullscreen ? "fullscreen" : "windowed", o.vsync,
            o.widescreen, (double)o.fov_scale, o.fps_limit);
}
