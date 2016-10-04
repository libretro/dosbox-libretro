/*
 *  Copyright (C) 2002-2003  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fluidsynth.h>

/* Missing fluidsynth API follow: */
extern "C" {
  typedef struct _fluid_midi_parser_t fluid_midi_parser_t;

  fluid_midi_parser_t *new_fluid_midi_parser(void);
  fluid_midi_event_t *
  fluid_midi_parser_parse(fluid_midi_parser_t *parser, unsigned char c);
  int delete_fluid_midi_parser(fluid_midi_parser_t* parser);

  void fluid_log_config(void);
}

/* Protect against multiple inclusions */
#ifndef MIXER_BUFSIZE
#include "mixer.h"
#endif

static MixerChannel *synthchan;

static fluid_synth_t *synth_soft;
static fluid_midi_parser_t *synth_parser;

static int synthsamplerate;


static void synth_log(int level, char *message, void *data) {
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		LOG(LOG_ALL,LOG_ERROR)(message);
		break;

	case FLUID_WARN:
		LOG(LOG_ALL,LOG_WARN)(message);
		break;

	default:
		LOG(LOG_ALL,LOG_NORMAL)(message);
		break;
	}
}

static void synth_CallBack(Bitu len) {
	fluid_synth_write_s16(synth_soft, len, MixTemp, 0, 2, MixTemp, 1, 2);
	synthchan->AddSamples_s16(len,(Bit16s *)MixTemp);
}

class MidiHandler_synth: public MidiHandler {
private:
	fluid_settings_t *settings;
	fluid_midi_router_t *router;
	int sfont_id;
	bool isOpen;

public:
	MidiHandler_synth() : isOpen(false),MidiHandler() {};
	char * GetName(void) { return "synth"; };
	bool Open(const char *conf) {

		/* Sound font file required */
		if (!conf || (conf[0] == '\0')) {
			LOG_MSG("SYNTH: Specify .SF2 sound font file with config=");
			return false;
		}

		fluid_log_config();
		fluid_set_log_function(FLUID_PANIC, synth_log, NULL);
		fluid_set_log_function(FLUID_ERR, synth_log, NULL);
		fluid_set_log_function(FLUID_WARN, synth_log, NULL);
		fluid_set_log_function(FLUID_INFO, synth_log, NULL);
		fluid_set_log_function(FLUID_DBG, synth_log, NULL);

		/* Create the settings. */
		settings = new_fluid_settings();
		if (settings == NULL) {
			LOG_MSG("SYNTH: Error allocating MIDI soft synth settings");
			return false;
		}

		fluid_settings_setstr(settings, "audio.sample-format", "16bits");

		if (synthsamplerate == 0) {
			synthsamplerate = 44100;
		}

		fluid_settings_setnum(settings,
			"synth.sample-rate", (double)synthsamplerate);

		fluid_settings_setnum(settings, "synth.gain", 1.0);

		/* Create the synthesizer. */
		synth_soft = new_fluid_synth(settings);
		if (synth_soft == NULL) {
			LOG_MSG("SYNTH: Error initialising MIDI soft synth");
			delete_fluid_settings(settings);
			return false;
		}

		/* Load a SoundFont */
		sfont_id = fluid_synth_sfload(synth_soft, conf, 0);
		if (sfont_id == -1) {
			LOG_MSG("SYNTH: Failed to load MIDI sound font file \"%s\"",
			   conf);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		/* Allocate one event to store the input data */
		synth_parser = new_fluid_midi_parser();
		if (synth_parser == NULL) {
			LOG_MSG("SYNTH: Failed to allocate MIDI parser");
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		router = new_fluid_midi_router(settings,
					       fluid_synth_handle_midi_event,
					       (void*)synth_soft);
		if (router == NULL) {
			LOG_MSG("SYNTH: Failed to initialise MIDI router");
			delete_fluid_midi_parser(synth_parser);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		synthchan=MIXER_AddChannel(synth_CallBack, synthsamplerate,
					   "SYNTH");
		synthchan->Enable(false);
		isOpen = true;
		return true;
	};
	void Close(void) {
		if (!isOpen) return;
		delete_fluid_midi_router(router);
		delete_fluid_midi_parser(synth_parser);
		delete_fluid_synth(synth_soft);
		delete_fluid_settings(settings);
		isOpen=false;
	};
	void PlayMsg(Bit8u *msg) {
		fluid_midi_event_t *evt;
		Bitu len;
		int i;

		len=MIDI_evt_len[*msg];
		synthchan->Enable(true);

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, msg[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
	void PlaySysex(Bit8u *sysex, Bitu len) {
		fluid_midi_event_t *evt;
		int i;

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, sysex[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
};

MidiHandler_synth Midi_synth;
