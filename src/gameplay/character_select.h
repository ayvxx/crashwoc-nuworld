#ifndef NW_GAMEPLAY_CHARACTER_SELECT_H
#define NW_GAMEPLAY_CHARACTER_SELECT_H

struct creature_s;
struct nupad_s;

void nw_character_select_load_choice(const char *root);
struct nupad_s *nw_character_select_pad(struct creature_s *creature, struct nupad_s *pad);
void nw_character_select_draw(void);

int nw_character_select_status(void);
void nw_character_select_request(int character);
void nw_character_select_tick(struct creature_s *creature);
void nw_character_select_load_models(void);
void nw_character_select_restore(void);

#endif
