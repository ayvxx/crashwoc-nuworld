#include "gameplay/character_select.h"

#include <SDL3/SDL.h>

#include "game/creature.h"
#include "game/creature.retail.h"
#include "game/gametypes.h"
#include "nups2/ps2ctl.h"
#include "platform/log.h"
#include "shim/shim.h"

extern struct cursor_s Cursor;
extern struct cammtx_s *pCam;
extern f32 PANEL3DMULX, PANEL3DMULY;
extern s32 Paused;
void Text3D(char *, float, float, float, float, float, float, s32, s32);
void GameSfx(s32, struct nuvec_s *);

enum { NW_CRASH_SPIN_SOUND = 0x13, NW_COCO_SPIN_SOUND = 0x1a };

extern struct ldata_s *LDATA;
extern s32 Level, Demo, cutmovie, VEHICLECONTROL, LEVELAICOUNT;
extern struct aitab_s AITab[];
extern struct aitype_s AIType[];
extern float IDLEWAIT;
extern struct MoveInfo CocoMoveInfo, CrashMoveInfo;
void *NuDatOpen(char *name, char **memory, s32 *used);
void NuDatSet(void *dat);
void NuDatClose(void *dat);
struct nuanimdata_s *InstAnimDataLoad(char *name);

enum {
    NW_CHARACTER_DEFAULT,
    NW_CHARACTER_CRASH,
    NW_CHARACTER_COCO,
    NW_CHARACTER_HUB = 0x25,
    NW_CHARACTER_LIST_END = 0xff,
};

static SDL_AtomicInt nw_character_request;
static SDL_AtomicInt nw_character_status;
static struct ldata_s *nw_overridden_level;
static short nw_original_character;
static int nw_character_choice;
static char *nw_character_choice_path;
static bool nw_switch_consumed;
struct nuanimdata_s *nw_coco_loading_fall;

void nw_character_select_load_choice(const char *root)
{
    SDL_free(nw_character_choice_path);
    nw_character_choice_path = NULL;
    if (!root || SDL_asprintf(&nw_character_choice_path, "%s/character-choice", root) < 0) return;
    size_t size = 0;
    char *choice = SDL_LoadFile(nw_character_choice_path, &size);
    if (choice && size == 1 && (choice[0] == '1' || choice[0] == '2')) {
        nw_character_choice = choice[0] - '0';
    }
    SDL_free(choice);
}

static void nw_character_save_choice(int choice)
{
    if (!nw_character_choice_path) return;
    char *temporary = NULL;
    const char value = '0' + choice;
    if (SDL_asprintf(&temporary, "%s.tmp", nw_character_choice_path) < 0) return;
    if (!SDL_SaveFile(temporary, &value, 1) || !SDL_RenamePath(temporary, nw_character_choice_path)) {
        nw_log_warn(NW_LOG_GAME, "Could not save the selected character: %s", SDL_GetError());
        SDL_RemovePath(temporary);
    }
    SDL_free(temporary);
}
static AnimList nw_coco_vehicle_pose = {
    .file = "a\\dropship", .action = 0x62, .blend_in_frames = 5,
    .blend_out_frames = 5, .speed = 1.0f, .flags = 2,
};

void nw_character_select_restore(void)
{
    if (nw_overridden_level) {
        nw_overridden_level->character = nw_original_character;
        nw_overridden_level = NULL;
    }
    nw_switch_consumed = false;
    nw_coco_loading_fall = NULL;
    SDL_SetAtomicInt(&nw_character_status, 0);
}

static bool nw_character_supported_level(void)
{
    return LDATA && Demo == 0 && cutmovie == -1 &&
           (LDATA->flags & 1) &&
           (unsigned)LDATA->character <= 1 && Level != 0x28;
}

static bool nw_character_loaded(int character)
{
    const int index = CRemap[character];
    return index >= 0 && index < NW_CHARACTER_MODEL_CAPACITY && CModel[index].obj &&
           CModel[index].anmdata[0x22] && CModel[index].anmdata[0x74];
}

static void nw_character_set_level(int character)
{
    if (!nw_overridden_level) {
        nw_overridden_level = LDATA;
        nw_original_character = LDATA->character;
    }
    LDATA->character = (short)character;
}

static void nw_load_coco_loading_fall(void)
{
    if (!nw_character_loaded(1)) return;
    void *archive = NuDatOpen("chars.dat", NULL, NULL);
    if (!archive) return;
    NuDatSet(archive);
    nw_coco_loading_fall = InstAnimDataLoad("chars\\a\\coco\\b\\falllong.ani");
    NuDatSet(NULL);
    NuDatClose(archive);
}

