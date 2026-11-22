/* How long SDL_Delay(n) really takes on RISC OS, and SDL_GetTicks's resolution. */
#include <stdio.h>
#include <sys/time.h>
#include <SDL.h>

static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char **argv)
{
    int n, i;
    unsigned t0, t1, steps = 0;
    (void) argc; (void) argv;
    SDL_Init(SDL_INIT_TIMER);
    t0 = SDL_GetTicks();
    for (i = 0; i < 200; )
    {
        t1 = SDL_GetTicks();
        if (t1 != t0) { steps += t1 - t0; t0 = t1; i++; }
    }
    printf("SDL_GetTicks step: %.2f ms average\n", steps / 200.0);
    for (n = 0; n <= 16; n = n ? n * 2 : 1)
    {
        double a = now_ms(), worst = 0;
        for (i = 0; i < 20; i++)
        {
            double b = now_ms();
            SDL_Delay(n);
            if (now_ms() - b > worst) worst = now_ms() - b;
        }
        printf("SDL_Delay(%2d): %.2f ms average, %.2f worst\n", n, (now_ms() - a) / 20, worst);
    }
    SDL_Quit();
    return 0;
}
