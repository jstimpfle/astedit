#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/sound.h>
#include <math.h>

#define BEEP_SAMPLES 5000  // at 44100 khz
#define RAMP_UP_SAMPLES 30
#define RAMP_DOWN_SAMPLES 200

#define BEEP_FREQ 400
static int sndstate_playing;
static int sndstate_sampleno;
static int sndstate_index;

static uint16_t samples[BEEP_SAMPLES][2];

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

        while (nToWrite > LENGTH(samples) - sndstate_index) {
                write_samples(&samples[sndstate_index][0],
                              LENGTH(samples) - sndstate_sampleno);
                nToWrite -= LENGTH(samples) - sndstate_sampleno;
                sndstate_index = 0;
        }
        write_samples(&samples[sndstate_index][0], nToWrite);
        sndstate_index += nToWrite;
        sndstate_sampleno += nToWrite;

        if (sndstate_sampleno == BEEP_SAMPLES) {
                sndstate_playing = 0;
                end_playing_a_sound();
                return;
        }
        //XXX: we probably need a separate thread to feed the sound card...
        continue_any_currently_playing_sounds();
}

static void precompute_samples()
{
        float sndstate_x = 1.0f;
        float sndstate_y = 0.0f;
        float freq = (float) BEEP_FREQ;
        float volume = 10000.f;
        for (int i = 0; i < BEEP_SAMPLES; i++) {
                float factor;
                if (BEEP_SAMPLES - i < RAMP_DOWN_SAMPLES)
                        factor = (BEEP_SAMPLES - i) / (float) RAMP_DOWN_SAMPLES;
                else if (i < RAMP_UP_SAMPLES)
                        factor = sndstate_sampleno / (float) RAMP_UP_SAMPLES;
                else
                        factor = 1.0f;
                float amplitude = factor * volume;
                samples[i][0] = samples[i][1] = (uint16_t) (amplitude * sndstate_y);
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
        }
}

void play_navigation_impossible_sound(void)
{
        precompute_samples();
        sndstate_playing = 1;
        sndstate_sampleno = 0;
        sndstate_index = 0;
        begin_playing_a_sound();
}