void nw_character_select_load_models(void)
{
    nw_character_select_restore();
    if (!nw_character_supported_level() || !LDATA->clist ||
        (Level != NW_CHARACTER_HUB && nw_character_choice == NW_CHARACTER_DEFAULT)) {
        LoadCharacterModels__retail();
        nw_load_coco_loading_fall();
        return;
    }

    u8 models[NW_CHARACTER_MODEL_CAPACITY + 1];
    unsigned count = 0;
    unsigned present = 0;
    bool scuba_gear = false;
    const bool needs_scuba = nw_character_choice == NW_CHARACTER_COCO &&
                             (Level == 2 || Level == 6 || Level == 34);
    u8 *const original_list = LDATA->clist;
    while (count < NW_CHARACTER_MODEL_CAPACITY && original_list[count] != NW_CHARACTER_LIST_END) {
        const u8 character = original_list[count];
        if (character < 2) {
            present |= 1u << character;
        }
        scuba_gear |= character == 157;
        models[count++] = character;
    }
    const unsigned needed = Level == NW_CHARACTER_HUB ? 3u : 1u << (nw_character_choice - 1);
    for (unsigned character = 0; character < 2 && count < NW_CHARACTER_MODEL_CAPACITY; ++character) {
        const unsigned bit = 1u << character;
        if ((needed & bit) && !(present & bit)) {
            models[count++] = (u8)character;
        }
    }
    if (needs_scuba && !scuba_gear && count < NW_CHARACTER_MODEL_CAPACITY) {
        models[count++] = 157;
    }
    models[count] = NW_CHARACTER_LIST_END;
    LDATA->clist = models;
    LoadCharacterModels__retail();
    LDATA->clist = original_list;
    nw_load_coco_loading_fall();

    if (nw_character_loaded(1)) {
        struct CharacterModel *coco = &CModel[CRemap[1]];
        if (!coco->anmdata[0x62] &&
            ((nw_character_choice == NW_CHARACTER_COCO && LDATA->vehicle != -1) ||
             LDATA->character == 1)) {
            void *archive = NuDatOpen("chars.dat", NULL, NULL);
            if (archive) {
                NuDatSet(archive);
                coco->anmdata[0x62] = InstAnimDataLoad("chars\\a\\coco\\a\\dropship.ani");
                if (coco->anmdata[0x62]) {
                    coco->animlist[0x62] = &nw_coco_vehicle_pose;
                }
                NuDatSet(NULL);
                NuDatClose(archive);
            }
        }
        if (LDATA->vehicle == 0x3b && coco->anmdata[0x62]) {
            static const unsigned actions[] = {0x5a, 0x65, 0x67, 0x69, 0x70};
            for (unsigned i = 0; i < SDL_arraysize(actions); ++i) {
                const unsigned action = actions[i];
                if (!coco->anmdata[action]) {
                    coco->anmdata[action] = coco->anmdata[0x62];
                    coco->animlist[action] = coco->animlist[0x62];
                }
            }
        }
        CocoMoveInfo.CROUCHINGFRAMES = CrashMoveInfo.CROUCHINGFRAMES > 0
                                          ? CrashMoveInfo.CROUCHINGFRAMES : 10;
        if (CrashMoveInfo.SLIDEFRAMES > 0) {
            CocoMoveInfo.SLIDESPEED = CrashMoveInfo.SLIDESPEED;
        }
    }
    if (nw_character_choice != NW_CHARACTER_DEFAULT) {
        const int character = nw_character_choice - 1;
        if (nw_character_loaded(character) && (character == 1 || LDATA->vehicle == -1)) {
            nw_character_set_level(character);
        }
    }
}

struct creature_s *nw_character_hub_npc(void)
{
    if (Level != NW_CHARACTER_HUB || !nw_character_supported_level() ||
        !nw_character_loaded(0) || !nw_character_loaded(1)) return NULL;

    for (int i = 1; i < 9; ++i) {
        struct creature_s *npc = &Character[i];
        const int ai = npc->i_aitab;
        if (npc->used && !npc->obj.dead && ai >= 0 && ai < LEVELAICOUNT &&
            SDL_strcmp(AIType[AITab[ai].ai_type].name, "coco") == 0) {
            return npc;
        }
    }
    return NULL;
}

static void nw_character_swap_hub_npc(struct creature_s *npc, int character)
{
    if (!npc || npc->obj.character == character) return;
    ChangeCharacter(npc, character);
    ResetAnimPacket(&npc->obj.anim, 0x22);
    npc->idle_mode = 0;
    npc->idle_action = 0x22;
    npc->idle_time = 0.0f;
    npc->idle_wait = IDLEWAIT * 30.0f;
    npc->idle_repeat = 0;
    npc->idle_sigh = 0;
}

