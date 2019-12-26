#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/sound.h>
#include <math.h>

#define BEEP_SAMPLES 5000  // at 44100 khz
#define RAMP_UP_SAMPLES 30
#define RAMP_DOWN_SAMPLES 200

static int sndstate_playing;
static int sndstate_sampleno;
static float sndstate_x;
static float sndstate_y;

static uint16_t samples[4096][2];

void continue_any_currently_playing_sounds(void)
{
        if (!sndstate_playing)
                return;

        update_available_samples();
        int avail = get_available_samples();

        int nToWrite = LENGTH(samples);
        if (nToWrite > avail)
                nToWrite = avail;
        if (nToWrite > BEEP_SAMPLES - sndstate_sampleno)
                nToWrite = BEEP_SAMPLES - sndstate_sampleno;

        for (int i = 0; i < nToWrite; i++) {
                float freq = 400.0f;
                float factor;
                if (BEEP_SAMPLES - sndstate_sampleno < RAMP_DOWN_SAMPLES)
                        factor = (BEEP_SAMPLES - sndstate_sampleno) / (float) RAMP_DOWN_SAMPLES;
                else if (sndstate_sampleno < RAMP_UP_SAMPLES)
                        factor = sndstate_sampleno / (float) RAMP_UP_SAMPLES;
                else
                        factor = 1.0f;
                samples[i][0] = samples[i][1] = factor * 20000.0f * sndstate_y;
                float a = sndstate_x;
                float b = sndstate_y;
                float c = cosf(freq * 0.00014247585f); //TODO constify
                float d = sinf(freq * 0.00014247585f); //TODO constify
                sndstate_x = a * c - b * d;
                sndstate_y = a * d + b * c;
                /* normalize to length 1.0f */
                {
                float len = sndstate_x * sndstate_x + sndstate_y * sndstate_y;
                sndstate_x /= len;
                sndstate_y /= len;
                }
                sndstate_sampleno ++;
        }

        write_samples(&samples[0][0], nToWrite);

        if (sndstate_sampleno == BEEP_SAMPLES) {
                sndstate_playing = 0;
                end_playing_a_sound();
        }
}

void play_navigation_impossible_sound(void)
{
        sndstate_playing = 1;
        sndstate_sampleno = 0;
        sndstate_x = 1.0f;
        sndstate_y = 0.0f;
        begin_playing_a_sound();
}
