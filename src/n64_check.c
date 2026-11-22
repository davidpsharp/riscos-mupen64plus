/* Test hooks for the port, called by the core at every vertical interrupt (main.c's new_vi).
 * They work the same on every platform and with any video plugin, the dummy one included,
 * because they read the N64's own memory, not the host's screen.
 *
 * Set by environment variables (on RISC OS, system variables of the same names):
 *   N64_HASHLOG=file   every N64_HASHEVERY VIs (default 60), a line "vi N rdram HASH fb HASH ms T"
 *                      (FNV-1a of RDRAM and of the VI's framebuffer; T = wall-clock ms since start)
 *   N64_SHOT=vi:file.png[,vi:file.png...]   the VI framebuffer as a PNG at those VIs
 *   N64_QUITVI=n       stop the emulator after n VIs
 *   N64_NOLIMIT=1      run as fast as possible (the speed limiter off)
 *   N64_VITIMES=file   every VI, a line "N us": microseconds since the previous VI (a fine clock)
 *   N64_PROFILE=file   at quit, where the time went: the RSP (with HLE, the video plugin's display
 *                      lists too), the video plugin's screen updates (swaps), the SDL event pump,
 *                      and the rest (CPU emulation). Sums of clock differences, so a coarse clock
 *                      (centiseconds) still gives unbiased totals over many calls.
 *
 * Copyright (C) 2026 David Sharp. GPL-2.0, as mupen64plus. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <png.h>

#include "device/device.h"
#include "main/main.h"
#include "plugin/plugin.h"

extern struct device g_dev;

static int inited;
static unsigned vi_count;
static FILE *hashlog;
static unsigned hash_every = 60;
static unsigned quit_vi;
static double t0;
static char shots[1024];

#ifdef __riscos__
unsigned long long riscos_now_us(void);
static unsigned long long now_us(void) { return riscos_now_us(); }
#else
static unsigned long long now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000ull + tv.tv_usec;
}
#endif
static FILE *vitimes;
static FILE *profile;
enum { P_RSP, P_SCREEN, P_PUMP, P_N };
static const char *const pnames[P_N] = { "rsp (incl. HLE display lists)", "gfx updateScreen", "SDL event pump" };
static unsigned long long ptime[P_N], pcalls[P_N], pump_t0, prof_t0;
static ptr_DoRspCycles real_rsp;
static ptr_UpdateScreen real_screen;

static unsigned int CALL timed_rsp(unsigned int cycles)
{
    unsigned long long t = now_us();
    unsigned int r = real_rsp(cycles);
    ptime[P_RSP] += now_us() - t;
    pcalls[P_RSP]++;
    return r;
}

static void CALL timed_screen(void)
{
    unsigned long long t = now_us();
    real_screen();
    ptime[P_SCREEN] += now_us() - t;
    pcalls[P_SCREEN]++;
}

void n64_check_pump_begin(void)
{
    if (profile != NULL)
        pump_t0 = now_us();
}

void n64_check_pump_end(void)
{
    if (profile != NULL)
    {
        ptime[P_PUMP] += now_us() - pump_t0;
        pcalls[P_PUMP]++;
    }
}

static void write_profile(void)
{
    unsigned long long total = now_us() - prof_t0, rest = total;
    int i;
    for (i = 0; i < P_N; i++)
    {
        fprintf(profile, "%-32s %8.0f ms %5.1f%%  %llu calls\n", pnames[i], ptime[i] / 1000.0,
                total ? 100.0 * ptime[i] / total : 0, pcalls[i]);
        rest -= ptime[i];
    }
    fprintf(profile, "%-32s %8.0f ms %5.1f%%\n", "the rest (CPU emulation)", rest / 1000.0,
            total ? 100.0 * rest / total : 0);
    fprintf(profile, "%-32s %8.0f ms over %u VIs\n", "total", total / 1000.0, vi_count);
    fclose(profile);
    profile = NULL;
}
static unsigned long long last_us;

static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

/* the framebuffer the VI shows: origin, width, pixel size, and a height guessed from the width */
struct vifb { uint32_t origin, width, height, bpp; };

static int vi_fb(struct vifb *f)
{
    const uint32_t *vi = g_dev.vi.regs;
    uint32_t type = vi[VI_STATUS_REG] & 3;
    f->origin = vi[VI_ORIGIN_REG] & 0xffffff;
    f->width = vi[VI_WIDTH_REG] & 0xfff;
    f->bpp = type == 3 ? 4 : type == 2 ? 2 : 0;
    f->height = f->width * 3 / 4;
    if (f->bpp == 0 || f->width == 0 || f->width > 1024)
        return 0;
    if (f->origin + f->width * f->height * f->bpp > g_dev.rdram.dram_size)
        return 0;
    return 1;
}

