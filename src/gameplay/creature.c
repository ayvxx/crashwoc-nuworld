#include <string.h>
#include <math.h>
#include <SDL3/SDL.h>

#include "game/creature.h"
#include "game/creature.retail.h"
#include "gameplay/airindicator.h"
#include "gameplay/character_select.h"
#include "gameplay/gems.h"
#include "gameplay/ground.h"
#include "gameplay/hubcrunch.h"
#include "gameplay/hubgem.h"
#include "gameplay/hitboxes.h"
#include "gameplay/mech.h"
#include "gameplay/slidespin.h"
#include "gameplay/submarine.h"
#include "nups2/ps2ctl.h"
#include "nu3d/nuhgobj.h"
#include "nu3d/nuanim.h"
#include "numath/numtx.h"
#include "numath/nutrig.h"
#include "game/gametypes.h"

extern s32 VEHICLECONTROL, Level;
extern struct MoveInfo CocoMoveInfo, CrashMoveInfo;
extern struct gtimer_s GameTimer;
extern float NuTrigTable[];
static bool nw_drawing_player;
static bool nw_coco_pose_active;
static bool nw_coco_plane_pose;
static float nw_coco_biplane_lift, nw_coco_biplane_back;
static void nw_coco_scuba_bubbles(void);
static bool nw_coco_swim_controls, nw_scuba_death_active;
static int nw_scuba_death_level = -1;

bool nw_coco_is_scuba_player(const struct obj_s *obj)
{
    return player && obj == &player->obj && VEHICLECONTROL == 2 &&
           (obj->character == 1 || nw_coco_swim_controls);
}

/* Use Crash's idle/walk/run split with clips authored for Coco's skeleton. */
float nw_coco_atlas_animation(struct creature_s *c, float dt)
{
    if (c != player || c->obj.character != 1 || c->obj.dead || VEHICLECONTROL != 1 ||
        c->obj.vehicle != 0x53) return dt;
    const float speed = c->obj.xz_distance;
    if (speed < 0.005f && c->obj.pad_speed == 0.0f) {
        c->obj.anim.newaction = 0x22;
        return dt;
    }
    const bool walking = speed < CrashMoveInfo.WALKSPEED;
    c->obj.anim.newaction = walking ? 0x74 : 0x3a;
    return dt * SDL_clamp(speed / (walking ? 0.029f : 0.060f), 0.60f, 1.65f);
}

void MovePlayer(struct creature_s *c, struct nupad_s *pad)
{
    static struct nupad_s aim;
    nw_character_select_tick(c);
    pad = nw_character_select_pad(c, pad);
    nw_mech_aim(c);
    nw_sub_move(c);
    pad = nw_crunch_pad(c, pad);
    const bool use_stock_swim_controls = c == player && c->obj.character == 1 && VEHICLECONTROL == 2;
    nw_coco_swim_controls = use_stock_swim_controls;
    if (use_stock_swim_controls) {
        c->obj.character = 0;
    }
    MovePlayer__retail(c, nw_mech_pad(c, pad, &aim));
    if (use_stock_swim_controls) {
        c->obj.character = 1;
    }
    nw_coco_swim_controls = false;
    if (c == player) {
        if (!c->obj.dead) nw_scuba_death_active = false;
        else if (nw_scuba_death_active) nw_coco_scuba_bubbles();
    }
    nw_crunch_push(c);
}

void CloseCreatures(void)
{
    nw_scuba_death_active = false;
    nw_character_select_restore();
    nw_crunch_close();
    nw_hubgem_close();
    CloseCreatures__retail();
}

void LoadCharacterModels(void)
{
    nw_character_select_load_models();
    nw_ground_loaded();
    nw_gems_models();
}

void DrawCreatures(struct creature_s *c, s32 count, s32 render, s32 shadow)
{
    nw_slide_spin_lean(c, count);
    nw_drawing_player = c == player;
    DrawCreatures__retail(c, count, render, shadow);
    nw_drawing_player = false;
    nw_slide_spin_unlean();
    if (c == &Character[0] && count == 1 && render && shadow) {
        nw_air_indicator_draw(c);
        nw_crunch_draw();
        nw_hitboxes_draw();
    }
}

extern float vtog_time, vtog_duration;
void NuHGobjEval(struct NUHGOBJ_s *, s32, struct NUJOINTANIM_s *, struct numtx_s *);
void NuHGobjEvalAnim(struct NUHGOBJ_s *, struct nuanimdata_s *, float, s32,
                     struct NUJOINTANIM_s *, struct numtx_s *);
void NuHGobjEvalAnimBlend(struct NUHGOBJ_s *, struct nuanimdata_s *, float,
                          struct nuanimdata_s *, float, float, s32,
                          struct NUJOINTANIM_s *, struct numtx_s *);

enum { COCO_HANG_MAX_JOINTS = 256, COCO_SCUBA_GEAR_JOINTS = 256 };
struct nw_mech_pilot {
    struct nuvec_s delta[4];
};
static const struct nw_mech_pilot *nw_mech_pilot;
int NuHGobjRndrMtxDwa(struct NUHGOBJ_s *, struct numtx_s *, int, short *,
                     struct numtx_s *, float **);

static const char *nw_coco_joint_name(const struct NUHGOBJ_s *body, int i)
{
    const char *name = body->joints[i].name;
    const char *leaf = name ? strrchr(name, '|') : NULL;
    return leaf ? leaf + 1 : name;
}

static void nw_rotate_hang_chain(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                                   s32 root, struct numtx_s *rotation) {
    float x = pose[root]._30, y = pose[root]._31, z = pose[root]._32;
    s32 i;

    rotation->_30 = x - (x * rotation->_00 + y * rotation->_10 + z * rotation->_20);
    rotation->_31 = y - (x * rotation->_01 + y * rotation->_11 + z * rotation->_21);
    rotation->_32 = z - (x * rotation->_02 + y * rotation->_12 + z * rotation->_22);
    for (i = root; i < body->num_joints; i++) {
        s32 parent = i;
        while (parent > root && parent < body->num_joints) {
            s32 next = body->joints[parent].parent_ix;
            if (next >= parent) break;
            parent = next;
        }
        if (parent == root) NuMtxMulH(&pose[i], &pose[i], rotation);
    }
}

static struct nuvec_s nw_pilot_direction(const struct numtx_s *a, const struct numtx_s *b) {
    struct nuvec_s d = { b->_30 - a->_30, b->_31 - a->_31, b->_32 - a->_32 };
    return d;
}