static void nw_character_apply_choice(struct creature_s *creature, int character)
{
    if (creature->obj.character != character) ChangeCharacter(creature, character);
    nw_character_swap_hub_npc(nw_character_hub_npc(), 1 - character);
    const int choice = character + 1;
    if (choice != nw_character_choice) {
        nw_character_save_choice(choice);
        nw_character_choice = choice;
    }
    nw_character_set_level(character);
    SDL_SetAtomicInt(&nw_character_status, choice);
}

void nw_character_select_tick(struct creature_s *creature)
{
    if (creature != player) return;
    const bool available = nw_character_supported_level() && Level == NW_CHARACTER_HUB &&
                           VEHICLECONTROL == 0 && !creature->obj.dead &&
                           !creature->obj.transporting && !creature->obj.finished &&
                           nw_character_loaded(0) && nw_character_loaded(1);
    SDL_SetAtomicInt(&nw_character_status, available ? creature->obj.character + 1 : 0);
    const int request = SDL_SetAtomicInt(&nw_character_request, 0);
    nw_character_swap_hub_npc(nw_character_hub_npc(), creature->obj.character == 1 ? 0 : 1);
    if (available && request >= NW_CHARACTER_CRASH && request <= NW_CHARACTER_COCO) {
        nw_character_apply_choice(creature, request - 1);
    }
}

static bool nw_can_switch_at_computer(const struct creature_s *creature)
{
    if (!creature || !nw_character_select_status() || Paused || Cursor.menu != -1 ||
        !creature->obj.ground) return false;

    const struct creature_s *npc = nw_character_hub_npc();
    if (!npc || npc->obj.invisible || npc->obj.character == creature->obj.character) return false;
    const float x = creature->obj.pos.x - npc->obj.pos.x;
    const float y = creature->obj.pos.y - npc->obj.pos.y;
    const float z = creature->obj.pos.z - npc->obj.pos.z;
    return x * x + z * z <= 1.0f * 1.0f && SDL_fabsf(y) <= 0.85f;
}

struct nupad_s *nw_character_select_pad(struct creature_s *creature, struct nupad_s *pad)
{
    if (creature != player || !pad) return pad;
    if (!(pad->paddata & NW_PAD_SQUARE)) nw_switch_consumed = false;
    if (!nw_switch_consumed && (pad->paddata_db & NW_PAD_SQUARE) &&
        nw_can_switch_at_computer(creature)) {
        nw_character_apply_choice(creature, 1 - creature->obj.character);
        creature->spin = 1;
        creature->spin_frame = 0;
        creature->spin_frames = creature->minfo->SPINFRAMES + creature->minfo->SPINRESETFRAMES;
        creature->spin_wait = 0;
        nw_switch_consumed = true;
        GameSfx(creature->obj.character == 1 ? NW_COCO_SPIN_SOUND : NW_CRASH_SPIN_SOUND, &creature->obj.pos);
    }
    if (!nw_switch_consumed) return pad;
    static struct nupad_s adjusted;
    adjusted = *pad;
    adjusted.paddata &= ~NW_PAD_SQUARE;
    adjusted.paddata_db &= ~NW_PAD_SQUARE;
    return &adjusted;
}

void nw_character_select_draw(void)
{
    if (!pCam || !nw_can_switch_at_computer(player)) return;
    const struct creature_s *npc = nw_character_hub_npc();
    const struct numtx_s *camera = &pCam->m;
    const float dx = npc->obj.pos.x - camera->_30;
    const float dy = npc->obj.pos.y + npc->obj.max.y * npc->obj.SCALE + 0.25f - camera->_31;
    const float dz = npc->obj.pos.z - camera->_32;
    const float x = dx * camera->_00 + dy * camera->_01 + dz * camera->_02;
    const float y = dx * camera->_10 + dy * camera->_11 + dz * camera->_12;
    const float z = dx * camera->_20 + dy * camera->_21 + dz * camera->_22;
    if (z < 1.0f) return;
    const float scale = SDL_min(2.0f, z * 0.23f);
    Text3D("PRESS s TO SWITCH", x / PANEL3DMULX, y / PANEL3DMULY, z,
           scale, scale, scale, 0, 0);
}

int nw_character_select_status(void)
{
    return SDL_GetAtomicInt(&nw_character_status);
}

void nw_character_select_request(int character)
{
    if ((unsigned)character <= 1) {
        SDL_SetAtomicInt(&nw_character_request, character + 1);
    }
}
