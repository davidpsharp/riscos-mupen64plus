/*
 * gl16: does drawing through SDL's GL window write outside its buffers on a 16 bpp (64K colour)
 * desktop? Mupen64Plus/Rice draws garbage textures in a 64K colour mode (GPU, software Mesa and
 * RoMac alike) and correctly in 16M colours, so this looks for heap memory being overwritten.
 *
 * It fills "guard" blocks on the heap with a pattern (before and after the GL context is made),
 * opens a window with a GL context as Rice asks for one, draws a textured quad for N frames
 * (uploading the texture every frame from a static pattern, as Rice's texture cache does), and
 * checks the guards and the texture's source after each frame. It prints the first corruption.
 *
 *   gl16 [frames] [w] [h]       (default 200 frames, 640 x 480)
 *
 * RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
 */
#include <SDL.h>
#include <SDL_opengl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GUARDS 48
#define GUARD_SIZE (64 * 1024)
#define TEX 256

static unsigned char * guards[GUARDS];
static unsigned char texsrc[TEX * TEX * 4];

/* the texture: 32-pixel red/white checks, BGRA bytes, with a blue ramp across */
static unsigned char texbyte(size_t i)
{
    size_t px = i / 4, x = px % TEX, y = px / TEX;
    int white = ((x / 32) + (y / 32)) & 1;
    switch (i & 3) {
    case 0: return white ? 255 : (unsigned char)x;          /* B */
    case 1: return white ? 255 : 0;                          /* G */
    case 2: return 255;                                      /* R */
    default: return 255;                                     /* A */
    }
}

static unsigned char pat(int g, size_t i) { return (unsigned char)(g * 31 + i * 7 + (i >> 9)); }

static void fill(int from, int to)
{
    for (int g = from; g < to; g++) {
        guards[g] = malloc(GUARD_SIZE);
        for (size_t i = 0; i < GUARD_SIZE; i++) guards[g][i] = pat(g, i);
    }
}

static int check(int frame)
{
    int bad = 0;
    for (int g = 0; g < GUARDS; g++) {
        if (!guards[g]) continue;
        for (size_t i = 0; i < GUARD_SIZE; i++)
            if (guards[g][i] != pat(g, i)) {
                size_t j = i;
                while (j < GUARD_SIZE && guards[g][j] != pat(g, j)) j++;
                printf("frame %d: guard %d (%p) changed at +%zu, %zu bytes, first %02x %02x %02x %02x\n",
                       frame, g, (void *)guards[g], i, j - i, guards[g][i],
                       i + 1 < GUARD_SIZE ? guards[g][i + 1] : 0,
                       i + 2 < GUARD_SIZE ? guards[g][i + 2] : 0,
                       i + 3 < GUARD_SIZE ? guards[g][i + 3] : 0);
                for (size_t k = 0; k < GUARD_SIZE; k++) guards[g][k] = pat(g, k);
                bad = 1;
                break;
            }
    }
    for (size_t i = 0; i < sizeof(texsrc); i++)
        if (texsrc[i] != texbyte(i)) {
            printf("frame %d: texture source changed at +%zu\n", frame, i);
            for (size_t k = 0; k < sizeof(texsrc); k++) texsrc[k] = texbyte(k);
            bad = 1;
            break;
        }
    return bad;
}

int main(int argc, char ** argv)
{
    int frames = argc > 1 ? atoi(argv[1]) : 200;
    int w = argc > 2 ? atoi(argv[2]) : 640, h = argc > 3 ? atoi(argv[3]) : 480;
    int bad_frames = 0;

    for (size_t i = 0; i < sizeof(texsrc); i++) texsrc[i] = texbyte(i);
    fill(0, GUARDS / 2);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
    SDL_DisplayMode dm;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0)
        printf("desktop %dx%d %s (%d bpp)\n", dm.w, dm.h, SDL_GetPixelFormatName(dm.format),
               SDL_BITSPERPIXEL(dm.format));
    /* as Rice asks (OGLGraphicsContext.cpp) */
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_Window * win = SDL_CreateWindow("gl16", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
                                        SDL_WINDOW_OPENGL);
    if (!win) { printf("SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) { printf("SDL_GL_CreateContext: %s\n", SDL_GetError()); return 1; }
    int r, g, b, a, d;
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r); SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b); SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &d);
    printf("GL %s / %s; rgba %d%d%d%d depth %d\n", glGetString(GL_RENDERER), glGetString(GL_VERSION),
           r, g, b, a, d);
    fill(GUARDS / 2, GUARDS);
    if (check(-1)) bad_frames++;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glEnable(GL_TEXTURE_2D);

    for (int f = 0; f < frames; f++) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) frames = f;
        glViewport(0, 0, w, h);
        glClearColor(0.2f, 0.3f, 0.5f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        /* Rice uploads with GL_BGRA where it can */
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX, TEX, 0, GL_BGRA, GL_UNSIGNED_BYTE, texsrc);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-0.9f, -0.9f);
        glTexCoord2f(1, 0); glVertex2f(0.9f, -0.9f);
        glTexCoord2f(1, 1); glVertex2f(0.9f, 0.9f);
        glTexCoord2f(0, 1); glVertex2f(-0.9f, 0.9f);
        glEnd();
        SDL_GL_SwapWindow(win);
        if (check(f)) bad_frames++;
    }
    printf("%d frames, %d with corruption; glGetError %x\n", frames, bad_frames, glGetError());
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
