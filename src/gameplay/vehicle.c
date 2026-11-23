#include "game/vehicle.h"
#include "game/vehicle.retail.h"
#include "gameplay/atlasphere.h"
#include "gameplay/bosshealth.h"
#include "gameplay/firefly.h"
#include "gameplay/fixes.h"
#include "gameplay/plane.h"
#include "gameplay/spacefighter.h"

void BossBar(f32 x, f32 y, f32 z, f32 xs, f32 ys, s32 health, s32 maxhealth)
{
    x = nw_boss_health_bar_x(x);
    BossBar__retail(x, y, z, xs, ys, health, maxhealth);
    nw_boss_health_bar(x, y, z, xs, ys, health, maxhealth);
}

void ProcessMovementAtlas(struct NEWBUGGY *Atlas, f32 DeltaTime)
{
    ProcessMovementAtlas__retail(Atlas, DeltaTime);
    nw_atlas_move(Atlas, DeltaTime);
}

void DrawGlider(struct creature_s *c)
{
    nw_fix_goggles_before(c);
    DrawGlider__retail(c);
    nw_fix_goggles_after();
}

void MoveGlider(struct creature_s *Cre, struct nupad_s *Pad)
{
    nw_fix_space_stations_before();
    MoveGlider__retail(Cre, Pad);
    nw_fix_space_stations_after();
}

void ProcessSatellite(struct satellite_s *sat)
{
    ProcessSatellite__retail(sat);
    nw_fix_space_clock(!sat->active);
}

void ProcessSpaceStation(struct spacestation_s *station)
{
    s32 hitpoints = station->hitpoints;
    ProcessSpaceStation__retail(station);
    nw_fix_space_clock(station->hitpoints < hitpoints || !station->active);
}

void ControlGlider(struct GLIDERSTRUCT *glider, struct nupad_s *pad)
{
    if (!nw_firefly_control(glider, pad) && !nw_plane_control(glider, pad) && !nw_space_control(glider, pad)) {
        ControlGlider__retail(glider, pad);
    }
}

void ProcessGliderMovement(struct GLIDERSTRUCT *glider, f32 dt)
{
    if (!nw_space_movement(glider, dt)) {
        ProcessGliderMovement__retail(glider, dt);
    }
}

void GliderFire(struct GLIDERSTRUCT *glider, struct nupad_s *pad)
{
    if (!nw_firefly_fire(glider, pad) && !nw_plane_fire(glider, pad) && !nw_space_fire(glider, pad)) {
        GliderFire__retail(glider, pad);
    }
}

void DrawTorpedoTarget(void)
{
    if (!nw_firefly_marker()) {
        DrawTorpedoTarget__retail();
    }
}

s32 CollideGliderBullets(struct nuvec_s *pos, f32 radius, s32 enemy, f32 yscale, s32 debris, s32 reflect)
{
    s32 hit;
    if (!nw_space_collide(pos, radius, enemy, yscale, debris, reflect, &hit)) {
        hit = CollideGliderBullets__retail(pos, radius, enemy, yscale, debris, reflect);
    }
    return hit;
}
