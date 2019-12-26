#include <stdint.h>

/* platform-specific */
void setup_sound(void);
void teardown_sound(void);
void write_sound(uint16_t *samples, int nsamples);
void update_available_samples(void);
int get_available_samples(void);
void write_samples(const uint16_t *samples, int numSamplesPerChannel);
/* try at an API to avoid underruns */
void begin_playing_a_sound();
void end_playing_a_sound();

void continue_any_currently_playing_sounds(void);
void play_navigation_impossible_sound(void);
