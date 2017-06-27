#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"
#include "alhelpers.h"
#include "apu.h"
#if WINDOWS_BUILD
#include <windows.h>
#else
#include <unistd.h>
#endif

static LPALBUFFERSAMPLESSOFT alBufferSamplesSOFT = wrap_BufferSamples;
static LPALISBUFFERFORMATSUPPORTEDSOFT alIsBufferFormatSupportedSOFT;

typedef struct StreamPlayer {
    /* These are the buffers and source to play out through OpenAL with */
    ALuint buffers[NUM_BUFFERS];
    ALuint source;

    /* Handle for the audio file */
    //FilePtr file;

    /* The format of the output stream */
    ALenum format;
    ALenum channels;
    ALenum type;
    ALuint rate;
} StreamPlayer;

static StreamPlayer *NewPlayer(void);
static void DeletePlayer(StreamPlayer *player);

/* Creates a new player object, and allocates the needed OpenAL source and
 * buffer objects. Error checking is simplified for the purposes of this
 * example, and will cause an abort if needed. */
static StreamPlayer *NewPlayer(void)
{
    StreamPlayer *player;

    player = malloc(sizeof(*player));
    //assert(player != NULL);

    memset(player, 0, sizeof(*player));

    /* Generate the buffers and source */
    alGenBuffers(NUM_BUFFERS, player->buffers);
    //assert(alGetError() == AL_NO_ERROR && "Could not create buffers");

    alGenSources(1, &player->source);
    //assert(alGetError() == AL_NO_ERROR && "Could not create source");

    /* Set parameters so mono sources play out the front-center speaker and
     * won't distance attenuate. */
    alSource3i(player->source, AL_POSITION, 0, 0, -1);
    alSourcei(player->source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcei(player->source, AL_ROLLOFF_FACTOR, 0);
    //assert(alGetError() == AL_NO_ERROR && "Could not set source parameters");

    return player;
}

/* Destroys a player object, deleting the source and buffers. No error handling
 * since these calls shouldn't fail with a properly-made player object. */
static void DeletePlayer(StreamPlayer *player)
{
   // ClosePlayerFile(player);

    alDeleteSources(1, &player->source);
    alDeleteBuffers(NUM_BUFFERS, player->buffers);
    if(alGetError() != AL_NO_ERROR)
        fprintf(stderr, "Failed to delete object IDs\n");

    memset(player, 0, sizeof(*player));
    free(player);
}

static int StartPlayer(StreamPlayer *player)
{
    size_t i;

    /* Rewind the source position and clear the buffer queue */
    alSourceRewind(player->source);
    alSourcei(player->source, AL_BUFFER, 0);

    /* Fill the buffer queue with empty data */
    for(i = 0;i < NUM_BUFFERS;i++)
    {
        uint8_t *data;

        /* Get some data to give it to the buffer */
        data = apuGetBuf();
        if(!data) break;

        alBufferSamplesSOFT(player->buffers[i], player->rate, player->format,
                            BytesToFrames(apuGetBufSize(), player->channels, player->type),
                            player->channels, player->type, data);
    }
    if(alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "Error buffering for playback\n");
        return 0;
    }

    /* Now queue and start playback! */
    alSourceQueueBuffers(player->source, i, player->buffers);
    alSourcePlay(player->source);
    if(alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "Error starting playback\n");
        return 0;
    }

    return 1;
}

StreamPlayer *player = NULL;

int audioInit()
{
    if(InitAL() != 0)
        goto error;

    if(alIsExtensionPresent("AL_SOFT_buffer_samples"))
    {
        alBufferSamplesSOFT = alGetProcAddress("alBufferSamplesSOFT");
        alIsBufferFormatSupportedSOFT = alGetProcAddress("alIsBufferFormatSupportedSOFT");
    }

    player = NewPlayer();

	player->channels = AL_STEREO_SOFT;
	player->rate = apuGetFrequency();
#if AUDIO_FLOAT
	player->type = AL_FLOAT_SOFT;
#else
	player->type = AL_SHORT_SOFT;
#endif
    player->format = GetFormat(player->channels, player->type, alIsBufferFormatSupportedSOFT);
    if(player->format == 0)
    {
        fprintf(stderr, "Unsupported format (%s, %s)\n",
                ChannelsName(player->channels), TypeName(player->type));
        goto error;
    }
	StartPlayer(player);
    return 0;

error:
    return 1;
}

int audioUpdate()
{
    ALint processed = 0, state;

    /* Get relevant source info */
    alGetSourcei(player->source, AL_SOURCE_STATE, &state);
    alGetSourcei(player->source, AL_BUFFERS_PROCESSED, &processed);
    if(alGetError() != AL_NO_ERROR)
    {
        fprintf(stderr, "Error checking source state\n");
        return 0;
    }
	if(!processed)
		return 0;

	/* Unqueue and handle processed buffer */
	ALuint bufid;
	uint8_t *data;

	alSourceUnqueueBuffers(player->source, 1, &bufid);

	/* Read the next chunk of data, refill the buffer, and queue it
	 * back on the source */
	data = apuGetBuf();
	if(data != NULL)
	{
		alBufferSamplesSOFT(bufid, player->rate, player->format,
							BytesToFrames(apuGetBufSize(), player->channels, player->type),
							player->channels, player->type, data);
		alSourceQueueBuffers(player->source, 1, &bufid);
	}
	if(alGetError() != AL_NO_ERROR)
	{
		fprintf(stderr, "Error buffering data\n");
		return 0;
	}

    /* Make sure the source hasn't underrun */
    if(state != AL_PLAYING && state != AL_PAUSED)
    {
        //ALint queued;

        alSourcePlay(player->source);
        if(alGetError() != AL_NO_ERROR)
        {
            fprintf(stderr, "Error restarting playback\n");
            return 0;
        }
    }

    return processed;
}

void audioDeinit()
{
	if(player)
	{
		DeletePlayer(player);
		player = NULL;
	}
    CloseAL();
}

void audioSleep()
{
#if WINDOWS_BUILD
	Sleep(1);
#else
	usleep(1000);
#endif
}
