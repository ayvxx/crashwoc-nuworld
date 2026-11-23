#include <stddef.h>
#include "gameplay/timetrial.h"

#include "game/creature.h"
#include "game/gametypes.h"
#include "gameplay/gameplay.h"

/* Declared by no header. */
extern struct ldata_s *LDATA;
void AddMaskFeathers(MaskInfo *mask);

#define NW_CLOCK 0x76 /* PickupItem's time trial clock */

static bool nw_time_trial_flying;

void nw_time_trial_started(void)
{
    MaskInfo *mask = player ? player->obj.mask : NULL;
    if (!nw_profile.movement.time_trial.no_masks || !mask || !mask->active || (LDATA->flags & 0xE00)) {
        return;
    }
    NW_DEVIATION(movement.time_trial.no_masks);
    AddMaskFeathers(mask);
    mask->active = 0;
}

void nw_time_trial_fruit(bool flying)
{
    nw_time_trial_flying = flying;
}

bool nw_time_trial_skip_pickup(const struct obj_s *item)
{
    if (!nw_time_trial_flying || item->character != NW_CLOCK || !nw_profile.movement.time_trial.no_fruit_clock) {
        return false;
    }
    NW_DEVIATION(movement.time_trial.no_fruit_clock);
    return true;
}