static float nw_pilot_dot(struct nuvec_s a, struct nuvec_s b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static void nw_pilot_aim(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                          s32 joint, struct nuvec_s from, struct nuvec_s to) {
    struct numtx_s rotation;
    NuMtxSetIdentity(&rotation);
    NuMtxRotateZ(&rotation, NuAtan2D(from.x, from.y));
    NuMtxRotateX(&rotation, -NuAtan2D(from.z, sqrtf(from.x * from.x + from.y * from.y)));
    NuMtxRotateX(&rotation, NuAtan2D(to.z, sqrtf(to.x * to.x + to.y * to.y)));
    NuMtxRotateZ(&rotation, -NuAtan2D(to.x, to.y));
    nw_rotate_hang_chain(body, pose, joint, &rotation);
}

static void nw_pilot_place_limb(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                                  s32 root, s32 hinge, s32 tip, struct nuvec_s goal,
                                  struct nuvec_s pole) {
    struct nuvec_s upper = nw_pilot_direction(&pose[root], &pose[hinge]);
    struct nuvec_s lower = nw_pilot_direction(&pose[hinge], &pose[tip]);
    struct nuvec_s forward = { goal.x - pose[root]._30, goal.y - pose[root]._31, goal.z - pose[root]._32 };
    struct nuvec_s upper_goal, lower_goal;
    float a = sqrtf(nw_pilot_dot(upper, upper));
    float b = sqrtf(nw_pilot_dot(lower, lower));
    float d = sqrtf(nw_pilot_dot(forward, forward));
    float along, height, proj, pole_len, reach;

    if (a < 0.001f || b < 0.001f || d < 0.001f) return;
    forward.x /= d;
    forward.y /= d;
    forward.z /= d;
    proj = nw_pilot_dot(pole, forward);
    pole.x -= proj * forward.x;
    pole.y -= proj * forward.y;
    pole.z -= proj * forward.z;
    pole_len = sqrtf(nw_pilot_dot(pole, pole));
    if (pole_len < 0.001f) return;
    reach = a + b - 0.002f;
    if (d > reach) d = reach;
    reach = (a > b ? a - b : b - a) + 0.002f;
    if (d < reach) d = reach;
    along = (a * a - b * b + d * d) / (2.0f * d);
    height = sqrtf(fmaxf(0.0f, a * a - along * along)) / pole_len;
    upper_goal.x = along * forward.x + height * pole.x;
    upper_goal.y = along * forward.y + height * pole.y;
    upper_goal.z = along * forward.z + height * pole.z;
    nw_pilot_aim(body, pose, root, upper, upper_goal);

    lower = nw_pilot_direction(&pose[hinge], &pose[tip]);
    lower_goal.x = d * forward.x - upper_goal.x;
    lower_goal.y = d * forward.y - upper_goal.y;
    lower_goal.z = d * forward.z - upper_goal.z;
    nw_pilot_aim(body, pose, hinge, lower, lower_goal);
}

static float nw_mech_limit_travel(float value, float limit) {
    if (value < -limit) return -limit;
    if (value > limit) return limit;
    return value;
}

static void nw_pose_mech_pilot(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                                 const struct nw_mech_pilot *rig) {
    static const char *const tips[] = {"leftWrist", "rightWrist", "leftFoot", "rightFoot"};
    s32 i, tip;

    for (i = 0; i < 4; i++) {
        s32 hinge, root;
        struct nuvec_s goal, pole;
        struct numtx_s foot_rest;
        float side = (i & 1) == 0 ? 1.0f : -1.0f;
        for (tip = 0; tip < body->num_joints; tip++) {
            const char *name = nw_coco_joint_name(body, tip);
            if (name != NULL && SDL_strcasecmp(name, tips[i]) == 0) break;
        }
        if (tip >= body->num_joints) continue;
        hinge = body->joints[tip].parent_ix;
        if (hinge < 0 || hinge >= tip) continue;
        root = body->joints[hinge].parent_ix;
        if (root < 0 || root >= hinge) continue;

        if (i < 2) {
            /* Keep the grasp inside the cockpit while tracking the levers. */
            goal.x = pose[root]._30 + side * 0.14f + rig->delta[i].x * 0.40f;
            goal.y = pose[root]._31 - 0.055f + rig->delta[i].y * 0.45f;
            goal.z = pose[root]._32 - 0.16f + rig->delta[i].z * 0.45f;
            pole = (struct nuvec_s){ side * 0.7f, -1.0f, 0.10f };
        } else {
            /* Each foot tracks its own mech ankle in cockpit space. Keep the
             * travel inside the cockpit and put the resting foot under the hip. */
            goal.x = pose[root]._30 + side * 0.040f +
                     nw_mech_limit_travel(rig->delta[i].x * 0.40f, 0.045f);
            goal.y = pose[root]._31 - 0.235f +
                     nw_mech_limit_travel(rig->delta[i].y * 0.62f, 0.085f);
            goal.z = pose[root]._32 - 0.025f +
                     nw_mech_limit_travel(rig->delta[i].z * 0.72f, 0.085f);
            pole = (struct nuvec_s){ side * 0.24f, 0.0f, -1.0f };
        }
        if (i == 2) foot_rest = pose[tip];
        nw_pilot_place_limb(body, pose, root, hinge, tip, goal, pole);
        if (i == 2) {
            struct numtx_s inverse, correction;
            s32 j;
            /* Follow the ankle's position while keeping the left shoe from twisting. */
            foot_rest._30 = pose[tip]._30;
            foot_rest._31 = pose[tip]._31;
            foot_rest._32 = pose[tip]._32;
            NuMtxInvH(&inverse, &pose[tip]);
            NuMtxMulH(&correction, &inverse, &foot_rest);
            for (j = tip; j < body->num_joints; j++) {
                s32 parent = j;
                while (parent > tip && parent < body->num_joints) {
                    s32 next = body->joints[parent].parent_ix;
                    if (next >= parent) break;
                    parent = next;
                }
                if (parent == tip) NuMtxMulH(&pose[j], &pose[j], &correction);
            }
        }
    }
}

static void nw_coco_hang_arm(struct NUHGOBJ_s *body, struct numtx_s *pose,
                              const char *arm_name, const char *wrist_name, u16 phase) {
    s32 arm = -1, wrist = -1, shoulders = -1, elbow, i;
    struct numtx_s reach;
    float x, y, z, dx, dy, dz;
    s32 yaw, pitch, step = 0;

    for (i = 0; i < body->num_joints; i++) {
        const char *name = nw_coco_joint_name(body, i);
        if (name != NULL && SDL_strcasecmp(name, arm_name) == 0) arm = i;
        if (name != NULL && SDL_strcasecmp(name, wrist_name) == 0) wrist = i;
        if (name != NULL && SDL_strcasecmp(name, "shoulders") == 0) shoulders = i;
    }
    if (arm < 0 || wrist < 0) return;

    elbow = body->joints[wrist].parent_ix;
    if (elbow >= 0 && elbow < body->num_joints && body->joints[elbow].parent_ix == arm) {
        float ux = pose[elbow]._30 - pose[arm]._30;
        float uy = pose[elbow]._31 - pose[arm]._31;
        float uz = pose[elbow]._32 - pose[arm]._32;
        float fx = pose[wrist]._30 - pose[elbow]._30;
        float fy = pose[wrist]._31 - pose[elbow]._31;
        float fz = pose[wrist]._32 - pose[elbow]._32;

        /* Point the forearm along the upper arm while keeping the elbow pivot. */
        NuMtxSetIdentity(&reach);
        NuMtxRotateZ(&reach, NuAtan2D(fx, fy));
        NuMtxRotateX(&reach, -NuAtan2D(fz, sqrtf(fx * fx + fy * fy)));
        NuMtxRotateX(&reach, NuAtan2D(uz, sqrtf(ux * ux + uy * uy)));
        NuMtxRotateZ(&reach, -NuAtan2D(ux, uy));
        nw_rotate_hang_chain(body, pose, elbow, &reach);
    }

    x = pose[arm]._30;
    y = pose[arm]._31;
    z = pose[arm]._32;
    dx = pose[wrist]._30 - x;
    dy = pose[wrist]._31 - y;
    dz = pose[wrist]._32 - z;
    yaw = NuAtan2D(dx, dy);
    pitch = NuAtan2D(dz, sqrtf(dx * dx + dy * dy));
    if (!player->spin && player->obj.xz_distance > 0.002f) {
        step = (s32)(NuTrigTable[(u16)(phase + (GameTimer.frame & 31) * 2048)] * 2300.0f);
    }

    NuMtxSetIdentity(&reach);
    NuMtxRotateZ(&reach, yaw);
    NuMtxRotateX(&reach, -pitch);
    /* The reach points up now. Flip the elbow bend around that line. */
    NuMtxRotateY(&reach, 0x8000);
    NuMtxRotateX(&reach, step);
    if (shoulders >= 0) {
        enum { GRIP_SPREAD = 37 * 0x10000 / 360 };
        NuMtxRotateZ(&reach, x < pose[shoulders]._30 ? GRIP_SPREAD : -GRIP_SPREAD);
    }
    nw_rotate_hang_chain(body, pose, arm, &reach);
}

static void nw_coco_hang_hair(struct NUHGOBJ_s *body, struct numtx_s *pose) {
    enum { COCO_IDLE = 0x22 };
    struct numtx_s idle_pose[COCO_HANG_MAX_JOINTS];
    struct numtx_s inverse_face, head_transform;
    struct CharacterModel *model = player->obj.model;
    s32 face = -1, quiff = -1, ponytail = -1, i;

    if (model == NULL || model->obj != body || model->anmdata[COCO_IDLE] == NULL) {
        return;
    }
    for (i = 0; i < body->num_joints; i++) {
        const char *name = nw_coco_joint_name(body, i);
        if (name == NULL) continue;
        if (SDL_strcasecmp(name, "face") == 0) face = i;
        if (SDL_strcasecmp(name, "quiff") == 0) quiff = i;
        if (SDL_strcasecmp(name, "hairDo") == 0) ponytail = i;
    }
    if (face < 0 || quiff < 0 || ponytail < 0) {
        return;
    }
    NuHGobjEvalAnim(body, model->anmdata[COCO_IDLE], 1.0f, 0, NULL, idle_pose);
    NuMtxInvH(&inverse_face, &idle_pose[face]);
    NuMtxMulH(&head_transform, &inverse_face, &pose[face]);
    for (i = 0; i < body->num_joints; i++) {
        s32 parent = i;
        while (parent >= 0 && parent < body->num_joints) {
            s32 next;
            if (parent == quiff || parent == ponytail) {
                NuMtxMulH(&pose[i], &idle_pose[i], &head_transform);
                break;
            }
            next = body->joints[parent].parent_ix;
            if (next >= parent) break;
            parent = next;
        }
    }
}

static void nw_coco_crouch_pose(struct NUHGOBJ_s *body, struct numtx_s *pose) {
    struct numtx_s rotation;
    s32 feet[2] = {-1, -1};
    float floor = 0.0f;
    float depth = (float)player->crouch_pos / CocoMoveInfo.CROUCHINGFRAMES;
    const int walking = player->crawl && player->obj.pad_speed > 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    if (walking) depth = 1.0f;
    const float stride = walking ? NuTrigTable[(u16)(GameTimer.frame * 1700u)] : 0.0f;
    for (s32 i = 0; i < body->num_joints; ++i) {
        const char *name = nw_coco_joint_name(body, i);
        if (name == NULL) continue;
        const int right = SDL_strncasecmp(name, "right", 5) == 0;
        const char *part = right ? name + 5 : SDL_strncasecmp(name, "left", 4) == 0 ? name + 4 : name;
        const float step = right ? -stride : stride;
        float degrees = 0.0f;
        if (SDL_strcasecmp(name, "back") == 0) degrees = walking ? -48.0f : -24.0f;
        else if (SDL_strcasecmp(name, "face") == 0) degrees = walking ? 31.0f : 17.0f;
        else if (SDL_strcasecmp(name, "hairDo") == 0) degrees = -13.0f;
        else if (SDL_strcasecmp(part, "Leg") == 0) degrees = 49.0f + step * 26.0f;
        else if (SDL_strcasecmp(part, "Knee") == 0) degrees = -92.0f + step * 11.0f;
        else if (SDL_strcasecmp(part, "Foot") == 0) {
            feet[right] = i;
            degrees = walking ? 40.0f - step * 8.0f : 43.0f;
        } else if (SDL_strcasecmp(part, "Arm") == 0) degrees = (walking ? 23.0f : 11.0f) + step * 28.0f;
        else if (SDL_strcasecmp(part, "Elbow") == 0) degrees = 17.0f;
        if (degrees == 0.0f) continue;
        NuMtxSetIdentity(&rotation);
        if (SDL_strcasecmp(part, "Arm") == 0) {
            /* The bind pose starts with arms extended horizontally.
             * Lower them immediately, then swing from that low position. */
            const s32 down = (s32)((66.0f + 8.0f * depth) * (65536.0f / 360.0f));
            NuMtxRotateZ(&rotation, right ? down : -down);
        }
        NuMtxRotateX(&rotation, (s32)(degrees * depth * (65536.0f / 360.0f)));
        nw_rotate_hang_chain(body, pose, i, &rotation);
    }
    if (feet[0] >= 0 && feet[1] >= 0) {
        floor = pose[feet[0]]._31 < pose[feet[1]]._31 ? pose[feet[0]]._31 : pose[feet[1]]._31;
        for (s32 i = 0; i < body->num_joints; ++i) pose[i]._31 += 0.034f - floor;
    }
}

static u32 nw_gear_death_tick;

static unsigned nw_scuba_bursts;
static struct nuvec_s nw_scuba_death_origin;
extern struct gdeb_s GDeb[];
void AddVariableShotDebrisEffect(s32 type, struct nuvec_s *at, s32 count, s32 yrot, s32 zrot);

float nw_coco_scuba_killed(struct obj_s *obj)
{
    nw_scuba_death_active = true;
    nw_scuba_death_level = Level;
    nw_gear_death_tick = GameTimer.frame;
    nw_scuba_bursts = 0;
    nw_scuba_death_origin = obj->pos;
    if (CRemap[1] >= 0) {
        obj->die_model[0] = CRemap[1];
        obj->die_model[1] = -1;
    }
    return 0.9f;
}

static void nw_coco_scuba_bubbles(void)
{
    if (!nw_scuba_death_active || nw_scuba_death_level != Level || nw_scuba_bursts >= 3 ||
        GameTimer.frame - nw_gear_death_tick < 8u * nw_scuba_bursts) {
        return;
    }
    struct nuvec_s at = nw_scuba_death_origin;
    at.y += 0.12f + 0.16f * nw_scuba_bursts;
    static const int effects[] = {20, 21, 22, 142};
    const unsigned count = nw_scuba_bursts == 0 ? 4u : 3u;
    for (unsigned i = 0; i < count; ++i) {
        if (GDeb[effects[i]].i >= 0) {
            const int bubbles = i == 0 ? 8 - 2 * (int)nw_scuba_bursts : i == 3 ? 6 : 5;
            AddVariableShotDebrisEffect(GDeb[effects[i]].i, &at, bubbles, 0, 0);
        }
    }
    ++nw_scuba_bursts;
}

static int nw_coco_find_joint(const struct NUHGOBJ_s *body, const char *name)
{
    for (int i = 0; i < body->num_joints; ++i) {
        const char *joint = nw_coco_joint_name(body, i);
        if (joint && SDL_strcasecmp(joint, name) == 0) return i;
    }
    return -1;
}

int nw_draw_coco_gear(struct NUHGOBJ_s *gear, struct numtx_s *world, struct numtx_s *pose);

static void nw_coco_scuba_gear(struct numtx_s *world, const struct NUHGOBJ_s *body,
                                struct numtx_s *pose) {
    static const struct {
        const char *gear_joint, *bone;
        struct nuvec_s mesh_anchor, bone_offset;
        float scale;
    } fit[] = {
        { "backpack", "back", { 0.05f, 0, -0.04f }, { 0, 0.09f, 0.10f }, 0.82f },
        { "mask", "face", { 0.02f, 0.006f, 0.017f }, { 0, 0.085f, -0.037f }, 0.90f },
    };
    struct numtx_s gear_pose[COCO_SCUBA_GEAR_JOINTS], bind[COCO_HANG_MAX_JOINTS];
    struct NUHGOBJ_s *gear;
    struct numtx_s inverse, attach, fit_matrix;
    s32 i, g, j;

    if (CRemap[157] < 0) return;
    gear = CModel[CRemap[157]].obj;
    if (gear == NULL || !gear->joints) return;

    NuHGobjEval((struct NUHGOBJ_s *)body, 0, NULL, bind);
    NuHGobjEval(gear, 0, NULL, gear_pose);
    for (i = 0; i < (s32)SDL_arraysize(fit); i++) {
        const float scale = fit[i].scale;
        const struct nuvec_s s = { scale, scale, scale };
        for (g = 0; g < gear->num_joints; g++) {
            if (nw_coco_joint_name(gear, g) && SDL_strcasecmp(nw_coco_joint_name(gear, g), fit[i].gear_joint) == 0) break;
        }
        for (j = 0; j < body->num_joints; j++) {
            if (nw_coco_joint_name(body, j) && SDL_strcasecmp(nw_coco_joint_name(body, j), fit[i].bone) == 0) break;
        }
        if (g >= gear->num_joints || j >= body->num_joints) continue;

        /* The rigid gear has zero joint translations and mesh-space pivots.
         * Its +X points up and +Y points to Coco's right. */
        NuMtxSetScale(&fit_matrix, (struct nuvec_s *)&s);
        NuMtxRotateZ(&fit_matrix, 0x4000);
        fit_matrix._30 = bind[j]._30 + fit[i].bone_offset.x -
                         (fit[i].mesh_anchor.x * fit_matrix._00 + fit[i].mesh_anchor.y * fit_matrix._10 +
                          fit[i].mesh_anchor.z * fit_matrix._20);
        fit_matrix._31 = bind[j]._31 + fit[i].bone_offset.y -
                         (fit[i].mesh_anchor.x * fit_matrix._01 + fit[i].mesh_anchor.y * fit_matrix._11 +
                          fit[i].mesh_anchor.z * fit_matrix._21);
        fit_matrix._32 = bind[j]._32 + fit[i].bone_offset.z -
                         (fit[i].mesh_anchor.x * fit_matrix._02 + fit[i].mesh_anchor.y * fit_matrix._12 +
                          fit[i].mesh_anchor.z * fit_matrix._22);
        NuMtxInvH(&inverse, &bind[j]);
        NuMtxMulH(&attach, &inverse, &pose[j]);
        NuMtxMulH(&gear_pose[g], &fit_matrix, &attach);
    }
    nw_draw_coco_gear(gear, world, gear_pose);
}

static void nw_coco_breaststroke(s32 *shoulder, s32 *elbow, s32 *depth) {
    /* Reach forward, sweep wide and down, then fold the arms in for recovery.
     * Mirroring the joint angles keeps the two arms in phase. */
    static const struct {
        u8 tick;
        s16 reach, bend, dip;
    } keys[] = {
        {  0,  70,   0, -12 },
        {  8,  70,   0, -12 },
        { 19,   0,   6, -13 },
        { 31, -67,  15, -19 },
        { 41, -23,  99, -25 },
        { 50,  35,  49, -22 },
        { 62,  70,   0, -12 },
    };
    u32 tick = (u32)GameTimer.frame % keys[sizeof(keys) / sizeof(keys[0]) - 1].tick;
    size_t i = 0;
    float t;

    while (tick >= keys[i + 1].tick && i + 2 < sizeof(keys) / sizeof(keys[0])) i++;
    t = (float)(tick - keys[i].tick) / (keys[i + 1].tick - keys[i].tick);
    t = t * t * (3.0f - 2.0f * t);
    *shoulder = (s32)((keys[i].reach + (keys[i + 1].reach - keys[i].reach) * t) * (65536.0f / 360.0f));
    *elbow = (s32)((keys[i].bend + (keys[i + 1].bend - keys[i].bend) * t) * (65536.0f / 360.0f));
    *depth = (s32)((keys[i].dip + (keys[i + 1].dip - keys[i].dip) * t) * (65536.0f / 360.0f));
}

static float nw_coco_swim_blend(int moving) {
    enum { TRANSITION_TICKS = 12 };
    static u32 last_tick;
    static s32 progress;
    static s32 last_level = -1;
    u32 now = (u32)GameTimer.frame;
    u32 elapsed = now - last_tick;
    float t;

    if (last_level != Level || elapsed > TRANSITION_TICKS) {
        progress = 0;
        elapsed = 1;
    }
    progress += moving ? (s32)elapsed : -(s32)elapsed;
    if (progress < 0) progress = 0;
    if (progress > TRANSITION_TICKS) progress = TRANSITION_TICKS;
    last_level = Level;
    last_tick = now;
    t = (float)progress / TRANSITION_TICKS;
    return t * t * (3.0f - 2.0f * t);
}

static void nw_smooth_scuba_pitch(struct numtx_s *world) {
    static s32 last_level = -1;
    static u32 last_tick;
    static s32 correction, smoothed;
    u32 tick = (u32)GameTimer.frame;
    u32 elapsed = tick - last_tick;
    s32 target;

    if (last_level == Level && elapsed == 0) {
        NuMtxPreRotateX(world, correction);
        return;
    }

    target = (s16)NuAtan2D(world->_11,
                          sqrtf(world->_10 * world->_10 + world->_12 * world->_12));
    if (last_level != Level || elapsed > 30) {
        smoothed = target;
    } else {
        s32 max_step = 225 * (s32)(elapsed > 2 ? 2 : elapsed);
        s32 step = (target - smoothed) / 3;
        if (step == 0) step = target - smoothed;
        if (step > max_step) step = max_step;
        if (step < -max_step) step = -max_step;
        smoothed += step;
    }
    correction = smoothed - target;
    last_tick = tick;
    last_level = Level;
    NuMtxPreRotateX(world, correction);
}

static void nw_coco_point_feet(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                                struct nuvec_s direction);

static s32 nw_draw_coco_scuba(struct numtx_s *world, int spinning) {
    struct CharacterModel *coco = &CModel[CRemap[1]];
    struct NUHGOBJ_s *body = coco->obj;
    struct numtx_s pose[COCO_HANG_MAX_JOINTS], swing, swim_world = *world;
    u16 phase = (u16)((u32)GameTimer.frame * 2048);
    s32 i, feet[2] = {-1, -1};
    s32 moving = player->obj.anim.newaction == 0x4c;
    float blend = nw_coco_swim_blend(moving);
    s32 kick = (s32)(NuTrigTable[phase] * (850.0f + blend * 1350.0f));
    s32 shoulder, elbow, depth;
    enum { KNEE_BEND = 6 * 0x10000 / 360, SWIM_HEAD_LIFT = 34 * 0x10000 / 360, SWIM_GAZE_LIFT = 9 * 0x10000 / 360 };
    s32 head_lift = spinning ? SWIM_HEAD_LIFT : (s32)(SWIM_HEAD_LIFT * (0.5f + blend * 0.5f));

    if (!spinning) head_lift += SWIM_GAZE_LIFT;
    if (body == NULL) return 0;
    NuHGobjEval(body, 0, NULL, pose);
    {
        float paddle = NuTrigTable[(u16)((u32)GameTimer.frame * 832)];
        s32 swim_shoulder, swim_elbow, swim_depth;
        shoulder = -6800 + (s32)(paddle * 1550.0f);
        elbow = 8200 - (s32)(paddle * 1900.0f);
        depth = -2100 + (s32)(paddle * 900.0f);
        if (blend > 0.0f) {
            nw_coco_breaststroke(&swim_shoulder, &swim_elbow, &swim_depth);
            shoulder += (s32)((swim_shoulder - shoulder) * blend);
            elbow += (s32)((swim_elbow - elbow) * blend);
            depth += (s32)((swim_depth - depth) * blend);
        }
    }
    for (i = 0; i < body->num_joints; i++) {
        const char *name = nw_coco_joint_name(body, i);
        s32 pitch = 0, sweep = 0, reach = 0;

        if (name == NULL) continue;
        if (SDL_strcasecmp(name, "face") == 0) pitch = head_lift;
        /* Keep the tail in its neutral orientation below its moving scalp root. */
        else if (SDL_strcasecmp(name, "hairDo") == 0) pitch = -head_lift;
        else if (SDL_strcasecmp(name, "leftLeg") == 0) pitch = spinning ? 0 : kick;
        else if (SDL_strcasecmp(name, "rightLeg") == 0) pitch = spinning ? 0 : -kick;
        else if (SDL_strcasecmp(name, "leftKnee") == 0) pitch = -KNEE_BEND - (kick > 0 ? kick / 4 : 0);
        else if (SDL_strcasecmp(name, "rightKnee") == 0) pitch = -KNEE_BEND + (kick < 0 ? kick / 4 : 0);
        else if (SDL_strcasecmp(name, "leftFoot") == 0) feet[0] = i;
        else if (SDL_strcasecmp(name, "rightFoot") == 0) feet[1] = i;
        else if (SDL_strcasecmp(name, "leftArm") == 0) {
            pitch = spinning ? 0x2200 : depth;
            sweep = spinning ? 0x3800 : 0;
            reach = spinning ? 0 : shoulder;
        } else if (SDL_strcasecmp(name, "rightArm") == 0) {
            pitch = spinning ? 0x2200 : depth;
            sweep = spinning ? -0x3800 : 0;
            reach = spinning ? 0 : -shoulder;
        } else if (!spinning && SDL_strcasecmp(name, "leftElbow") == 0) reach = elbow;
        else if (!spinning && SDL_strcasecmp(name, "rightElbow") == 0) reach = -elbow;
        else continue;

        NuMtxSetIdentity(&swing);
        NuMtxRotateY(&swing, sweep);
        NuMtxRotateZ(&swing, reach);
        NuMtxRotateX(&swing, pitch);
        nw_rotate_hang_chain(body, pose, i, &swing);
    }
    NuMtxPreRotateX(&swim_world, spinning ? -0x4000 : -0x3000 - (s32)(0x1000 * blend));
    if (!spinning) nw_smooth_scuba_pitch(&swim_world);
    const int face = nw_coco_find_joint(body, "face");
    const int waist = nw_coco_find_joint(body, "waist");
    if (face >= 0 && waist >= 0 && feet[0] >= 0 && feet[1] >= 0) {
        const struct numtx_s *head = &pose[face], *hips = &pose[waist];
        const struct numtx_s *left = &pose[feet[0]], *right = &pose[feet[1]];
        const struct nuvec_s center = {
            head->_30 * 0.35f + hips->_30 * 0.35f + (left->_30 + right->_30) * 0.15f,
            head->_31 * 0.35f + hips->_31 * 0.35f + (left->_31 + right->_31) * 0.15f,
            head->_32 * 0.35f + hips->_32 * 0.35f + (left->_32 + right->_32) * 0.15f,
        };
        const struct obj_s *obj = &player->obj;
        swim_world._30 = world->_30 -
            (center.x * swim_world._00 + center.y * swim_world._10 + center.z * swim_world._20);
        const float mid_height = (obj->bot + obj->top) * obj->SCALE * 0.5f;
        const float determinant = NuMtxDet3(world);
        swim_world._31 = world->_31 + (determinant < 0.0f ? -mid_height : mid_height) -
            (center.x * swim_world._01 + center.y * swim_world._11 + center.z * swim_world._21);
        swim_world._32 = world->_32 -
            (center.x * swim_world._02 + center.y * swim_world._12 + center.z * swim_world._22);
    }
    static const char *const toe_names[] = {"leftBall", "rightBall"};
    for (int side = 0; side < 2; ++side) {
        const int foot = feet[side];
        if (foot < 0) continue;
        const int knee = body->joints[foot].parent_ix;
        const int toe = nw_coco_find_joint(body, toe_names[side]);
        if (knee < 0 || knee >= foot || toe < 0) continue;
        const struct nuvec_s current = nw_pilot_direction(&pose[foot], &pose[toe]);
        const struct nuvec_s down_the_leg = nw_pilot_direction(&pose[knee], &pose[foot]);
        const float toe_length = sqrtf(nw_pilot_dot(current, current));
        const float shin_length = sqrtf(nw_pilot_dot(down_the_leg, down_the_leg));
        if (toe_length < 0.001f || shin_length < 0.001f) continue;

        /* Keep the toes pointed away from her head with a softer ankle angle. */
        const float kick_direction = 0.75f / shin_length;
        const float animated_direction = 0.25f / toe_length;
        const struct nuvec_s relaxed = {
            down_the_leg.x * kick_direction + current.x * animated_direction,
            down_the_leg.y * kick_direction + current.y * animated_direction,
            down_the_leg.z * kick_direction + current.z * animated_direction,
        };
        nw_pilot_aim(body, pose, foot, current, relaxed);
    }
    nw_coco_scuba_gear(&swim_world, body, pose);
    return NuHGobjRndrMtxDwa(body, &swim_world, body->num_layers, NULL, pose, NULL);
}

static void nw_vehicle_seat_motion(struct numtx_s *world, const AnimPacket *anim,
                                   int vehicle, const char *joint_name, int idle,
                                   struct nw_mech_pilot *pilot)
{
    if (!anim || CRemap[vehicle] < 0) {
        return;
    }
    struct CharacterModel *model = &CModel[CRemap[vehicle]];
    struct NUHGOBJ_s *body = model->obj;
    if (!body || !body->joints) {
        return;
    }
    int seat = 0;
    while (seat < body->num_joints) {
        const char *name = body->joints[seat].name;
        if (name && SDL_strcasecmp(name, joint_name) == 0) {
            break;
        }
        ++seat;
    }
    if (seat == body->num_joints) {
        return;
    }
    struct numtx_s joints[256], inverse, relative, result;
    if (model->anmdata[idle]) {
        NuHGobjEvalAnim(body, model->anmdata[idle], 1.0f, 0, NULL, joints);
    } else {
        NuHGobjEval(body, 0, NULL, joints);
    }
    const struct numtx_s neutral_seat = joints[seat];
    struct numtx_s neutral_controls[4];
    int controls[4] = {-1, -1, -1, -1};
    if (pilot) {
        static const char *const names[] = {"leftControl", "rightControl", "leftAnkle", "rightAnkle"};
        for (int i = 0; i < body->num_joints; ++i) {
            const char *name = nw_coco_joint_name(body, i);
            for (int j = 0; name && j < 4; ++j) {
                if (SDL_strcasecmp(name, names[j]) == 0) {
                    controls[j] = i;
                    neutral_controls[j] = joints[i];
                }
            }
        }
    }
    NuMtxInvH(&inverse, &joints[seat]);
    const unsigned action = anim->action;
    const unsigned from = anim->blend_src_action;
    const unsigned to = anim->blend_dst_action;
    if (anim->blend && anim->blend_frames && from < 0x76 && to < 0x76 &&
        model->anmdata[from] && model->anmdata[to]) {
        NuHGobjEvalAnimBlend(body, model->anmdata[from], anim->blend_src_time,
                             model->anmdata[to], anim->blend_dst_time,
                             (float)anim->blend_frame / anim->blend_frames, 0, NULL, joints);
    } else if (action < 0x76 && model->anmdata[action]) {
        NuHGobjEvalAnim(body, model->anmdata[action], anim->anim_time, 0, NULL, joints);
    } else {
        return;
    }
    NuMtxMulH(&relative, &inverse, &joints[seat]);
    NuMtxMulH(&result, &relative, world);
    *world = result;
    if (pilot) {
        NuMtxInvH(&inverse, &joints[seat]);
        struct numtx_s neutral = neutral_seat;
        NuMtxMulH(&relative, &inverse, &neutral);
        for (int i = 0; i < 4; ++i) {
            if (controls[i] >= 0) {
                NuMtxMulH(&result, &joints[controls[i]], &relative);
                pilot->delta[i] = nw_pilot_direction(&neutral_controls[i], &result);
            }
        }
    }
}

s32 DrawCharacterModel(struct CharacterModel *model, AnimPacket *anim,
                       struct numtx_s *world, struct numtx_s *shadow, s32 render,
                       struct numtx_s *reflection, struct numtx_s *locators,
                       struct nuvec_s *locator_motion, struct obj_s *obj)
{
    if (player && obj == &player->obj && obj->dead &&
        nw_scuba_death_active && nw_scuba_death_level == Level) {
        return 0;
    }
    struct numtx_s seat;
    AnimPacket rider;
    struct nw_mech_pilot pilot = {0};
    const bool coco = player && obj == &player->obj && obj->character == 1 &&
                      model == obj->model && !obj->dead;
    if (player && obj == &player->obj && obj->character == 1 &&
        model->character == 1 && world && VEHICLECONTROL == 1 &&
        (vtog_time == vtog_duration || obj->vehicle == 0x20) && !obj->dead) {
        float lift = 0.0f;
        float back = 0.0f;
        int seat_vehicle = obj->vehicle;
        const char *seat_joint = NULL;
        int seat_idle = 0x62;
        switch (obj->vehicle) {
        case 0xa1: /* Mine tub */
            seat_joint = "centre";
            seat_idle = 0x68;
            lift = 0.50f;
            back = -0.30f;
            break;
        case 0x63: /* Jeep */
        case 0x99: /* Offroader */
            seat_vehicle = 0x99;
            seat_joint = "jeepMain";
            lift = 0.60f;
            back = -0.20f;
            break;
        case 0x3b: /* Gyrocopter */
            seat_joint = "centre";
            lift = 0.50f;
            back = -0.10f;
            break;
        case 0x44: /* Mech */
        case 0xb2: /* Fire engine */
            seat_joint = "shoulders";
            nw_mech_pilot = &pilot;
            lift = 0.70f;
            back = -0.15f;
            break;
        case 0x36: /* Biplane */
            lift = 0.29f + 0.08f;
            back = -0.30f;
            nw_coco_biplane_lift = lift;
            nw_coco_biplane_back = back;
            nw_coco_plane_pose = true;
            break;
        case 0x81: /* Dropship */
            lift = 0.29f;
            nw_coco_plane_pose = true;
            break;
        case 0x8b: /* Firefly */
            lift = 0.04f;
            back = -0.06f;
            nw_coco_plane_pose = true;
            break;
        case 0x20: /* Submarine */
            lift = 0.04f;
            back = -0.10f;
            break;
        }
        seat = *world;
        if (seat_joint) {
            nw_vehicle_seat_motion(&seat, anim, seat_vehicle, seat_joint, seat_idle,
                                   nw_mech_pilot ? &pilot : NULL);
        }
        seat._30 += seat._10 * lift - seat._20 * back;
        seat._31 += seat._11 * lift - seat._21 * back;
        seat._32 += seat._12 * lift - seat._22 * back;
        world = &seat;
        if (anim && (seat_joint || nw_coco_plane_pose || obj->vehicle == 0x20 || (unsigned)anim->action >= 0x76 ||
                     !model->anmdata[anim->action])) {
            const int action = model->anmdata[0x62] ? 0x62 : 0x22;
            ResetAnimPacket(&rider, action);
            anim = &rider;
        }
    }
    nw_coco_pose_active = coco;
    const s32 drawn = DrawCharacterModel__retail(model, anim, world, shadow, render, reflection,
                                                locators, locator_motion, obj);
    nw_coco_pose_active = false;
    nw_coco_plane_pose = false;
    nw_mech_pilot = NULL;
    return drawn;
}
bool nw_coco_swim_render(struct NUHGOBJ_s *model, struct numtx_s *world, int *drawn)
{
    if (!nw_drawing_player || !player || player->obj.character != 1 ||
        VEHICLECONTROL != 2 || player->obj.dead || CRemap[1] < 0 ||
        !CModel[CRemap[1]].obj || !CModel[CRemap[1]].obj->joints) {
        return false;
    }
    const bool swimming = CRemap[115] >= 0 && model == CModel[CRemap[115]].obj;
    const bool spinning = CRemap[116] >= 0 && model == CModel[CRemap[116]].obj;
    if (!swimming && !spinning) {
        return false;
    }
    *drawn = nw_draw_coco_scuba(world, spinning);
    return true;
}

static void nw_coco_plane_legs(const struct NUHGOBJ_s *body, struct numtx_s *pose)
{
    static const char *const shoes[] = {"leftFoot", "rightFoot"};
    static const char *const pedals[] = {"LeftAnkle", "RightAnkle"};
    const bool biplane = player->obj.vehicle == 0x36;
    const struct CharacterModel *pilot = biplane && CRemap[0] >= 0 ? &CModel[CRemap[0]] : NULL;
    struct numtx_s reference[256];
    /* The biplane has no pedal joint. Its stock pilot pose places both feet at the controls. */
    const bool have_pedals = pilot && pilot->obj && pilot->obj->joints && pilot->anmdata[0x62];
    if (have_pedals) {
        NuHGobjEvalAnim(pilot->obj, pilot->anmdata[0x62], 1.0f, 0, NULL, reference);
    }
    for (int side = 0; side < 2; ++side) {
        const int foot = nw_coco_find_joint(body, shoes[side]);
        if (foot < 0) continue;
        const int knee = body->joints[foot].parent_ix;
        if (knee < 0 || knee >= foot) continue;
        const int hip = body->joints[knee].parent_ix;
        if (hip < 0 || hip >= knee) continue;

        struct nuvec_s goal = {
            pose[hip]._30,
            pose[hip]._31 - (biplane ? 0.12f : 0.10f),
            pose[hip]._32 - (biplane ? 0.235f : 0.19f),
        };
        const int pedal = have_pedals ? nw_coco_find_joint(pilot->obj, pedals[side]) : -1;
        if (pedal >= 0) {
            goal.x = reference[pedal]._30;
            goal.y = reference[pedal]._31 - nw_coco_biplane_lift;
            goal.z = reference[pedal]._32 + nw_coco_biplane_back;
        }
        const struct nuvec_s pole = {0.0f, 0.55f, -1.0f};
        nw_pilot_place_limb(body, pose, hip, knee, foot, goal, pole);
    }
}

static void nw_coco_point_feet(const struct NUHGOBJ_s *body, struct numtx_s *pose,
                                struct nuvec_s direction)
{
    static const char *const shoes[] = {"leftFoot", "rightFoot"};
    static const char *const toes[] = {"leftBall", "rightBall"};
    for (int side = 0; side < 2; ++side) {
        const int foot = nw_coco_find_joint(body, shoes[side]);
        const int toe = nw_coco_find_joint(body, toes[side]);
        if (foot < 0 || toe < 0) continue;
        const struct nuvec_s current = nw_pilot_direction(&pose[foot], &pose[toe]);
        nw_pilot_aim(body, pose, foot, current, direction);
    }
}

/* The grip stays fixed while the torso and legs turn around the midpoint
 * between her shoulders. Bend one knee into the kick before each rotation. */
static void nw_coco_hang_spin(struct NUHGOBJ_s *body, struct numtx_s *pose)
{
    const int active = player->spin_frames - player->minfo->SPINRESETFRAMES;
    const int frame = player->spin_frame;
    if (!player->spin || active <= 0 || frame >= active) return;

    const int left = nw_coco_find_joint(body, "leftArm");
    const int right = nw_coco_find_joint(body, "rightArm");
    const int shoulders = nw_coco_find_joint(body, "shoulders");
    const int leg = nw_coco_find_joint(body, "rightLeg");
    const int knee = nw_coco_find_joint(body, "rightKnee");
    if (left < 0 || right < 0 || shoulders < 0 || leg < 0 || knee < 0) return;

    float kick = SDL_clamp(fminf(frame, active - frame) / 4.0f, 0.0f, 1.0f);
    kick *= kick * (3.0f - 2.0f * kick);
    struct numtx_s rotation;
    NuMtxSetRotationX(&rotation, (int)(16000.0f * kick));
    nw_rotate_hang_chain(body, pose, leg, &rotation);
    NuMtxSetRotationX(&rotation, (int)(-1800.0f * kick));
    nw_rotate_hang_chain(body, pose, knee, &rotation);

    const int turns = (active + 4) / 8;
    const int angle = (int)(65536.0f * turns * frame / active);
    NuMtxSetRotationY(&rotation, angle);
    const float x = (pose[left]._30 + pose[right]._30) * 0.5f;
    const float z = (pose[left]._32 + pose[right]._32) * 0.5f;
    rotation._30 = x - x * rotation._00 - z * rotation._20;
    rotation._32 = z - x * rotation._02 - z * rotation._22;
    for (int i = 0; i < body->num_joints; ++i) {
        int joint = i;
        while (joint != left && joint != right && body->joints[joint].parent_ix < joint) {
            joint = body->joints[joint].parent_ix;
        }
        if (joint != left && joint != right) NuMtxMulH(&pose[i], &pose[i], &rotation);
    }
}

bool nw_coco_player_eyes_closed(void)
{
    return player && player->obj.character == 1 && player->obj.dead != 0;
}

bool nw_coco_body_eyes_closed(void)
{
    return nw_drawing_player && nw_coco_player_eyes_closed();
}

struct numtx_s *nw_coco_render_pose(struct NUHGOBJ_s *body, struct numtx_s *pose,
                                    struct numtx_s *adjusted)
{
    if (!nw_coco_pose_active || !body->joints || body != player->obj.model->obj || !pose) {
        return pose;
    }
    if (nw_coco_plane_pose) {
        memcpy(adjusted, pose, body->num_joints * sizeof(*pose));
        nw_coco_plane_legs(body, adjusted);
        return adjusted;
    }
    if (nw_mech_pilot) {
        memcpy(adjusted, pose, body->num_joints * sizeof(*pose));
        nw_pose_mech_pilot(body, adjusted, nw_mech_pilot);
        return adjusted;
    } else if (VEHICLECONTROL == 1 && player->obj.vehicle == 0x53 &&
               player->obj.anim.newaction == 0x3a) {
        const int back = nw_coco_find_joint(body, "back");
        if (back >= 0) {
            memcpy(adjusted, pose, body->num_joints * sizeof(*pose));
            struct numtx_s lean;
            const float amount = SDL_clamp(player->obj.xz_distance / 0.065f, 0.0f, 1.0f);
            NuMtxSetRotationX(&lean, (s32)(-2900.0f * amount));
            nw_rotate_hang_chain(body, adjusted, back, &lean);
            return adjusted;
        }
    } else if (player->obj.dangle) {
        memcpy(adjusted, pose, body->num_joints * sizeof(*pose));
        nw_coco_hang_hair(body, adjusted);
        if (!player->spin && player->obj.xz_distance > 0.001f) {
            const float sway = NuTrigTable[(u16)(GameTimer.frame * 1800u)];
            for (int i = 0; i < body->num_joints; ++i) {
                const char *name = nw_coco_joint_name(body, i);
                const bool left = name && SDL_strcasecmp(name, "leftLeg") == 0;
                const bool right = name && SDL_strcasecmp(name, "rightLeg") == 0;
                if (left || right) {
                    struct numtx_s swing;
                    NuMtxSetIdentity(&swing);
                    NuMtxRotateX(&swing, (s32)(sway * (left ? 2550.0f : -2550.0f)));
                    NuMtxRotateZ(&swing, (s32)(sway * 540.0f));
                    nw_rotate_hang_chain(body, adjusted, i, &swing);
                }
            }
        }
        nw_coco_point_feet(body, adjusted, (struct nuvec_s){0.0f, -1.0f, -0.18f});
        nw_coco_hang_arm(body, adjusted, "leftArm", "leftWrist", 0);
        nw_coco_hang_arm(body, adjusted, "rightArm", "rightWrist", 0x8000);
        nw_coco_hang_spin(body, adjusted);
        return adjusted;
    } else if (VEHICLECONTROL == 0 && player->obj.ground && !player->slide &&
               !player->spin && (player->crawl || player->crouch_pos) &&
               CocoMoveInfo.CROUCHINGFRAMES > 0) {
        NuHGobjEval(body, 0, NULL, adjusted);
        nw_coco_crouch_pose(body, adjusted);
        return adjusted;
    }
    return pose;
}
