/*
  SDL_mixer:  An audio mixer library based on the SDL library
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/* This file supports libOPNMIDI music streams */

#include "music_midi_opn.h"

#ifdef MUSIC_MID_OPNMIDI
#include <opnmidi.h>
#include "OPNMIDI/gm_opn_bank.h"
#include <stdio.h>

/* Global OPNMIDI flags which are applying on initializing of MIDI player with a file */
typedef struct {
    int volume_model;
    int chips_count;
    int full_brightness_range;
    int soft_pan;
    int emulator;
    char custom_bank_path[2048];
    double tempo;
    double gain;
} OpnMidi_Setup;

#define OPNMIDI_DEFAULT_CHIPS_COUNT     6

static OpnMidi_Setup opnmidi_setup = {
    0, -1, 0, 1, -1, "", 1.0, 2.0
};

static void OPNMIDI_SetDefault(OpnMidi_Setup *setup)
{
    setup->volume_model = 0;
    setup->chips_count = -1;
    setup->full_brightness_range = 0;
    setup->soft_pan = 1;
    setup->emulator = -1;
    setup->custom_bank_path[0] = '\0';
    setup->tempo = 1.0;
    setup->gain = 2.0;
}

#endif


int SDLCALLCC Mix_OPNMIDI_getVolumeModel()
{
#ifdef MUSIC_MID_OPNMIDI
    return opnmidi_setup.volume_model;
#else
    return -1;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setVolumeModel(int vm)
{
#ifdef MUSIC_MID_OPNMIDI
    opnmidi_setup.volume_model = vm;
    if (vm < 0) {
        opnmidi_setup.volume_model = 0;
    }
#else
    (void)vm;
#endif
}

int SDLCALLCC Mix_OPNMIDI_getFullRangeBrightness()
{
#ifdef MUSIC_MID_OPNMIDI
    return opnmidi_setup.full_brightness_range;
#else
    return -1;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setFullRangeBrightness(int frb)
{
#ifdef MUSIC_MID_OPNMIDI
    opnmidi_setup.full_brightness_range = frb;
#else
    (void)frb;
#endif
}

int SDLCALLCC Mix_OPNMIDI_getFullPanStereo()
{
#ifdef MUSIC_MID_OPNMIDI
    return opnmidi_setup.soft_pan;
#else
    return -1;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setFullPanStereo(int fp)
{
#ifdef MUSIC_MID_OPNMIDI
    opnmidi_setup.soft_pan = fp;
#else
    (void)fp;
#endif
}

int SDLCALLCC Mix_OPNMIDI_getEmulator()
{
#ifdef MUSIC_MID_OPNMIDI
    return opnmidi_setup.emulator;
#else
    return -1;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setEmulator(int emu)
{
#ifdef MUSIC_MID_OPNMIDI
    opnmidi_setup.emulator = emu;
#else
    (void)emu;
#endif
}

int SDLCALLCC Mix_OPNMIDI_getChipsCount()
{
#ifdef MUSIC_MID_OPNMIDI
    return opnmidi_setup.chips_count;
#else
    return -1;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setChipsCount(int chips)
{
#ifdef MUSIC_MID_OPNMIDI
    opnmidi_setup.chips_count = chips;
#else
    (void)frb;
#endif
}

void SDLCALLCC Mix_OPNMIDI_setSetDefaults()
{
#ifdef MUSIC_MID_OPNMIDI
    OPNMIDI_SetDefault(&opnmidi_setup);
#endif
}

void SDLCALLCC Mix_OPNMIDI_setCustomBankFile(const char *bank_wonp_path)
{
#ifdef MUSIC_MID_OPNMIDI
    if (bank_wonp_path) {
        SDL_strlcpy(opnmidi_setup.custom_bank_path, bank_wonp_path, 2048);
    } else {
        opnmidi_setup.custom_bank_path[0] = '\0';
    }
#else
    (void)bank_wonp_path;
#endif
}

#ifdef MUSIC_MID_OPNMIDI

/* This structure supports OPNMIDI-based MIDI music streams */
typedef struct
{
    int play_count;
    struct OPN2_MIDIPlayer *opnmidi;
    int playing;
    int volume;
    double tempo;
    double gain;

    SDL_AudioStream *stream;
    void *buffer;
    size_t buffer_size;
    Mix_MusicMetaTags tags;
    struct OPNMIDI_AudioFormat sample_format;
} OpnMIDI_Music;


/* Set the volume for a OPNMIDI stream */
static void OPNMIDI_setvolume(void *music_p, int volume)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    music->volume = (int)SDL_floor(((double)(volume) * music->gain) + 0.5);
}

/* Get the volume for a OPNMIDI stream */
static int OPNMIDI_getvolume(void *music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    return (int)SDL_floor(((double)(music->volume) / music->gain) + 0.5);
}

static double str_to_float(const char *str)
{
    char str_buff[25];
    char float_buff[4];
    char *p;
    /* UGLY WORKAROUND: Replace dot with local character (for example, comma) */
    SDL_strlcpy(str_buff, str, 25);
    SDL_snprintf(float_buff, 4, "%.1f", 0.0);
    for (p = str_buff; (p = SDL_strchr(p, '.')); ++p) {
        *p = float_buff[1];
    }
    return SDL_strtod(str_buff, NULL);
}

static void process_args(const char *args, OpnMidi_Setup *setup)
{
#define ARG_BUFFER_SIZE    1024
    char arg[ARG_BUFFER_SIZE];
    char type = '-';
    size_t maxlen = 0;
    size_t i, j = 0;
    int value_opened = 0;
    if (args == NULL) {
        return;
    }
    maxlen = SDL_strlen(args);
    if (maxlen == 0) {
        return;
    }

    maxlen += 1;
    OPNMIDI_SetDefault(setup);

    for (i = 0; i < maxlen; i++) {
        char c = args[i];
        if (value_opened == 1) {
            if ((c == ';') || (c == '\0')) {
                int value;
                arg[j] = '\0';
                if (type != 'x') {
                    value = SDL_atoi(arg);
                }
                switch(type)
                {
                case 'c':
                    setup->chips_count = value;
                    break;
                case 'v':
                    setup->volume_model = value;
                    break;
                case 'l':
                    setup->volume_model = value;
                    break;
                case 'r':
                    setup->full_brightness_range = value;
                    break;
                case 'p':
                    setup->soft_pan = value;
                    break;
                case 'e':
                    setup->emulator = value;
                    break;
                case 't':
                    if (arg[0] == '=') {
                        setup->tempo = str_to_float(arg + 1);
                        if (setup->tempo <= 0.0) {
                            setup->tempo = 1.0;
                        }
                    }
                    break;
                case 'g':
                    if (arg[0] == '=') {
                        setup->gain = str_to_float(arg + 1);
                        if (setup->gain < 0.0) {
                            setup->gain = 1.0;
                        }
                    }
                    break;
                case 'x':
                    if (arg[0] == '=') {
                        SDL_strlcpy(setup->custom_bank_path, arg + 1, (ARG_BUFFER_SIZE - 1));
                    }
                    break;
                case '\0':
                    break;
                default:
                    break;
                }
                value_opened = 0;
            }
            arg[j++] = c;
        } else {
            if (c == '\0') {
                return;
            }
            type = c;
            value_opened = 1;
            j = 0;
        }
    }
#undef ARG_BUFFER_SIZE
}

static void OPNMIDI_delete(void *music_p);

static OpnMIDI_Music *OPNMIDI_LoadSongRW(SDL_RWops *src, const char *args)
{
    void *bytes = 0;
    long filesize = 0;
    int err = 0;
    Sint64 length = 0;
    size_t bytes_l;
    unsigned char byte[1];
    OpnMIDI_Music *music = NULL;
    OpnMidi_Setup setup = opnmidi_setup;
    unsigned short src_format = music_spec.format;

    if (src == NULL) {
        return NULL;
    }

    process_args(args, &setup);

    music = (OpnMIDI_Music *)SDL_calloc(1, sizeof(OpnMIDI_Music));

    music->tempo = setup.tempo;
    music->gain = setup.gain;

    switch (music_spec.format) {
    case AUDIO_U8:
        music->sample_format.type = OPNMIDI_SampleType_U8;
        music->sample_format.containerSize = sizeof(Uint8);
        music->sample_format.sampleOffset = sizeof(Uint8) * 2;
        break;
    case AUDIO_S8:
        music->sample_format.type = OPNMIDI_SampleType_S8;
        music->sample_format.containerSize = sizeof(Sint8);
        music->sample_format.sampleOffset = sizeof(Sint8) * 2;
        break;
    case AUDIO_S16:
        music->sample_format.type = OPNMIDI_SampleType_S16;
        music->sample_format.containerSize = sizeof(Sint16);
        music->sample_format.sampleOffset = sizeof(Sint16) * 2;
        break;
    case AUDIO_U16:
        music->sample_format.type = OPNMIDI_SampleType_U16;
        music->sample_format.containerSize = sizeof(Uint16);
        music->sample_format.sampleOffset = sizeof(Uint16) * 2;
        break;
    case AUDIO_S32:
        music->sample_format.type = OPNMIDI_SampleType_S32;
        music->sample_format.containerSize = sizeof(Sint32);
        music->sample_format.sampleOffset = sizeof(Sint32) * 2;
        break;
    case AUDIO_F32:
    default:
        music->sample_format.type = OPNMIDI_SampleType_F32;
        music->sample_format.containerSize = sizeof(float);
        music->sample_format.sampleOffset = sizeof(float) * 2;
        src_format = AUDIO_F32;
    }

    music->stream = SDL_NewAudioStream(src_format, 2, music_spec.freq,
                                       music_spec.format, music_spec.channels, music_spec.freq);

    if (!music->stream) {
        OPNMIDI_delete(music);
        return NULL;
    }

    music->buffer_size = music_spec.samples * music->sample_format.containerSize * 2/*channels*/ * music_spec.channels;
    music->buffer = SDL_malloc(music->buffer_size);
    if (!music->buffer) {
        SDL_OutOfMemory();
        OPNMIDI_delete(music);
        return NULL;
    }

    length = SDL_RWseek(src, 0, RW_SEEK_END);
    if (length < 0) {
        Mix_SetError("OPN2-MIDI: wrong file\n");
        OPNMIDI_delete(music);
        return NULL;
    }

    SDL_RWseek(src, 0, RW_SEEK_SET);
    bytes = SDL_malloc((size_t)length);
    if (!bytes) {
        SDL_OutOfMemory();
        OPNMIDI_delete(music);
        return NULL;
    }

    filesize = 0;
    while ((bytes_l = SDL_RWread(src, &byte, sizeof(Uint8), 1)) != 0) {
        ((unsigned char*)bytes)[filesize] = byte[0];
        filesize++;
    }

    if (filesize == 0) {
        Mix_SetError("OPN2-MIDI: wrong file\n");
        SDL_free(bytes);
        OPNMIDI_delete(music);
        return NULL;
    }

    music->opnmidi = opn2_init(music_spec.freq);
    if (!music->opnmidi) {
        SDL_OutOfMemory();
        OPNMIDI_delete(music);
        return NULL;
    }

    if (setup.custom_bank_path[0] != '\0') {
        err = opn2_openBankFile(music->opnmidi, (char*)setup.custom_bank_path);
    } else {
        err = opn2_openBankData(music->opnmidi, g_gm_opn2_bank, sizeof(g_gm_opn2_bank));
    }

    if ( err < 0 ) {
        Mix_SetError("OPN2-MIDI: %s", opn2_errorInfo(music->opnmidi));
        SDL_free(bytes);
        OPNMIDI_delete(music);
        return NULL;
    }

    if (setup.emulator >= 0) {
        opn2_switchEmulator(music->opnmidi, setup.emulator);
    }
    opn2_setVolumeRangeModel(music->opnmidi, setup.volume_model);
    opn2_setFullRangeBrightness(music->opnmidi, setup.full_brightness_range);
    opn2_setSoftPanEnabled(music->opnmidi, setup.soft_pan);
    opn2_setNumChips(music->opnmidi, (setup.chips_count >= 0) ? setup.chips_count : OPNMIDI_DEFAULT_CHIPS_COUNT);
    opn2_setTempo(music->opnmidi, music->tempo);

    err = opn2_openData( music->opnmidi, bytes, (unsigned long)filesize);
    SDL_free(bytes);

    if (err != 0) {
        Mix_SetError("OPN2-MIDI: %s", opn2_errorInfo(music->opnmidi));
        OPNMIDI_delete(music);
        return NULL;
    }

    music->volume                 = MIX_MAX_VOLUME;
    meta_tags_init(&music->tags);
    meta_tags_set(&music->tags, MIX_META_TITLE, opn2_metaMusicTitle(music->opnmidi));
    meta_tags_set(&music->tags, MIX_META_COPYRIGHT, opn2_metaMusicCopyright(music->opnmidi));
    return music;
}

/* Load OPNMIDI stream from an SDL_RWops object */
static void *OPNMIDI_new_RWex(struct SDL_RWops *src, int freesrc, const char *args)
{
    OpnMIDI_Music *adlmidiMusic;

    adlmidiMusic = OPNMIDI_LoadSongRW(src, args);
    if (!adlmidiMusic) {
        return NULL;
    }
    if (freesrc) {
        SDL_RWclose(src);
    }

    return adlmidiMusic;
}

static void *OPNMIDI_new_RW(struct SDL_RWops *src, int freesrc)
{
    return OPNMIDI_new_RWex(src, freesrc, NULL);
}

/* Start playback of a given Game Music Emulators stream */
static int OPNMIDI_play(void *music_p, int play_counts)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    opn2_positionRewind(music->opnmidi);
    music->play_count = play_counts;
    opn2_setLoopEnabled(music->opnmidi, (play_counts < 0));
    return 0;
}

static int OPNMIDI_playSome(void *context, void *data, int bytes, SDL_bool *done)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)context;
    int filled, gottenLen, amount;

    filled = SDL_AudioStreamGet(music->stream, data, bytes);
    if (filled != 0) {
        return filled;
    }

    if (!music->play_count) {
        /* All done */
        *done = SDL_TRUE;
        return 0;
    }

    /* Align bytes length to correctly capture a stereo input */
    if ((bytes % (int)music->sample_format.sampleOffset) != 0) {
        bytes += ((int)music->sample_format.sampleOffset - (bytes % (int)music->sample_format.sampleOffset));
    }

    gottenLen = opn2_playFormat(music->opnmidi, (bytes / (int)music->sample_format.containerSize),
                               (OPN2_UInt8*)music->buffer,
                               (OPN2_UInt8*)music->buffer + music->sample_format.containerSize,
                               &music->sample_format);

    if (gottenLen <= 0) {
        *done = SDL_TRUE;
        return 0;
    }

    amount = gottenLen * (int)music->sample_format.containerSize;
    if (amount > 0) {
        if (SDL_AudioStreamPut(music->stream, music->buffer, amount) < 0) {
            return -1;
        }
    } else {
        if (music->play_count == 1) {
            music->play_count = 0;
            SDL_AudioStreamFlush(music->stream);
        } else {
            int play_count = -1;
            if (music->play_count > 0) {
                play_count = (music->play_count - 1);
            }
            opn2_positionRewind(music->opnmidi);
            music->play_count = play_count;
        }
    }

    return 0;
}

/* Play some of a stream previously started with OPNMIDI_play() */
static int OPNMIDI_playAudio(void *music_p, void *stream, int len)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    return music_pcm_getaudio(music_p, stream, len, music->volume, OPNMIDI_playSome);
}

