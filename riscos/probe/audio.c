/* How fast SDL's RISC OS audio driver takes samples, with the main thread idle or busy.
 *   audioprobe [idle|busy] [samples]    plays silence at 44100 Hz for 10 s */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

static volatile unsigned long frames, calls;

static void callback(void *u, Uint8 *stream, int len)
{
    (void) u;
    memset(stream, 0, len);
    frames += len / 4;
    calls++;
}

int main(int argc, char **argv)
{
    SDL_AudioSpec want, have;
    int busy = argc > 1 && strcmp(argv[1], "busy") == 0;
    Uint32 t0, t;
    unsigned long f0;
    volatile unsigned long spin = 0;

    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    memset(&want, 0, sizeof want);
    want.freq = 44100;
    want.format = AUDIO_S16LSB;
    want.channels = 2;
    want.samples = argc > 2 ? atoi(argv[2]) : 1024;
    want.callback = callback;
    if (SDL_OpenAudio(&want, &have) != 0)
    {
        printf("SDL_OpenAudio: %s\n", SDL_GetError());
        return 1;
    }
    printf("driver %s, %d Hz, %d samples, main thread %s\n", SDL_GetCurrentAudioDriver(), have.freq,
           have.samples, busy ? "busy" : "idle");
    SDL_PauseAudio(0);
    SDL_Delay(1000);
    f0 = frames;
    t0 = SDL_GetTicks();
    while ((t = SDL_GetTicks()) - t0 < 10000)
    {
        if (busy)
            for (spin = 0; spin < 1000000; spin++)
                ;
        else
            SDL_Delay(100);
    }
    printf("%.0f sample frames/s, %lu callbacks\n", (frames - f0) * 1000.0 / (t - t0), calls);
    SDL_CloseAudio();
    SDL_Quit();
    return 0;
}
