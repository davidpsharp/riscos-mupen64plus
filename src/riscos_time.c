/* A millisecond clock and delay for RISC OS, for the core's speed limiter.
 *
 * UnixLib's clocks and SDL_GetTicks count centiseconds, and SDL_Delay rounds up to the next
 * centisecond and often a further one (16 ms takes 20-30 ms), which is too coarse for a 16.7 ms
 * frame. The HAL's counter 0 counts down from its period to 0 once a centisecond, so the
 * monotonic centisecond count plus how far the counter has got gives microseconds (as in the
 * riscos-doom3 and ioquake3 ports). Where there's no counter (OS_Hardware fails) it's centiseconds.
 *
 * Copyright (C) 2026 David Sharp. GPL-2.0, as mupen64plus. */
#ifdef __riscos__
#include <kernel.h>
#include <swis.h>
#include <stdlib.h>
#include <SDL.h>

static unsigned int rate, period;

unsigned long long riscos_now_us(void)
{
    unsigned int cs1, cs2, count;
    unsigned long long us;
    if (!rate)
    {
        if (_swix(OS_Hardware, _IN(0)|_INR(8,9)|_OUT(0), 0, 0, 19, &rate) ||
            _swix(OS_Hardware, _IN(0)|_INR(8,9)|_OUT(0), 0, 0, 20, &period) || !rate || !period)
        {
            rate = 1;
            period = 0;
        }
    }
    do {
        _swix(OS_ReadMonotonicTime, _OUT(0), &cs1);
        count = 0;
        if (period)
            _swix(OS_Hardware, _IN(0)|_INR(8,9)|_OUT(0), 0, 0, 21, &count);
        _swix(OS_ReadMonotonicTime, _OUT(0), &cs2);
    } while (cs1 != cs2);
    us = (unsigned long long) cs1 * 10000;
    if (period && count <= period)
        us += (unsigned long long) (period - count) * 1000000 / rate;
    return us;
}

unsigned int riscos_ticks_ms(void)
{
    return (unsigned int) (riscos_now_us() / 1000);
}

/* sleep in centisecond steps (SDL_Delay) while at least 25 ms remain, then wait out the rest; with
 * no HAL counter the clock is only centiseconds, so waiting precisely is pointless: SDL_Delay */
void riscos_delay_ms(unsigned int ms)
{
    static int mode = -1;                 /* N64_DELAY=sdl: plain SDL_Delay (for comparison) */
    unsigned long long end;
    if (mode < 0)
        mode = getenv("N64_DELAY") != NULL && getenv("N64_DELAY")[0] == 's';
    if (mode || (riscos_now_us(), period == 0))   /* no HAL counter (RoMac): only centiseconds */
    {
        SDL_Delay(ms);
        return;
    }
    end = riscos_now_us() + (unsigned long long) ms * 1000;
    while (riscos_now_us() + 25000 < end)
        SDL_Delay(1);
    while (riscos_now_us() < end)
        ;
}
#endif
