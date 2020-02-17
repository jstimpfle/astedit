/* shit implementation, and only on linux for now. */
#include <astedit/astedit.h>
#include <astedit/logging.h>
#include <astedit/memory.h>
#include <astedit/sound.h>
#include <alsa/asoundlib.h>
#include <alsa/asoundlib.h>
#include <math.h>

static snd_pcm_t *pcm_handle;
static int sndrate;
static int sndchannels;
static int sndformat;  // bits per sample per channel
static int sndaccess;
static int sndperiodsize;
static int sndperiods;

static void _fatal_alsa_error(struct LogInfo ctx, int error, const char *msg)
{
	_fatalf(ctx, "Error from ALSA when %s: %s", msg, snd_strerror(error));
}

static void _report_alsa_error(struct LogInfo ctx, int error, const char *msg)
{
        _log_postf(ctx, "Error from ALSA when %s: %s", msg, snd_strerror(error));
}

#define report_alsa_error(error, msg) _report_alsa_error(MAKE_LOGINFO(), (error), (msg))
#define fatal_alsa_error(error, msg) _fatal_alsa_error(MAKE_LOGINFO(), (error), (msg))

void setup_sound(void)
{
        int err;

        err = snd_pcm_open(&pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0)
		fatal_alsa_error(err, "calling snd_pcm_device_open() for playback");

        {
                snd_pcm_hw_params_t *hw_params;

                err = snd_pcm_hw_params_malloc(&hw_params);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_malloc()");

		static const int possible_rates[] = { 48000, 44100 };
		static const int possible_channels[] = { 2 };
		static const int possible_formats[] = { SND_PCM_FORMAT_S16_LE };
		static const int possible_accesses[] = { SND_PCM_ACCESS_RW_INTERLEAVED }; // TODO: we want to support INTERLEAVED and NONINTERLEAVED

		for (int i = 0; i < LENGTH(possible_rates); i++) {
			err = snd_pcm_hw_params_any(pcm_handle, hw_params) < 0;
			if (err < 0)
				fatal_alsa_error(err, "Failed to snd_pcm_hw_params_any()");
			err = snd_pcm_hw_params_set_rate(pcm_handle, hw_params, possible_rates[i], 0);
			if (err < 0)
				continue;
			for (int j = 0; j < LENGTH(possible_channels); j++) {
				err = snd_pcm_hw_params_set_channels(pcm_handle, hw_params, possible_channels[j]);
				if (err < 0)
					continue;
				for (int k = 0; k < LENGTH(possible_formats); k++) {
					err = snd_pcm_hw_params_set_format(pcm_handle, hw_params, possible_formats[k]);
					if (err < 0)
						continue;
					for (int l = 0; l < LENGTH(possible_accesses); l++) {
						err = snd_pcm_hw_params_set_access(pcm_handle, hw_params, possible_accesses[l]);
						if (err < 0)
							continue;

						snd_pcm_uframes_t minperiodsize = 512;
						snd_pcm_uframes_t maxperiodsize = 2048;
						int mindir;
						int maxdir;
						err = snd_pcm_hw_params_set_period_size_minmax(pcm_handle, hw_params,
								&minperiodsize, &mindir,
								&maxperiodsize, &maxdir);
						if (err < 0)
							continue;

						goto found;
					}
				}
			}
		}
		fatalf("Failed to find a valid sound card configuration");

found:
                err = snd_pcm_hw_params(pcm_handle, hw_params);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params() with the configuration we found");

		int dir;
		unsigned int cfgrate;
		unsigned int cfgchannels;
		snd_pcm_format_t cfgformat;
		snd_pcm_access_t cfgaccess;
		snd_pcm_uframes_t cfgperiodsize;
		unsigned cfgperiods;

		err = snd_pcm_hw_params_get_rate(hw_params, &cfgrate, &dir);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_rate()");

		err = snd_pcm_hw_params_get_channels(hw_params, &cfgchannels);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_channels()");

		err = snd_pcm_hw_params_get_format(hw_params, &cfgformat);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_format()");

		err = snd_pcm_hw_params_get_access(hw_params, &cfgaccess);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_access()");

		err = snd_pcm_hw_params_get_period_size(hw_params, &cfgperiodsize, &dir);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_periods_size()");

		err = snd_pcm_hw_params_get_periods(hw_params, &cfgperiods, &dir);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_hw_params_get_periods()");

		sndrate = cfgrate;
		sndchannels = cfgchannels;
		sndformat = cfgformat;
		sndaccess = cfgaccess;
		sndperiodsize = cfgperiodsize;
		sndperiods = cfgperiods;

		log_postf("Sound configured. rate=%d channels=%d, periodsize: %d, periods: %d",
				sndrate, sndchannels, sndperiodsize, sndperiods);



		/*
                err = snd_pcm_hw_params_set_period_size(pcm_handle, hw_params, num_samples_per_period, 0);
                err = snd_pcm_hw_params_set_buffer_size(pcm_handle, hw_params, num_periods_in_buffer * num_samples_per_period);
		*/

                snd_pcm_hw_params_free(hw_params);
        }

        {
                snd_pcm_sw_params_t *sw_params;

                err = snd_pcm_sw_params_malloc(&sw_params);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_sw_params_malloc()");

                err = snd_pcm_sw_params_current(pcm_handle, sw_params);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_sw_params_current()");

                err = snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, sndperiodsize);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_sw_params_set_start_threshold()");

                err = snd_pcm_sw_params(pcm_handle, sw_params);
		if (err < 0)
			fatal_alsa_error(err, "calling snd_pcm_sw_params()");

                snd_pcm_sw_params_free(sw_params);
        }
}

void teardown_sound(void)
{
        /* Documented in file MEMORY LEAK in alsa-lib package */
        snd_config_update_free_global();

        int err = snd_pcm_close(pcm_handle);
	if (err < 0) {
		report_alsa_error(err, "calling snd_pcm_close()");
		log_postf("WARNING: failed to close ALSA pcm handle");
	}

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
				if (err < 0) {
					report_alsa_error(err, "recovering from error");
					log_postf("TODO: Should we be prepared to handle this at all or rather terminate?");
				}
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
        if (err < 0)
		report_alsa_error(err, "snd_pcm_drain()");
        */
        int err = snd_pcm_prepare(pcm_handle);
	if (err < 0)
		report_alsa_error(err, "snd_pcm_prepare()");
}

void end_playing_a_sound()
{
        /* currently nothing */
}
