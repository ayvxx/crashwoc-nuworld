#ifndef NUWORLD_GAMEPLAY_TIMETRIAL_H
#define NUWORLD_GAMEPLAY_TIMETRIAL_H

#include <stdbool.h>

struct obj_s;

void nw_time_trial_started(void);
/* Retail's UpdateWumpa: every item it hits there is hit by flying fruit. */
void nw_time_trial_fruit(bool flying);
bool nw_time_trial_skip_pickup(const struct obj_s *item);

#endif
