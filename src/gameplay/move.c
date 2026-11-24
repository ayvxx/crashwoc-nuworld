#include <stdbool.h>

#include "game/creature.h"
#include "game/gametypes.h"
#include "nu_port.h"
#include "nups2/ps2ctl.h"
#include "game/move.retail.h"
#include "gameplay/air.h"
#include "gameplay/fixes.h"
#include "gameplay/ground.h"
#include "gameplay/mech.h"
#include "gameplay/monkeybars.h"
#include "gameplay/slidespin.h"
#include "gameplay/submarine.h"

void MoveCRASH(struct creature_s *plr, struct nupad_s *pad)
{
    nw_ground_before(plr, pad);
    nw_air_before(plr, pad);
    nw_slide_spin_before(plr, pad);
    nw_fix_ground_slam(plr);
    MoveCRASH__retail(plr, pad);
    nw_air_move(plr, pad);
    nw_ground_after(plr);
    nw_slide_spin_after(plr);
}

extern s32 VEHICLECONTROL, fadeval;
extern struct cursor_s Cursor;
extern float vtog_time, vtog_duration;
extern struct MoveInfo CocoMoveInfo, CrashMoveInfo;
extern struct gtimer_s GameTimer;
void GameSfx(s32 effect, struct nuvec_s *pos);
void GetTopBot(struct creature_s *plr);

static bool nw_coco_slide_to_crawl;

void MoveCOCO(struct creature_s *plr, struct nupad_s *pad)
{
    const bool on_foot = plr == player && VEHICLECONTROL == 0 && vtog_time == vtog_duration;
    const bool ready = on_foot && !plr->obj.dead && !plr->obj.finished &&
                       !plr->obj.transporting && !plr->freeze && fadeval <= 0 &&
                       Cursor.menu != 0x1f && Cursor.menu != 0x21 &&
                       GameTimer.frame >= NW_HZ(50, 60);
    const bool grounded = plr->obj.ground != 0;
    const bool crouch = ready && pad && (pad->paddata & 0x20);
    const bool jump = ready && pad && (pad->paddata_db & 0x40);
    const short previous_slide = plr->slide;
    const bool cancel_slide = jump && grounded && !plr->jump && previous_slide > 0;

    if (!crouch || !grounded || plr->jump || plr->obj.dangle) {
        nw_coco_slide_to_crawl = false;
    }
    if (plr == player && plr->obj.dangle) {
        MoveCRASH__retail(plr, pad);
        return;
    }

    nw_air_before(plr, pad);
    nw_fix_ground_slam(plr);
    const bool double_jump = jump && !grounded && plr->jump == 3 && !plr->slam &&
                             !plr->slam_wait && !plr->slide;
    if (double_jump) {
        plr->jump = 4;
        plr->jump_frames = CrashMoveInfo.JUMPFRAMES0;
        plr->jump_frame = 0;
        plr->somersault = 0;
        plr->obj.anim.anim_time = 1.0f;
        GameSfx(0x2f, &plr->obj.pos);
    }
    if (cancel_slide) {
        plr->slide = 0;
    }
    const float standing_height = (plr->obj.max.y - plr->obj.min.y) * plr->obj.SCALE;
    if (ready && !crouch && grounded && plr->obj.pad_speed > 0.0f &&
        plr->obj.clearance > standing_height) {
        plr->crawl = 0;
        plr->crouch_pos = 0;
    }
    if (ready && !cancel_slide && !plr->slide &&
        (plr->crawl || plr->crouch_pos || (crouch && grounded && !plr->jump))) {
        const short crouch_frames = CrashMoveInfo.CROUCHINGFRAMES;
        const short slide_frames = CrashMoveInfo.SLIDEFRAMES;
        CrashMoveInfo.CROUCHINGFRAMES = CocoMoveInfo.CROUCHINGFRAMES;
        CrashMoveInfo.SLIDEFRAMES = CocoMoveInfo.SLIDEFRAMES;
        MoveCRASH__retail(plr, pad);
        CrashMoveInfo.CROUCHINGFRAMES = crouch_frames;
        CrashMoveInfo.SLIDEFRAMES = slide_frames;
    } else {
        if (plr->jump == 4 && on_foot) {
            const short frames = CocoMoveInfo.JUMPFRAMES2;
            const float height = CocoMoveInfo.JUMPHEIGHT;
            CocoMoveInfo.JUMPFRAMES2 = CrashMoveInfo.JUMPFRAMES2;
            CocoMoveInfo.JUMPHEIGHT = CrashMoveInfo.JUMPHEIGHT;
            if (!double_jump && (pad->paddata & 0x40) &&
                plr->jump_frames < CrashMoveInfo.JUMPFRAMES2) {
                ++plr->jump_frames;
            }
            const short hold_frames = plr->jump_frames;
            if (double_jump) plr->jump_frames = 0;
            MoveCOCO__retail(plr, pad);
            if (double_jump) plr->jump_frames = hold_frames;
            CocoMoveInfo.JUMPFRAMES2 = frames;
            CocoMoveInfo.JUMPHEIGHT = height;
        } else {
            MoveCOCO__retail(plr, pad);
        }
    }
    if (double_jump) {
        plr->jump_frame = 0;
    } else if (plr->jump == 4 && (!(pad->paddata & 0x40) ||
                                  plr->jump_frames >= CrashMoveInfo.JUMPFRAMES2)) {
        plr->jump = 5;
    }
    if (cancel_slide && !plr->jump) {
        plr->slide = previous_slide - 1;
    }
    if (crouch && !cancel_slide && grounded && !plr->jump) {
        nw_coco_slide_to_crawl |= previous_slide > 0;
        if (nw_coco_slide_to_crawl && !plr->slide) {
            plr->crawl = 1;
            plr->crouch_pos = CocoMoveInfo.CROUCHINGFRAMES;
        }
    }
    if (on_foot && plr->slide) {
        plr->obj.attack |= 0x10;
        GetTopBot(plr);
    }
    nw_air_move(plr, pad);
}

void MoveMECH(struct creature_s *plr, struct nupad_s *pad)
{
    nw_mech_before(plr, pad);
    MoveMECH__retail(plr, pad);
    nw_mech_after(plr, pad);
}

void AnimateCRASH(struct creature_s *plr)
{
    AnimateCRASH__retail(plr);
    nw_monkey_bars_animate(plr);
    nw_ground_animate(plr);
}

void AnimateCOCO(struct creature_s *plr)
{
    if (plr == player && plr->obj.dangle) {
        AnimateCRASH__retail(plr);
    } else {
        AnimateCOCO__retail(plr);
    }
    nw_ground_animate(plr);
}

void MoveSUBMARINE(struct creature_s *plr, struct nupad_s *pad)
{
    nw_sub_turn(plr);
    nw_sub_fire_before(plr);
    MoveSUBMARINE__retail(plr, pad);
    nw_sub_fire_after(plr);
}
