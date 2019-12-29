/* shit implementation, and only on linux for now. */
#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/memoryalloc.h>
#include <astedit/sound.h>
#include <alsa/asoundlib.h>
#include <alsa/asoundlib.h>
#include <math.h>

enum {
        num_samples_per_second = 44100,
        num_samples_per_period = 512,
        num_periods_in_buffer = 3,
};

static snd_pcm_t *pcm_handle;

static void _report_alsa_error(struct LogInfo ctx, int error, const char *msg)
{
        _log_postf(ctx, "Error from ALSA when %s: %s", msg, snd_strerror(error));
}

#define report_alsa_error(error, msg) _report_alsa_error(MAKE_LOGINFO(), (error), (msg))

void setup_sound(void)
{
        int err;
#define CHECK(x) if (err < 0) { report_alsa_error(err, (x)); return; }

        err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
        CHECK("open pcm stream");

        {
                snd_pcm_hw_params_t *hw_params;

                err = snd_pcm_hw_params_malloc(&hw_params);
                CHECK("allocating hw_params structure");

                err = snd_pcm_hw_params_any(pcm_handle, hw_params);
                CHECK("snd_pcm_hw_params_any()");

                err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
                CHECK("setting hardware access mode");

                err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
                CHECK("choosing signed 16bit litte-endian samples");

                err = snd_pcm_hw_params_set_rate(pcm_handle, hw_params, num_samples_per_second, 0);
                CHECK("setting hardware sample rate");

                err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 2);
                CHECK("setting hardware to 2-channel mode");

                err = snd_pcm_hw_params_set_period_size(pcm_handle, hw_params, num_samples_per_period, 0);
                CHECK("setting hardware period size");

                err = snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, num_periods_in_buffer * num_samples_per_period);
                CHECK("setting hardware buffer size");

                err = snd_pcm_hw_params(pcm_handle, hw_params);
                CHECK("applying hardware configuration");

                snd_pcm_hw_params_free(hw_params);
        }

        {
                snd_pcm_sw_params_t *sw_params;

                err = snd_pcm_sw_params_malloc(&sw_params);
                CHECK("allocating sw_params structure");

                err = snd_pcm_sw_params_current(pcm_handle, sw_params);
                CHECK("fill sw_params structures with current values");

                err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, num_samples_per_period);
                CHECK("set software start threshold to period size");

                err = snd_pcm_sw_params(pcm_handle, sw_params);
                CHECK("applying software configuration");

                snd_pcm_sw_params_free(sw_params);
        }


//#undef CHECK
}

void teardown_sound(void)
{
        int err = snd_pcm_close(pcm_handle);
        CHECK("closing pcm");

        /* Documented in file MEMORY LEAK in alsa-lib package */
        snd_config_update_free_global();
}

static const char *alsa_state_to_string(snd_pcm_state_t state)
{
        switch(state) {
        case SND_PCM_STATE_DISCONNECTED: return "DISCONNECTED";
        case SND_PCM_STATE_DRAINING: return "DRAINING";
        case SND_PCM_STATE_OPEN: return "OPEN";
        case SND_PCM_STATE_PAUSED: return "PAUSED";
        case SND_PCM_STATE_PREPARED: return "PREPARED";
        case SND_PCM_STATE_RUNNING: return "RUNNING";
        case SND_PCM_STATE_SETUP: return "SETUP";
        case SND_PCM_STATE_SUSPENDED: return "SUSPENDED";
        case SND_PCM_STATE_XRUN: return "XRUN";
        default: return "(unknown)";
        }
}

int numAvailableSamples;

void update_available_samples(void)
{
        int err;
        int avail;

tryagain:
        err = avail = snd_pcm_avail(pcm_handle);
        if (err < 0) {
                report_alsa_error(err, "snd_pcm_avail()");
                err = snd_pcm_recover(pcm_handle, err, 0); // XXX this prints a message to stderr. We might want to avoid that
                if (err < 0) {
                        report_alsa_error(err, "snd_pcm_recover()");
                        return;
                }
                goto tryagain;
        }
        numAvailableSamples = avail;
}

int get_available_samples(void)
{
        return numAvailableSamples;
}

void write_samples(const uint16_t *samples, int nsamples)
{
        ENSURE(nsamples <= numAvailableSamples);

        for (;;) {
                int err = snd_pcm_writei(pcm_handle, samples, nsamples);
                if (err < 0) {
                        log_postf("Trying to handle error after writei");
                        ENSURE(err != -EBADFD);
                        if (err == -EPIPE || err == -ESTRPIPE) {
                                err = snd_pcm_recover(pcm_handle, err, 0);
                                CHECK("recovering from error");
                                log_postf("state is now: %s", alsa_state_to_string(snd_pcm_state(pcm_handle)));
                        }
                        else {
                                fatalf("Unknown alsa error after writei");
                        }
                }
                else {
                        ENSURE(err == nsamples);  // not sure if that's true
                        break;
                }
        }
}

void begin_playing_a_sound()
{
        /*
        int err = snd_pcm_drain(pcm_handle);
        CHECK("snd_pcm_drain()");
        */
        int err = snd_pcm_prepare(pcm_handle);
        CHECK("snd_pcm_prepare()");
}

void end_playing_a_sound()
{
        /* currently nothing */
}