/* one pixel as 0xRRGGBB; RDRAM is held as host-order 32-bit words */
static uint32_t pixel(const struct vifb *f, uint32_t x, uint32_t y)
{
    const uint8_t *ram = (const uint8_t *) g_dev.rdram.dram;
    uint32_t a = f->origin + (y * f->width + x) * f->bpp;
    if (f->bpp == 4)
    {
        uint32_t w = g_dev.rdram.dram[a >> 2];
        return w >> 8;
    }
    else
    {
        uint32_t w = g_dev.rdram.dram[a >> 2];
        uint32_t p = (a & 2) ? (w & 0xffff) : (w >> 16);
        uint32_t r = (p >> 11) & 31, g = (p >> 6) & 31, b = (p >> 1) & 31;
        (void) ram;
        return ((r << 3 | r >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (b << 3 | b >> 2);
    }
}

static uint32_t fb_hash(void)
{
    struct vifb f;
    uint32_t h = 2166136261u, x, y;
    if (!vi_fb(&f))
        return 0;
    for (y = 0; y < f.height; y++)
        for (x = 0; x < f.width; x++)
        {
            uint32_t p = pixel(&f, x, y);
            h = (h ^ (p & 0xff)) * 16777619u;
            h = (h ^ ((p >> 8) & 0xff)) * 16777619u;
            h = (h ^ (p >> 16)) * 16777619u;
        }
    return h;
}

static void write_png(const char *path)
{
    struct vifb f;
    FILE *fp;
    png_structp png;
    png_infop info;
    uint8_t *row;
    uint32_t x, y;

    if (!vi_fb(&f))
    {
        fprintf(stderr, "n64_check: no framebuffer at VI %u for %s\n", vi_count, path);
        return;
    }
    fp = fopen(path, "wb");
    if (fp == NULL)
        return;
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        fclose(fp);
        return;
    }
    png_init_io(png, fp);
    png_set_IHDR(png, info, f.width, f.height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    row = malloc(f.width * 3);
    for (y = 0; y < f.height; y++)
    {
        for (x = 0; x < f.width; x++)
        {
            uint32_t p = pixel(&f, x, y);
            row[x * 3] = p >> 16;
            row[x * 3 + 1] = p >> 8;
            row[x * 3 + 2] = p;
        }
        png_write_row(png, row);
    }
    free(row);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    fprintf(stderr, "n64_check: VI %u: %ux%u %ubpp framebuffer written to %s\n",
            vi_count, f.width, f.height, f.bpp * 8, path);
}

static void init(void)
{
    const char *s;
    inited = 1;
    t0 = now_ms();
    if ((s = getenv("N64_HASHLOG")) != NULL && *s)
        hashlog = fopen(s, "w");
    if ((s = getenv("N64_HASHEVERY")) != NULL && atoi(s) > 0)
        hash_every = atoi(s);
    if ((s = getenv("N64_QUITVI")) != NULL)
        quit_vi = atoi(s);
    if ((s = getenv("N64_SHOT")) != NULL)
        strncpy(shots, s, sizeof shots - 1);
    if ((s = getenv("N64_PROFILE")) != NULL && *s && (profile = fopen(s, "w")) != NULL)
    {
        real_rsp = rsp.doRspCycles;
        rsp.doRspCycles = timed_rsp;
        real_screen = gfx.updateScreen;
        gfx.updateScreen = timed_screen;
        prof_t0 = now_us();
    }
    if ((s = getenv("N64_VITIMES")) != NULL && *s)
        vitimes = fopen(s, "w");
    if ((s = getenv("N64_NOLIMIT")) != NULL && *s == '1')
        main_speedlimiter_toggle();
}

void n64_check_vi(void)
{
    char *p;
    if (!inited)
        init();
    vi_count++;
    if (vitimes != NULL)
    {
        unsigned long long t = now_us();
        fprintf(vitimes, "%u %llu\n", vi_count, last_us ? t - last_us : 0);
        last_us = t;
        if (vi_count % 60 == 0)
            fflush(vitimes);
    }
    if (hashlog != NULL && vi_count % hash_every == 0)
    {
        fprintf(hashlog, "vi %u rdram %08x fb %08x ms %.0f\n", vi_count,
                fnv1a((const uint8_t *) g_dev.rdram.dram, g_dev.rdram.dram_size), fb_hash(),
                now_ms() - t0);
        fflush(hashlog);
    }
    /* "vi:file" items, separated by commas */
    for (p = shots; *p; )
    {
        char *end = strchr(p, ','), *colon = strchr(p, ':');
        size_t len = end ? (size_t) (end - p) : strlen(p);
        if (colon != NULL && colon < p + len && (unsigned) atoi(p) == vi_count)
        {
            char path[512];
            size_t n = len - (colon + 1 - p);
            if (n >= sizeof path)
                n = sizeof path - 1;
            memcpy(path, colon + 1, n);
            path[n] = 0;
            write_png(path);
        }
        p += len + (end ? 1 : 0);
    }
    if (quit_vi && vi_count == quit_vi)
    {
        if (hashlog != NULL)
            fprintf(hashlog, "quit at vi %u after %.0f ms\n", vi_count, now_ms() - t0);
        if (profile != NULL)
            write_profile();
        fprintf(stderr, "n64_check: quitting at VI %u after %.0f ms (%.1f VI/s)\n", vi_count,
                now_ms() - t0, vi_count * 1000.0 / (now_ms() - t0));
        main_stop();
    }
}
