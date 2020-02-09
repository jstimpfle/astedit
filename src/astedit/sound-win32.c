/* shit implementation, and only on linux for now. */
#include <astedit/astedit.h>
#include <astedit/sound.h>

void setup_sound(void)
{
}

void teardown_sound(void)
{
}

void update_available_samples(void)
{
}

int get_available_samples(void)
{
        // as long as there is no sound implementation, report
        // a number > 0 as the number of available bytes. This
        // is so that writers don't get stuck re-trying.
        return 4096;
}

void write_samples(const uint16_t *samples, int nsamples)
{
        UNUSED(samples);
        UNUSED(nsamples);
}

void begin_playing_a_sound()
{
}

void end_playing_a_sound()
{

}
