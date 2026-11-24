#include "nu_types.h"
#include "game/gametypes.h"
#include "game/panel.retail.h"
#include "nu_port.h"
#include "gameplay/gameplay.h"
#include "gameplay/bosshealth.h"
#include "gameplay/character_select.h"
#include "gameplay/hubcrunch.h"
#include "gameplay/hubgem.h"
#include "gameplay/minigame.h"
#include "gameplay/vehiclehints.h"

/* Declared by no header. */
extern struct game_s Game;
extern char *tLOADING[];
extern f32 NuTrigTable[];

static const float nw_relic_glow_edge = 0.7f;

void DrawGameMessage(char *txt, s32 message_frame, float ys)
{
    if (txt == tLOADING[Game.language] && nw_minigame_playing()) {
        return;
    }
    DrawGameMessage__retail(txt, message_frame, ys);
}

void DrawPanelDebris(void)
{
    nw_boss_health_crunch_time();
    nw_crunch_prompt();
    nw_hubgem_message();
    nw_vehicle_hints_draw();
    DrawPanelDebris__retail();
    nw_character_select_draw();
}

void ResetPanelDebris(void)
{
    ResetPanelDebris__retail();
    nw_vehicle_hints_reset();
}

s32 DrawPanel3DTempCharacter(u16 xrot, u16 yrot, u16 zrot, s32 rot, f32 x, f32 y, f32 z, f32 scale)
{
    if (nw_boss_health_head(xrot, yrot, rot, x, y, z, scale)) {
        return 1;
    }
    return DrawPanel3DTempCharacter__retail(xrot, yrot, zrot, rot, x, y, z, scale);
}

unsigned short nw_relic_spin(unsigned short still, unsigned short turning)
{
    if (!nw_profile.relic_spin) {
        return still;
    }
    NW_DEVIATION(relic_spin);
    return turning;
}

float nw_relic_glow(float scale, unsigned short turning)
{
    if (!nw_profile.relic_spin) {
        return scale;
    }
    NW_DEVIATION(relic_spin);
    float face = NuTrigTable[(unsigned short)(turning + 0x4000)];
    return scale * (nw_relic_glow_edge + (1.0f - nw_relic_glow_edge) * face * face);
}
