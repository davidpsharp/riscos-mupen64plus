/*
 * riscos-n64: the emulator's side of the !Mupen64Plus launcher.
 *
 * Commands from the launcher's icon bar menu. The launcher sets the system variable
 * Mupen64Plus$Command; once a frame (from main_check_inputs, after SDL's event pump) this
 * reads it, clears it and does what it says, from the emulation thread as the keyboard
 * shortcuts are:
 *   save | load | slot N | fullscreen | pause | stop
 * It also keeps Mupen64Plus$GameState up to date, "<slot> <paused 0|1>", for the menu's ticks.
 *
 * And one icon on the icon bar: SDL's RISC OS driver puts the program's own icon there when it
 * becomes a Wimp task, but the launcher's icon stays while a game runs (its menu has the game's
 * commands). So when Mupen64Plus$LauncherIcon is set (the launcher's Game file sets it), the
 * program is linked with --wrap=_kernel_swi, and this declines SDL's Wimp_CreateIcon on the icon
 * bar: SDL then has icon -1 and never sees a click on one.
 *
 * Copyright (C) 2026 David Sharp. GPL-2.0, as mupen64plus.
 */
#ifdef __riscos__

#include <kernel.h>
#include <swis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main/main.h"
#include "main/savestates.h"
#include "plugin/plugin.h"

static char const CommandVar[] = "Mupen64Plus$Command";
static char const StateVar[] = "Mupen64Plus$GameState";

/* the value of a system variable, or "" if it isn't set */
static void read_var(char const * name, char * buf, int size)
{
    _kernel_swi_regs r;
    r.r[0] = (int)name;
    r.r[1] = (int)buf;
    r.r[2] = size - 1;
    r.r[3] = 0;
    r.r[4] = 0;
    buf[0] = 0;
    if (_kernel_swi(OS_ReadVarVal, &r, &r) == NULL && r.r[2] >= 0 && r.r[2] < size)
        buf[r.r[2]] = 0;
    else
        buf[0] = 0;
}

static void set_var(char const * name, char const * value)
{
    _kernel_swi_regs r;
    r.r[0] = (int)name;
    r.r[1] = (int)value;
    r.r[2] = value ? (int)strlen(value) : -1;   /* -1: delete it */
    r.r[3] = 0;
    r.r[4] = 0;
    _kernel_swi(OS_SetVarVal, &r, &r);
}

void riscos_desktop_poll(void)
{
    static char last_state[16];
    char cmd[32], state[16];

    read_var(CommandVar, cmd, sizeof(cmd));
    if (cmd[0]) {
        set_var(CommandVar, NULL);
        if (strcmp(cmd, "save") == 0)
            main_state_save(0, NULL);           /* mupen64plus's format, the current slot */
        else if (strcmp(cmd, "load") == 0)
            main_state_load(NULL);
        else if (strncmp(cmd, "slot ", 5) == 0)
            main_state_set_slot(atoi(cmd + 5));
        else if (strcmp(cmd, "fullscreen") == 0)
            gfx.changeWindow();                 /* as Alt+Enter */
        else if (strcmp(cmd, "pause") == 0)
            main_toggle_pause();
        else if (strcmp(cmd, "stop") == 0)
            main_stop();
    }

    snprintf(state, sizeof(state), "%u %d", savestates_get_slot(), g_rom_pause ? 1 : 0);
    if (strcmp(state, last_state) != 0) {
        set_var(StateVar, state);
        strcpy(last_state, state);
    }
}

/* --- no icon of SDL's own on the icon bar ----------------------------------------------- */

_kernel_oserror * __real__kernel_swi(int no, _kernel_swi_regs * in, _kernel_swi_regs * out);

__attribute__((used))
_kernel_oserror * __wrap__kernel_swi(int no, _kernel_swi_regs * in, _kernel_swi_regs * out)
{
    if ((no & ~0x20000) == Wimp_CreateIcon && in != NULL && in->r[1] != 0 &&
        *(int const *)in->r[1] == -1 /* the right of the icon bar */) {
        char value[8];
        read_var("Mupen64Plus$LauncherIcon", value, sizeof(value));
        if (value[0]) {
            out->r[0] = -1;
            return NULL;
        }
    }
    return __real__kernel_swi(no, in, out);
}

#endif