/* Close the given Game Music Emulators stream */
static void OPNMIDI_delete(void *music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    if (music) {
        meta_tags_clear(&music->tags);
        if (music->opnmidi) {
            opn2_close( music->opnmidi );
        }
        if (music->stream) {
            SDL_FreeAudioStream(music->stream);
        }
        if (music->buffer) {
            SDL_free(music->buffer);
        }
        SDL_free( music );
    }
}

static const char* OPNMIDI_GetMetaTag(void *context, Mix_MusicMetaTag tag_type)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)context;
    return meta_tags_get(&music->tags, tag_type);
}

/* Jump (seek) to a given position (time is in seconds) */
static int OPNMIDI_jump_to_time(void *music_p, double time)
{
    OpnMIDI_Music *music = (OpnMIDI_Music*)music_p;
    opn2_positionSeek(music->opnmidi, time);
    return 0;
}

static double OPNMIDI_currentPosition(void* music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    return opn2_positionTell(music->opnmidi);
}


static double OPNMIDI_songLength(void* music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    return opn2_totalTimeLength(music->opnmidi);
}

static int OPNMIDI_setTempo(void *music_p, double tempo)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    if (music && (tempo > 0.0)) {
        opn2_setTempo(music->opnmidi, tempo);
        music->tempo = tempo;
        return 0;
    }
    return -1;
}

