#include "game/creature.h"
#include "game/game_obj.retail.h"
#include "numath/nuvec.h"
#include "gameplay/air.h"
#include "gameplay/crash.h"
#include "gameplay/ground.h"
#include "gameplay/hazards.h"
#include "gameplay/hubgem.h"
#include "gameplay/line2d.h"
#include "gameplay/mech.h"
#include "gameplay/slidespin.h"
#include "gameplay/submarine.h"
#include "gameplay/timetrial.h"

/* Declared by no header. */
extern f32 in_speed;
extern f32 in_f_friction;
extern f32 in_s_friction;

void MoveLoopXZ(struct obj_s *obj, u16 *hdg)
{
    float speed = in_speed, forward = in_f_friction, side = in_s_friction;
    u16 walk;
    if (nw_mech_loop(obj, hdg, &walk)) {
        MoveLoopXZ__retail(obj, &walk);
        in_speed = speed;
        in_f_friction = forward;
        in_s_friction = side;
        return;
    }
    bool crash = nw_crash_on_foot(&Character[0]);
    if (obj != &Character[0].obj || !(crash || nw_coco_on_foot(&Character[0]))) {
        MoveLoopXZ__retail(obj, hdg);
        return;
    }
    if (crash) {
        nw_ground_steer(&Character[0], hdg);
        nw_slide_spin_loop(&Character[0], hdg);
    }
    struct nuvec_s v;
    NuVecRotateY(&v, &obj->mom, -(hdg ? *hdg : obj->hdg));
    nw_ground_loop(&Character[0], v.z);
    nw_air_loop(&Character[0], v.z);
    MoveLoopXZ__retail(obj, hdg);
    in_speed = speed;
    in_f_friction = forward;
    in_s_friction = side;
    if (crash) {
        nw_line2d_hold(&Character[0]);
    }
}

void PlayerCreatureCollisions(struct obj_s *obj)
{
    nw_hazards_shrink(obj);
    nw_sub_shrink();
    PlayerCreatureCollisions__retail(obj);
    nw_sub_restore();
    nw_hazards_restore();
    nw_hubgem_collide(obj);
}

void ObjectRotation(struct obj_s *obj, s32 mode, s32 blend)
{
    nw_sub_pitch_before(obj);
    ObjectRotation__retail(obj, mode, blend);
    nw_sub_pitch(obj, mode);
}

void UpdateProjectiles(void)
{
    nw_sub_shrink();
    nw_sub_shots(true);
    UpdateProjectiles__retail();
    nw_sub_shots(false);
    nw_sub_restore();
}

void AddProjectile(struct nuvec_s *src, struct nuvec_s *dst, struct nuvec_s *mom, s32 type, u16 yrot,
                   struct obj_s *owner)
{
    struct nuvec_s v;
    AddProjectile__retail(src, dst, nw_sub_launch(type, mom, owner, &v), type, yrot, owner);
}

/* Retail's spin (attack 2) knocks fruit away uncollected; without it the pickup reach is the plain radius. */
s32 WumpaCollisions(struct obj_s *obj)
{
    struct creature_s *c = &Character[0];
    const NwCrashMovement *m = nw_foot(c);
    if (obj != &c->obj || !(obj->attack & 2) || !m->spin_fruit) {
        return WumpaCollisions__retail(obj);
    }
    NW_FOOT_DEVIATION(m, spin_fruit);
    float radius = obj->radius;
    u16 attack = obj->attack;
    if (!(attack & 0xC)) {
        obj->radius *= obj->character == 1 && c->slide ? 3.0f : 2.0f;
    }
    obj->attack &= ~2;
    s32 r = WumpaCollisions__retail(obj);
    obj->radius = radius;
    obj->attack = attack;
    return r;
}

void PickupItem(struct obj_s *obj)
{
    if (!nw_time_trial_skip_pickup(obj)) {
        PickupItem__retail(obj);
    }
}
