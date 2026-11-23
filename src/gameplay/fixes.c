#include "gameplay/fixes.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "game/creature.h"
#include "gamelib/terrain.h"
#include "gameplay/crash.h"
#include "gameplay/gameplay.h"
#include "nu3d/nuanim.h"
#include "nu3d/nuhgobj.h"
#include "numath/numtx.h"

extern TerrainSetType *CurTerr;
extern s32 Level;
extern s32 clock_ok;
struct creature_s *FindClock(void);
void ClockOff(void);

#define NW_FIX_SPACE_LEVEL 0x1A
#define NW_FIX_SITUS 0x800

void NuHGobjEval(struct NUHGOBJ_s *hobj, int njanims, struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array);
void NuHGobjEvalAnim(struct NUHGOBJ_s *hobj, struct nuanimdata_s *animdata, float time, int njanims,
                     struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array);
void NuHGobjEvalAnimBlend(struct NUHGOBJ_s *hobj, struct nuanimdata_s *animdata1, float time1,
                          struct nuanimdata_s *animdata2, float time2, float blend, int njanims,
                          struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array);

static struct {
    struct NUHGOBJ_s *hobj;
    int joint;
    struct numtx_s mtx;
} nw_fix_goggles;
static struct NUHGOBJ_s *nw_fix_goggles_model;
static struct numtx_s nw_fix_goggles_on_face;

void nw_fix_ground_slam(struct creature_s *plr)
{
    if (nw_profile.bug_fixes && plr->obj.ground != 0 && plr->ok_slam != 0 && plr->jump_frame >= plr->jump_frames) {
        NW_DEVIATION(bug_fixes);
        plr->ok_slam = 0;
    }
}

float nw_fix_finish_top(struct obj_s *obj)
{
    float top = obj->top;
    struct creature_s *c = player;
    if (nw_profile.bug_fixes && obj == &c->obj && (nw_crash_on_foot(c) || nw_coco_on_foot(c)) && top < obj->max.y) {
        NW_DEVIATION(bug_fixes);
        obj->top = obj->max.y;
    }
    return top;
}

static int nw_fix_joint(const struct NUHGOBJ_s *hobj, const char *name)
{
    for (int j = 0; j < hobj->num_joints; j++) {
        if (hobj->joints[j].name && strcmp(hobj->joints[j].name, name) == 0) {
            return j;
        }
    }
    return -1;
}

static bool nw_fix_has(const struct CharacterModel *m, int action)
{
    return (u16)action < 0x76 && m->anmdata[action] != NULL;
}

static bool nw_fix_pose(struct CharacterModel *m, AnimPacket *a, struct numtx_s *tmtx)
{
    if (a->blend && nw_fix_has(m, a->blend_src_action) && nw_fix_has(m, a->blend_dst_action)) {
        NuHGobjEvalAnimBlend(m->obj, m->anmdata[a->blend_src_action], a->blend_src_time, m->anmdata[a->blend_dst_action],
                             a->blend_dst_time, (float)a->blend_frame / (float)a->blend_frames, 0, NULL, tmtx);
        return true;
    }
    if (!a->blend && nw_fix_has(m, a->action)) {
        NuHGobjEvalAnim(m->obj, m->anmdata[a->action], a->anim_time, 0, NULL, tmtx);
        return true;
    }
    NuHGobjEval(m->obj, 0, NULL, tmtx);
    return false;
}

void nw_fix_goggles_before(struct creature_s *c)
{
    static struct numtx_s rider[256], vehicle[256];
    nw_fix_goggles.hobj = NULL;
    if (!nw_profile.bug_fixes || c->obj.vehicle < 0 || CRemap[c->obj.vehicle] < 0 || !c->obj.model) {
        return;
    }
    struct CharacterModel *v = &CModel[CRemap[c->obj.vehicle]];
    int goggles = nw_fix_joint(v->obj, "gogglesControl"), face = nw_fix_joint(c->obj.model->obj, "Face");
    if (goggles < 0 || face < 0) {
        return;
    }
    NW_DEVIATION(bug_fixes);
    nw_fix_pose(c->obj.model, &c->obj.anim, rider);
    if (nw_fix_pose(v, &c->obj.anim, vehicle)) {
        struct numtx_s inv;
        NuMtxInv(&inv, &rider[face]);
        NuMtxMul(&nw_fix_goggles_on_face, &vehicle[goggles], &inv);
        nw_fix_goggles_model = v->obj;
    } else if (nw_fix_goggles_model == v->obj) {
        nw_fix_goggles.hobj = v->obj;
        nw_fix_goggles.joint = goggles;
        NuMtxMul(&nw_fix_goggles.mtx, &nw_fix_goggles_on_face, &rider[face]);
    }
}

void nw_fix_goggles_after(void)
{
    nw_fix_goggles.hobj = NULL;
}

void nw_fix_goggles_place(struct NUHGOBJ_s *hobj, struct numtx_s *mtx_array)
{
    if (hobj && hobj == nw_fix_goggles.hobj) {
        mtx_array[nw_fix_goggles.joint] = nw_fix_goggles.mtx;
    }
}

static const struct nuvec_s nw_fix_space_old[] = {
    { -14.860422f, 0.0f, 28.568737f },
    { -15.846087f, 0.0f, -26.554375f },
    { 32.109058f, 0.0f, -0.205634f },
};
static TerrainSituType *nw_fix_space_hidden[16];
static int nw_fix_space_nhidden;

static bool nw_fix_space_old_station(const struct nuvec_s *at)
{
    for (size_t i = 0; i < SDL_arraysize(nw_fix_space_old); i++) {
        const struct nuvec_s *s = &nw_fix_space_old[i];
        if (fabsf(at->x - s->x) < 0.01f && fabsf(at->y - s->y) < 0.01f && fabsf(at->z - s->z) < 0.01f) {
            return true;
        }
    }
    return false;
}

void nw_fix_space_stations_before(void)
{
    nw_fix_space_nhidden = 0;
    if (!nw_profile.bug_fixes || Level != NW_FIX_SPACE_LEVEL || !CurTerr) {
        return;
    }
    for (int i = 0; i < NW_FIX_SITUS && nw_fix_space_nhidden < (int)SDL_arraysize(nw_fix_space_hidden); i++) {
        TerrainSituType *t = &CurTerr->terr[i];
        if (t->type == 0 && nw_fix_space_old_station(&t->Location)) {
            NW_DEVIATION(bug_fixes);
            t->type = -1;
            nw_fix_space_hidden[nw_fix_space_nhidden++] = t;
        }
    }
}

void nw_fix_space_stations_after(void)
{
    for (int i = 0; i < nw_fix_space_nhidden; i++) {
        nw_fix_space_hidden[i]->type = 0;
    }
    nw_fix_space_nhidden = 0;
}

void nw_fix_space_clock(bool progress)
{
    if (!progress || !nw_profile.bug_fixes || (!clock_ok && !FindClock())) {
        return;
    }
    NW_DEVIATION(bug_fixes);
    ClockOff();
}