static double OPNMIDI_getTempo(void *music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    if (music) {
        return music->tempo;
    }
    return -1.0;
}

static double OPNMIDI_loopStart(void* music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    return opn2_loopStartTime(music->opnmidi);
}

static double OPNMIDI_loopEnd(void* music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    return opn2_loopEndTime(music->opnmidi);
}

static double OPNMIDI_loopLength(void* music_p)
{
    OpnMIDI_Music *music = (OpnMIDI_Music *)music_p;
    if (music) {
        double start = opn2_loopStartTime(music->opnmidi);
        double end = opn2_loopEndTime(music->opnmidi);
        if (start >= 0 && end >= 0) {
            return (end - start);
        }
    }
    return -1;
}


Mix_MusicInterface Mix_MusicInterface_OPNMIDI =
{
    "OPNMIDI",
    MIX_MUSIC_OPNMIDI,
    MUS_MID,
    SDL_FALSE,
    SDL_FALSE,

    NULL,   /* Load */
    NULL,   /* Open */
    OPNMIDI_new_RW,
    OPNMIDI_new_RWex,/* CreateFromRWex [MIXER-X]*/
    NULL,   /* CreateFromFile */
    NULL,   /* CreateFromFileEx [MIXER-X]*/
    OPNMIDI_setvolume,
    OPNMIDI_getvolume,   /* GetVolume [MIXER-X]*/
    OPNMIDI_play,
    NULL,   /* IsPlaying */
    OPNMIDI_playAudio,
    OPNMIDI_jump_to_time,
    OPNMIDI_currentPosition,   /* Tell [MIXER-X]*/
    OPNMIDI_songLength,   /* FullLength [MIXER-X]*/
    OPNMIDI_setTempo,   /* Set Tempo multiplier [MIXER-X] */
    OPNMIDI_getTempo,   /* Get Tempo multiplier [MIXER-X] */
    OPNMIDI_loopStart,   /* LoopStart [MIXER-X]*/
    OPNMIDI_loopEnd,   /* LoopEnd [MIXER-X]*/
    OPNMIDI_loopLength,   /* LoopLength [MIXER-X]*/
    OPNMIDI_GetMetaTag,   /* GetMetaTag [MIXER-X]*/
    NULL,   /* Pause */
    NULL,   /* Resume */
    NULL,   /* Stop */
    OPNMIDI_delete,
    NULL,   /* Close */
    NULL,   /* Unload */
};

#endif /* MUSIC_MID_OPNMIDI */

