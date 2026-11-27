#include <math.h>
#include <string.h>
#include <stdbool.h>

#include "nu_types.h"
#include "numath/numathtypes.h"
#include "numath/nuvec.h"
#include "gamelib/terrain.retail.h"
#include "gameplay/edges.h"
#include "gameplay/character_select.h"
#include "game/creature.h"
#include "gameplay/hubgem.h"
#include "gameplay/submarine.h"

extern s32 WallSplCount;
extern struct nuvec_s WallSplList[128];
static const struct creature_s *nw_computer_wall_npc;

void NewTerrainScaleY(struct nuvec_s *vpos, struct nuvec_s *vvel, u8 *flags, s32 terid, f32 stopflag, f32 size,
                      f32 yscale)
{
    nw_sub_accel(vvel);
    nw_edges_scan(flags);
    const struct creature_s *npc = player && flags == player->obj.gndflags.chrs
        ? nw_character_hub_npc() : NULL;
    const struct creature_s *previous_npc = nw_computer_wall_npc;
    nw_computer_wall_npc = NULL;
    if (npc) {
        const float x = vpos->x - npc->obj.pos.x;
        const float z = vpos->z - npc->obj.pos.z;
        if (x * x + z * z < 9.0f && fabsf(vpos->y - npc->obj.pos.y) < 0.9f) {
            nw_computer_wall_npc = npc;
        }
    }
    NewTerrainScaleY__retail(vpos, vvel, flags, terid, stopflag, size, yscale);
    nw_computer_wall_npc = previous_npc;
    nw_edges_scanned(flags);
}

void TerrainImpact(struct nuvec_s *vpos, struct nuvec_s *vvel, u8 *flags)
{
    nw_edges_impact(flags);
    TerrainImpact__retail(vpos, vvel, flags);
}

static struct nuvec_s nw_wall_point(struct nuvec_s a, struct nuvec_s b, float t)
{
    return (struct nuvec_s){a.x + (b.x - a.x) * t,
                            a.y + (b.y - a.y) * t,
                            a.z + (b.z - a.z) * t};
}

s32 HitWallSpline(void)
{
    nw_hubgem_walls();
    if (!nw_computer_wall_npc || WallSplCount < 0 || WallSplCount > 128) {
        return HitWallSpline__retail();
    }

    /* Keep the opening behind the NPC, on the walkway side of the desk. */
    const float radius = 1.35f;
    const struct nuvec_s center = nw_computer_wall_npc->obj.pos;
    const float desk_limit = 0.27f;
    struct nuvec_s forward = {0.0f, 0.0f, 1.0f};
    NuVecRotateY(&forward, &forward, nw_computer_wall_npc->obj.hdg);
    struct nuvec_s original[128];
    const int count = WallSplCount;
    memcpy(original, WallSplList, count * sizeof(*original));
    int written = 0;
    for (int i = 0; i + 1 < count; i += 2) {
        const struct nuvec_s a = original[i], b = original[i + 1];
        const float x = a.x - center.x, z = a.z - center.z;
        const float dx = b.x - a.x, dz = b.z - a.z;
        const float depth = x * forward.x + z * forward.z;
        const float depth_step = dx * forward.x + dz * forward.z;
        const float side = (x * forward.z - z * forward.x) * 1.5f;
        const float side_step = (dx * forward.z - dz * forward.x) * 1.5f;
        const float length_sq = depth_step * depth_step + side_step * side_step;
        const float along = depth * depth_step + side * side_step;
        const float outside = depth * depth + side * side - radius * radius;
        const float discriminant = along * along - length_sq * outside;
        if (length_sq > 0.0f && discriminant > 0.0f) {
            const float root = sqrtf(discriminant);
            float enter = (-along - root) / length_sq;
            float leave = (-along + root) / length_sq;
            if (depth_step > 0.00001f) leave = fminf(leave, (desk_limit - depth) / depth_step);
            else if (depth_step < -0.00001f) enter = fmaxf(enter, (desk_limit - depth) / depth_step);
            else if (depth > desk_limit) leave = enter;
            if (enter < leave && enter < 1.0f && leave > 0.0f &&
                (!(enter > 0.0f && leave < 1.0f) || written + count - i + 2 <= 128)) {
                if (enter > 0.0f) {
                    WallSplList[written++] = a;
                    WallSplList[written++] = nw_wall_point(a, b, enter);
                }
                if (leave < 1.0f) {
                    WallSplList[written++] = nw_wall_point(a, b, leave);
                    WallSplList[written++] = b;
                }
                continue;
            }
        }
        WallSplList[written++] = a;
        WallSplList[written++] = b;
    }
    /* Three outward-facing edges around the small computer in front of the NPC. */
    if (written <= 122) {
        const float front = 0.48f, back = 0.98f, half_width = 0.43f;
        const struct nuvec_s left = {
            center.x + forward.x * front - forward.z * half_width, center.y,
            center.z + forward.z * front + forward.x * half_width,
        };
        const struct nuvec_s right = {
            center.x + forward.x * front + forward.z * half_width, center.y,
            center.z + forward.z * front - forward.x * half_width,
        };
        const struct nuvec_s back_left = {left.x + forward.x * (back - front),
            left.y, left.z + forward.z * (back - front)};
        const struct nuvec_s back_right = {right.x + forward.x * (back - front),
            right.y, right.z + forward.z * (back - front)};
        WallSplList[written++] = back_left;
        WallSplList[written++] = left;
        WallSplList[written++] = left;
        WallSplList[written++] = right;
        WallSplList[written++] = right;
        WallSplList[written++] = back_right;
    }
    WallSplCount = written;
    const s32 hit = HitWallSpline__retail();
    memcpy(WallSplList, original, count * sizeof(*original));
    WallSplCount = count;
    return hit;
}
