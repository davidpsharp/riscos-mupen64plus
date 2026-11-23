/*
 * Mupen64Plus for RISC OS: the launcher (!Mupen64Plus.!RunImage).
 *
 * A small desktop task with the icon on the icon bar. Its menu has Info (About this program),
 * Help, Choices and Quit, and the running game's commands (save and load its state, its slot, full
 * screen, pause, stop), which it passes on through Mupen64Plus$Command. Dropping an N64 ROM on the icon (a .z64, .v64 or .n64 file, or a zip holding
 * one) starts the emulator in a desktop window: the Obey file Game, which runs Mupen64 with
 * the ROM's name in Unix form. A click on the icon offers the last ROM played again.
 * One game at a time: while it runs, the launcher says so. The emulator doesn't put an icon of its own
 * on the icon bar (src/riscos_desktop.c), so this one stays.
 *
 * The Wimp code (icon, menu, Info window, the message window and its buttons, the handler swap
 * around Wimp_Poll) is riscos-doom3's launcher's, itself adapted from David Sharp's OpenTS RISC OS
 * port (port/riscos/desktop.cpp).
 *
 * RISC OS port of Mupen64Plus. Copyright (C) 2026 David Sharp. GPL-2.0.
 */
#include <kernel.h>
#include <swis.h>
#include <unixlib/local.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

int __riscosify_control = __RISCOSIFY_NO_SUFFIX;

#ifndef LAUNCHER_VERSION
#define LAUNCHER_VERSION "0.1.0-dev"
#endif

namespace {

/* --- the Wimp (riscos-doom3's launcher, after OpenTS's port/riscos/desktop.cpp) ------------ */

int TaskHandle = 0;
int IconHandle = -1;
int InfoWindow = -1;
int GameTask = 0;	// the emulator's task while it runs
std::string GameRom;	// the ROM it's playing (RISC OS name)

char const AppSprite[] = "!mupen64plus";
char const AppName[] = "Mupen64Plus";
char const AppPurpose[] = "Nintendo 64 emulator";
char const AppAuthor[] = "The mupen64plus team";

bool Swi(int swi, _kernel_swi_regs & regs)
{
	_kernel_oserror * error = _kernel_swi(swi, &regs, &regs);
	if (error != nullptr) {
		fprintf(stderr, "launcher: SWI &%X: %s\n", swi, error->errmess);
		return false;
	}
	return true;
}

struct Icon {
	int x0, y0, x1, y1;
	unsigned flags;
	union {
		char text[12];
		struct {
			char const * buffer;
			char const * validation;
			int length;
		} ind;
	} data;
};

void Plain_Text(Icon & icon, int x0, int y0, int x1, int y1, unsigned flags, char const * text)
{
	icon.x0 = x0; icon.y0 = y0; icon.x1 = x1; icon.y1 = y1;
	icon.flags = flags;
	memset(icon.data.text, 0, sizeof(icon.data.text));
	strncpy(icon.data.text, text, sizeof(icon.data.text));
}

void Indirected(Icon & icon, int x0, int y0, int x1, int y1, unsigned flags, char const * text, char const * valid)
{
	icon.x0 = x0; icon.y0 = y0; icon.x1 = x1; icon.y1 = y1;
	icon.flags = flags;
	icon.data.ind.buffer = text;
	icon.data.ind.validation = valid;
	icon.data.ind.length = (int)strlen(text) + 1;
}

struct WindowBlock {
	int visible[4];
	int scroll[2];
	int behind;
	unsigned flags;
	unsigned char colours[8];
	int extent[4];
	unsigned title_flags;
	unsigned button_type;
	int sprite_area;
	short min_w, min_h;
	struct {
		char const * buffer;
		char const * validation;
		int length;
	} title;
	int icon_count;
};

// "About this program", as Edit's progInfo: labels beside sunken fields.
void Create_Info_Window(void)
{
	static char version[64];
	// the build date as "22 Nov 2026" (__DATE__ is "Nov 22 2026", the day padded with a space)
	char const * date = __DATE__;
	int day = atoi(date + 4);
	snprintf(version, sizeof(version), "%s (%d %.3s %s)", LAUNCHER_VERSION, day, date, date + 7);
	static struct {
		WindowBlock w;
		Icon icons[10];
	} block;
	static char const title[] = "About this program";
	static char const field_valid[] = "R2";
	memset(&block, 0, sizeof(block));
	block.w.visible[0] = 392; block.w.visible[1] = 628;
	block.w.visible[2] = 392 + 668; block.w.visible[3] = 628 + 308;
	block.w.behind = -1;
	block.w.flags = 0x84000012;
	unsigned char const colours[8] = {7, 2, 7, 1, 12, 14, 12, 0};
	memcpy(block.w.colours, colours, sizeof(colours));
	block.w.extent[1] = -308;
	block.w.extent[2] = 668;
	block.w.title_flags = 0x0000013D;
	block.w.sprite_area = 1;
	block.w.title.buffer = title;
	block.w.title.validation = (char const *)-1;
	block.w.title.length = sizeof(title);
	char const * labels[5] = {"Name", "Purpose", "Author", "Ported by", "Version"};
	char const * values[5] = {AppName, AppPurpose, AppAuthor, "David Sharp", version};
	for (int row = 0; row < 5; row++) {
		int const top = -4 - 60 * row;
		Indirected(block.icons[row], 184, top - 52, 660, top, 0x1700613D, values[row], field_valid);
		Plain_Text(block.icons[5 + row], 8, top - 48, 184, top - 8, 0x17000211, labels[row]);
	}
	block.w.icon_count = 10;
	_kernel_swi_regs regs;
	regs.r[1] = (int)&block;
	if (Swi(Wimp_CreateWindow, regs)) {
		InfoWindow = regs.r[0];
	}
}

struct MenuItem {
	unsigned flags;		// 1 ticked, 2 a dotted line after it, 0x80 the last item
	int submenu;
	unsigned icon_flags;
	char const * buffer;	// indirected text
	char const * validation;
	int length;
};

template <int N> struct MenuBlock {
	char title[12];
	unsigned char colours[4];
	int width, height, gap;
	MenuItem items[N];
};

// The icon bar menu. The game's commands are shaded when no game is running; while one runs, the
// launcher passes them on (Send_Command). The keys are the game's own for the same thing.
enum { MenuInfo, MenuHelp, MenuChoices, MenuSave, MenuLoad, MenuSlot, MenuFullScreen, MenuPause, MenuStop, MenuQuit,
	MenuCount };
char const * const MenuText[MenuCount] = {
	"Info", "Help", "Choices...", "Save state       F5", "Load state       F7", "Slot", "Full screen  Alt+Enter",
	"Pause                   P", "Stop game      Escape", "Quit"};
MenuBlock<MenuCount> Menu;
MenuBlock<10> SlotMenu;
char SlotText[10][4];
int MenuHeight = 0;

void Set_Item(MenuItem & item, char const * text, int submenu, bool shaded)
{
	item.submenu = submenu;
	item.icon_flags = 0x07000121 | (shaded ? 0x00400000u : 0u);	// text, filled, indirected
	item.buffer = text;
	item.validation = (char const *)-1;
	item.length = (int)strlen(text) + 1;
}

template <int N> void Menu_Header(MenuBlock<N> & menu, char const * title, int widest)
{
	memset(&menu, 0, sizeof(menu));
	strncpy(menu.title, title, sizeof(menu.title));
	menu.colours[0] = 7; menu.colours[1] = 2; menu.colours[2] = 7; menu.colours[3] = 0;
	menu.width = 16 * widest + 32;
	menu.height = 44;
}

// The game's slot and whether it's paused, as the emulator last said (Mupen64Plus$GameState).
void Game_State(int & slot, bool & paused);

void Build_Menu(void)
{
	bool const game = GameTask != 0;
	int slot = 0;
	bool paused = false;
	if (game) {
		Game_State(slot, paused);
	}
	int widest = 0;
	for (int i = 0; i < MenuCount; i++) {
		widest = (int)strlen(MenuText[i]) > widest ? (int)strlen(MenuText[i]) : widest;
	}
	Menu_Header(Menu, AppName, widest);
	for (int i = 0; i < MenuCount; i++) {
		bool const game_item = i >= MenuSave && i <= MenuStop;
		Set_Item(Menu.items[i], MenuText[i], i == MenuInfo ? InfoWindow : i == MenuSlot ? (int)&SlotMenu : -1,
			game_item && !game);
	}
	Menu.items[MenuChoices].flags |= 2;
	Menu.items[MenuStop].flags |= 2;
	if (paused) {
		Menu.items[MenuPause].flags |= 1;
	}
	Menu.items[MenuQuit].flags |= 0x80;
	MenuHeight = MenuCount * 44 + 2 * 24;

	Menu_Header(SlotMenu, "Slot", 4);
	for (int i = 0; i < 10; i++) {
		snprintf(SlotText[i], sizeof(SlotText[i]), "%d", i);
		Set_Item(SlotMenu.items[i], SlotText[i], -1, false);
		if (i == slot) {
			SlotMenu.items[i].flags |= 1;
		}
	}
	SlotMenu.items[9].flags |= 0x80;
}

void Open_Menu(int mouse_x)
{
	Build_Menu();
	_kernel_swi_regs regs;
	regs.r[1] = (int)&Menu;
	regs.r[2] = mouse_x - 64;
	regs.r[3] = 96 + MenuHeight;
	Swi(Wimp_CreateMenu, regs);
}

void Put_Icon_On_Bar(void)
{
	struct {
		int window;
		Icon icon;
	} block;
	block.window = -1;
	Plain_Text(block.icon, 0, 0, 68, 68, 0x1700301A, AppSprite);
	_kernel_swi_regs regs;
	regs.r[0] = 0;
	regs.r[1] = (int)&block;
	if (Swi(Wimp_CreateIcon, regs)) {
		IconHandle = regs.r[0];
	}
}

/* Around Wimp_Poll the OS's default handlers are in, not UnixLib's (as in OpenTS: another
   task's error or an event could otherwise run UnixLib's handler with this task paged out). */
struct HandlerType { int Address, Workspace, Buffer; };
int const Swapped_Handlers[] = {1, 2, 3, 4, 6, 7, 9, 10};
int const Swapped_Count = sizeof(Swapped_Handlers) / sizeof(Swapped_Handlers[0]);

HandlerType Set_Handler(int number, HandlerType const & handler)
{
	_kernel_swi_regs regs;
	regs.r[0] = number;
	regs.r[1] = handler.Address;
	regs.r[2] = handler.Workspace;
	regs.r[3] = handler.Buffer;
	_kernel_swi(OS_ChangeEnvironment, &regs, &regs);
	HandlerType previous = {regs.r[1], regs.r[2], regs.r[3]};
	return previous;
}

void Use_Default_Handlers(HandlerType saved[])
{
	for (int i = 0; i < Swapped_Count; i++) {
		_kernel_swi_regs regs;
		regs.r[0] = Swapped_Handlers[i];
		_kernel_swi(OS_ReadDefaultHandler, &regs, &regs);
		HandlerType handler = {regs.r[1], regs.r[2], regs.r[3]};
		saved[i] = Set_Handler(Swapped_Handlers[i], handler);
	}
}

void Use_Own_Handlers(HandlerType const saved[])
{
	for (int i = 0; i < Swapped_Count; i++) {
		Set_Handler(Swapped_Handlers[i], saved[i]);
	}
}

std::string Os_Variable(char const * name)
{
	char buf[512];
	_kernel_swi_regs regs;
	regs.r[0] = (int)name;
	regs.r[1] = (int)buf;
	regs.r[2] = sizeof(buf) - 1;
	regs.r[3] = 0;
	regs.r[4] = 3;
	if (_kernel_swi(OS_ReadVarVal, &regs, &regs) != nullptr) {
		return "";
	}
	buf[regs.r[2]] = '\0';
	return buf;
}

void Oscli(std::string const & command)
{
	_kernel_swi_regs regs;
	regs.r[0] = (int)command.c_str();
	Swi(OS_CLI, regs);
}

enum Action { ACT_CLOSE, ACT_HELP, ACT_PLAY };

int const MessageWidth = 1120;
int const MaxLines = 12;
int const MaxButtons = 3;
int MessageWindow = -1;
char MessageLines[MaxLines][100];
char ButtonText[MaxButtons][24];
Action ButtonAction[MaxButtons];
int ButtonCount = 0;
char SpriteName[16];
int StatusIcon = -1;	// the status line, under the text
int BarIcon = -1;	// the progress bar inside its trough
int TroughIcon = -1;
int TroughBox[4];
char StatusText[100];

void Close_Message(void)
{
	if (MessageWindow < 0) {
		return;
	}
	int block[1] = {MessageWindow};
	_kernel_swi_regs regs;
	regs.r[1] = (int)block;
	Swi(Wimp_DeleteWindow, regs);
	MessageWindow = -1;
	StatusIcon = BarIcon = TroughIcon = -1;
}

int Wrap(std::string const & text, size_t width)
{
	int lines = 0;
	size_t pos = 0;
	while (pos < text.size() && lines < MaxLines) {
		size_t end = text.size();
		size_t nl = text.find('\n', pos);
		if (nl != std::string::npos && nl - pos <= width) {
			end = nl;
		} else if (end - pos > width) {
			end = text.rfind(' ', pos + width);
			if (end == std::string::npos || end <= pos) {
				end = pos + width;
			}
		}
		snprintf(MessageLines[lines++], sizeof(MessageLines[0]), "%s", text.substr(pos, end - pos).c_str());
		pos = end;
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n')) {
			if (text[pos++] == '\n') {
				break;
			}
		}
	}
	return lines;
}

bool Wimp_Has_Sprite(char const * name)
{
	_kernel_swi_regs regs;
	regs.r[0] = 40;
	regs.r[2] = (int)name;
	return _kernel_swi(Wimp_SpriteOp, &regs, &regs) == nullptr;
}

struct Button {
	char const * text;
	Action action;
};

void Show_Message(std::string const & text, bool error, std::vector<Button> const & buttons, bool progress = false)
{
	Close_Message();
	fprintf(stderr, "launcher: message: %s\n", text.c_str());
	int lines = Wrap(text, 58);
	snprintf(SpriteName, sizeof(SpriteName), "%s", error ? "error" : "information");
	bool sprite = Wimp_Has_Sprite(SpriteName);
	int text_x = sprite ? 128 : 32;
	int text_bottom = -32 - 44 * (lines > 2 ? lines : 2);
	int bar_top = text_bottom - 16;
	int button_y = progress ? bar_top - 44 - 16 - 48 - 28 : text_bottom - 28;
	int height = -button_y + 68 + 28;

	static struct {
		WindowBlock w;
		Icon icons[3 + MaxButtons + MaxLines + 1];
	} block;
	memset(&block, 0, sizeof(block));
	block.w.behind = -1;
	block.w.flags = 0x84000012;
	unsigned char const colours[8] = {7, 2, 7, 1, 12, 14, 12, 0};
	memcpy(block.w.colours, colours, sizeof(colours));
	block.w.extent[1] = -height;
	block.w.extent[2] = MessageWidth;
	block.w.title_flags = 0x0000013D;
	block.w.sprite_area = 1;
	block.w.title.buffer = AppName;
	block.w.title.validation = (char const *)-1;
	block.w.title.length = sizeof(AppName);

	static char const default_valid[] = "R6,3";
	static char const plain_valid[] = "R5,3";
	ButtonCount = buttons.size() < (size_t)MaxButtons ? (int)buttons.size() : MaxButtons;
	int right = MessageWidth - 32;
	int widths[MaxButtons];
	for (int b = 0; b < ButtonCount; b++) {
		snprintf(ButtonText[b], sizeof(ButtonText[b]), "%s", buttons[b].text);
		ButtonAction[b] = buttons[b].action;
		widths[b] = (int)strlen(ButtonText[b]) * 16 + 64;
		if (widths[b] < 200) {
			widths[b] = 200;
		}
	}
	for (int b = ButtonCount - 1; b >= 0; b--) {
		Indirected(block.icons[b], right - widths[b], button_y - 68, right, button_y, 0x1700313D, ButtonText[b],
			b == ButtonCount - 1 ? default_valid : plain_valid);
		right -= widths[b] + 24;
	}
	int n = ButtonCount;
	for (int i = 0; i < lines; i++) {
		int top = -32 - 44 * i;
		Indirected(block.icons[n++], text_x, top - 44, MessageWidth - 32, top, 0x07000111, MessageLines[i],
			(char const *)-1);
		block.icons[n - 1].data.ind.length = sizeof(MessageLines[i]);
	}
	if (sprite) {
		Indirected(block.icons[n++], 32, -32 - 76, 32 + 76, -32, 0x1700011A, SpriteName, (char const *)1);
		block.icons[n - 1].data.ind.length = (int)strlen(SpriteName) + 1;
	}
	if (progress) {
		// the trough: a sunken white box; the bar: a filled green box inside it, resized as it goes
		TroughBox[0] = text_x; TroughBox[1] = bar_top - 44; TroughBox[2] = MessageWidth - 32; TroughBox[3] = bar_top;
		TroughIcon = n;
		static char const trough_valid[] = "R2";
		static char const empty[] = "";
		Indirected(block.icons[n++], TroughBox[0], TroughBox[1], TroughBox[2], TroughBox[3], 0x0700013D, empty,
			trough_valid);
		BarIcon = n;
		Plain_Text(block.icons[n++], TroughBox[0] + 4, TroughBox[1] + 4, TroughBox[0] + 4, TroughBox[3] - 4,
			0xA0000020, "");
		StatusIcon = n;
		StatusText[0] = '\0';
		Indirected(block.icons[n++], text_x, TroughBox[1] - 16 - 48, MessageWidth - 32, TroughBox[1] - 16, 0x07000111,
			StatusText, (char const *)-1);
		block.icons[n - 1].data.ind.length = sizeof(StatusText);
	}
	block.w.icon_count = n;

	_kernel_swi_regs regs;
	regs.r[1] = (int)&block;
	if (!Swi(Wimp_CreateWindow, regs)) {
		return;
	}
	MessageWindow = regs.r[0];

	int size[4];
	static int const vars[] = {4, 5, 11, 12, -1};
	regs.r[0] = (int)vars;
	regs.r[1] = (int)size;
	int screen_w = 1600, screen_h = 1200;
	if (_kernel_swi(OS_ReadVduVariables, &regs, &regs) == nullptr) {
		screen_w = (size[2] + 1) << size[0];
		screen_h = (size[3] + 1) << size[1];
	}
	int open[8] = {MessageWindow, (screen_w - MessageWidth) / 2, (screen_h - height) / 2, 0, 0, 0, 0, -1};
	open[3] = open[1] + MessageWidth;
	open[4] = open[2] + height;
	regs.r[1] = (int)open;
	Swi(Wimp_OpenWindow, regs);
	regs.r[0] = MessageWindow;
	regs.r[1] = -1;
	regs.r[2] = 0;
	regs.r[3] = 0;
	regs.r[4] = 1 << 25;
	regs.r[5] = -1;
	Swi(Wimp_SetCaretPosition, regs);
}

void Tell(std::string const & text, bool error)
{
	Show_Message(text, error, {{"OK", ACT_CLOSE}});
}


/* --- files ------------------------------------------------------------------------------ */

void Open_Help(void)
{
	Oscli("Filer_Run " + Os_Variable("Mupen64Plus$Dir") + ".!Help");
}

// The last ROM played, kept in <Choices$Write>.Mupen64Plus.LastROM (one line, its RISC OS name).
std::string Last_Rom_File(void)
{
	return Os_Variable("Choices$Write") + ".Mupen64Plus.LastROM";
}

std::string Read_Last_Rom(void)
{
	FILE * f = fopen(Last_Rom_File().c_str(), "r");
	if (f == nullptr) {
		return "";
	}
	char line[1024] = "";
	if (fgets(line, sizeof(line), f) == nullptr) {
		line[0] = '\0';
	}
	fclose(f);
	line[strcspn(line, "\r\n")] = '\0';
	return line;
}

void Write_Last_Rom(std::string const & name)
{
	Oscli("CDir " + Os_Variable("Choices$Write") + ".Mupen64Plus");
	FILE * f = fopen(Last_Rom_File().c_str(), "w");
	if (f != nullptr) {
		fprintf(f, "%s\n", name.c_str());
		fclose(f);
	}
}

std::string Leaf(std::string const & name)
{
	size_t dot = name.rfind('.');
	return dot == std::string::npos ? name : name.substr(dot + 1);
}

// What a file is, from its first bytes: an N64 ROM in one of its three byte orders, a zip
// (mupen64plus opens the first ROM inside), or neither.
enum Kind { NOT_A_ROM, ROM, ZIP, MISSING };

Kind Kind_Of(std::string const & name)
{
	FILE * f = fopen(name.c_str(), "rb");
	if (f == nullptr) {
		return MISSING;
	}
	unsigned char b[4] = {0, 0, 0, 0};
	size_t n = fread(b, 1, 4, f);
	fclose(f);
	if (n < 4) {
		return NOT_A_ROM;
	}
	if ((b[0] == 0x80 && b[1] == 0x37 && b[2] == 0x12 && b[3] == 0x40) ||	// .z64
	    (b[0] == 0x37 && b[1] == 0x80 && b[2] == 0x40 && b[3] == 0x12) ||	// .v64
	    (b[0] == 0x40 && b[1] == 0x12 && b[2] == 0x37 && b[3] == 0x80)) {	// .n64
		return ROM;
	}
	if (b[0] == 'P' && b[1] == 'K' && b[2] == 3 && b[3] == 4) {
		return ZIP;
	}
	return NOT_A_ROM;
}

/* --- commands to the running game -------------------------------------------------------- */

// The emulator reads Mupen64Plus$Command once a frame, clears it and does what it says
// (src/riscos_desktop.c): save, load, slot N, fullscreen, pause, stop.
void Set_Variable(char const * name, char const * value)
{
	_kernel_swi_regs regs;
	regs.r[0] = (int)name;
	regs.r[1] = (int)value;
	regs.r[2] = value != nullptr ? (int)strlen(value) : -1;	// -1 deletes it
	regs.r[3] = 0;
	regs.r[4] = 0;
	_kernel_swi(OS_SetVarVal, &regs, &regs);
}

void Send_Command(char const * command)
{
	if (GameTask != 0) {
		fprintf(stderr, "launcher: to the game: %s\n", command);
		Set_Variable("Mupen64Plus$Command", command);
	}
}

void Game_State(int & slot, bool & paused)
{
	std::string state = Os_Variable("Mupen64Plus$GameState");
	slot = 0;
	paused = false;
	if (state.size() >= 3) {
		slot = atoi(state.c_str());
		paused = state[state.size() - 1] == '1';
	}
}

/* --- settings: <Choices$Write>.Mupen64Plus.mupen64plus/cfg ----------------------------------- */

// The emulator's own settings file, mupen64plus's ini format. The Choices window changes a few of
// its lines and leaves the rest of the file (and anyone's edits to it) as it was. The emulator
// writes the file back when it quits (it keeps the save slot there), so while a game runs, the
// changes wait and are written when it ends.
char const CfgUnix[] = "/<Choices$Write>/Mupen64Plus/mupen64plus.cfg";

struct Setting {
	std::string section, key, value;
};
std::vector<Setting> PendingSettings;

void Ensure_Cfg(void)
{
	Oscli("CDir <Choices$Write>.Mupen64Plus");
	Oscli("CDir <Choices$Write>.Mupen64Plus.Saves");
	FILE * f = fopen(CfgUnix, "r");
	if (f != nullptr) {
		fclose(f);
		return;
	}
	Oscli("Copy <Mupen64Plus$Dir>.Defaults.mupen64plus/cfg <Choices$Write>.Mupen64Plus.mupen64plus/cfg ~C~V");
}

std::vector<std::string> Read_Lines(void)
{
	std::vector<std::string> lines;
	FILE * f = fopen(CfgUnix, "r");
	if (f == nullptr) {
		return lines;
	}
	char line[2048];
	while (fgets(line, sizeof(line), f) != nullptr) {
		line[strcspn(line, "\r\n")] = '\0';
		lines.push_back(line);
	}
	fclose(f);
	return lines;
}

std::string Trim(std::string const & text)
{
	size_t a = text.find_first_not_of(" \t");
	size_t b = text.find_last_not_of(" \t");
	return a == std::string::npos ? "" : text.substr(a, b - a + 1);
}

// Where a key's line is in a section ("key = value"), or -1; also where the section ends.
int Find_Key(std::vector<std::string> const & lines, std::string const & section, std::string const & key, int & end)
{
	bool in = false;
	end = -1;
	for (size_t i = 0; i < lines.size(); i++) {
		std::string const t = Trim(lines[i]);
		if (!t.empty() && t[0] == '[') {
			if (in) {
				return -1;
			}
			in = t == "[" + section + "]";
			if (in) {
				end = (int)i + 1;
			}
			continue;
		}
		if (!in) {
			continue;
		}
		if (!t.empty() && t[0] != '#') {
			end = (int)i + 1;
		}
		size_t eq = t.find('=');
		if (eq != std::string::npos && Trim(t.substr(0, eq)) == key) {
			return (int)i;
		}
	}
	return -1;
}

std::string Get_Setting(std::vector<std::string> const & lines, char const * section, char const * key)
{
	int end;
	int i = Find_Key(lines, section, key, end);
	if (i < 0) {
		return "";
	}
	std::string value = Trim(lines[i].substr(lines[i].find('=') + 1));
	if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"') {
		value = value.substr(1, value.size() - 2);
	}
	return value;
}

bool Write_Settings(std::vector<Setting> const & settings)
{
	Ensure_Cfg();
	std::vector<std::string> lines = Read_Lines();
	if (lines.empty()) {
		return false;
	}
	for (Setting const & setting : settings) {
		std::string const line = setting.key + " = " + setting.value;
		int end;
		int i = Find_Key(lines, setting.section, setting.key, end);
		if (i >= 0) {
			lines[i] = line;
		} else if (end >= 0) {
			lines.insert(lines.begin() + end, line);
		} else {
			lines.push_back("");
			lines.push_back("[" + setting.section + "]");
			lines.push_back(line);
		}
	}
	FILE * f = fopen(CfgUnix, "w");
	if (f == nullptr) {
		return false;
	}
	for (std::string const & line : lines) {
		fprintf(f, "%s\n", line.c_str());
	}
	return fclose(f) == 0;
}

/* --- the Choices window -------------------------------------------------------------------- */

int ChoicesWindow = -1;
int const ChoicesWidth = 1000;

struct ChoiceSize { int w, h; char const * text; };
ChoiceSize const Sizes[] = {{640, 480, "640 \xD7 480"}, {960, 720, "960 \xD7 720"}, {1280, 960, "1280 \xD7 960"}};
int const SizeCount = 3;
char const * const Cores[] = {"Pure interpreter (slowest)", "Cached interpreter", "Dynamic recompiler (fastest)"};

// icon numbers
enum {
	IcoSizeLabel, IcoSize0, IcoSize1, IcoSize2, IcoFullScreen,
	IcoCoreLabel, IcoCore2, IcoCore1, IcoCore0,
	IcoSoundLabel, IcoSound,
	IcoNote, IcoEdit, IcoCancel, IcoSave,
	IcoCount
};

void Create_Choices_Window(void)
{
	static struct {
		WindowBlock w;
		Icon icons[IcoCount];
	} block;
	static char const title[] = "Mupen64Plus choices";
	static char const radio_valid[] = "Sradiooff,radioon";
	static char const option_valid[] = "Soptoff,opton";
	static char const default_valid[] = "R6,3";
	static char const action_valid[] = "R5,3";
	static char const label_valid[] = "";
	int const height = 724;
	memset(&block, 0, sizeof(block));
	block.w.behind = -1;
	block.w.flags = 0x84000012;
	unsigned char const colours[8] = {7, 2, 7, 1, 12, 14, 12, 0};
	memcpy(block.w.colours, colours, sizeof(colours));
	block.w.extent[1] = -height;
	block.w.extent[2] = ChoicesWidth;
	block.w.title_flags = 0x0000013D;
	block.w.sprite_area = 1;
	block.w.title.buffer = title;
	block.w.title.validation = (char const *)-1;
	block.w.title.length = sizeof(title);

	unsigned const label = 0x17000111;		// text, vertically centred, indirected
	unsigned const option = 0x17003113;		// text right of a sprite, vertically centred, indirected, click
	unsigned const radio = 0x1700B113;		// the same, radio (button type 11); the ESG is added
	int y = -24;
	Indirected(block.icons[IcoSizeLabel], 24, y - 44, 600, y, label, "Window size", label_valid);
	y -= 52;
	for (int i = 0; i < SizeCount; i++) {
		Indirected(block.icons[IcoSize0 + i], 64 + 300 * i, y - 44, 64 + 300 * i + 280, y, radio | (1 << 16),
			Sizes[i].text, radio_valid);
	}
	y -= 52;
	Indirected(block.icons[IcoFullScreen], 64, y - 44, ChoicesWidth - 32, y, option,
		"Start in full screen (Alt+Enter switches while playing)", option_valid);
	y -= 76;
	Indirected(block.icons[IcoCoreLabel], 24, y - 44, 600, y, label, "Emulation", label_valid);
	y -= 52;
	for (int i = 0; i < 3; i++) {
		Indirected(block.icons[IcoCore2 + i], 64, y - 44, ChoicesWidth - 32, y, radio | (2 << 16), Cores[2 - i],
			radio_valid);
		y -= 52;
	}
	y -= 24;
	Indirected(block.icons[IcoSoundLabel], 24, y - 44, 600, y, label, "Sound", label_valid);
	y -= 52;
	Indirected(block.icons[IcoSound], 64, y - 44, ChoicesWidth - 32, y, option,
		"Play sound (needs SharedSoundBuffer and StreamManager)", option_valid);
	y -= 76;
	Indirected(block.icons[IcoNote], 24, y - 44, ChoicesWidth - 24, y, label,
		"These apply from the next game. Edit file opens all the settings.", label_valid);
	y -= 76;
	Indirected(block.icons[IcoEdit], 24, y - 68, 224, y, 0x1700313D, "Edit file", action_valid);
	Indirected(block.icons[IcoCancel], ChoicesWidth - 456, y - 68, ChoicesWidth - 256, y, 0x1700313D, "Cancel",
		action_valid);
	Indirected(block.icons[IcoSave], ChoicesWidth - 232, y - 68, ChoicesWidth - 24, y, 0x1700313D, "Save",
		default_valid);
	block.w.icon_count = IcoCount;

	_kernel_swi_regs regs;
	regs.r[1] = (int)&block;
	if (Swi(Wimp_CreateWindow, regs)) {
		ChoicesWindow = regs.r[0];
	}
}

void Select_Icon(int icon, bool on)
{
	int block[4] = {ChoicesWindow, icon, on ? 0x00200000 : 0, 0x00200000};
	_kernel_swi_regs regs;
	regs.r[1] = (int)block;
	Swi(Wimp_SetIconState, regs);
}

bool Icon_Selected(int icon)
{
	int block[10] = {ChoicesWindow, icon};
	_kernel_swi_regs regs;
	regs.r[1] = (int)block;
	return Swi(Wimp_GetIconState, regs) && (block[6] & 0x00200000) != 0;
}

// The settings shown: the ones waiting for the game to end, else the file's.
void Show_Choices(void)
{
	Ensure_Cfg();
	std::vector<std::string> lines = Read_Lines();
	for (Setting const & setting : PendingSettings) {
		int end;
		int i = Find_Key(lines, setting.section, setting.key, end);
		if (i >= 0) {
			lines[i] = setting.key + " = " + setting.value;
		}
	}
	int w = atoi(Get_Setting(lines, "Video-General", "ScreenWidth").c_str());
	bool known = false;
	for (int i = 0; i < SizeCount; i++) {
		Select_Icon(IcoSize0 + i, Sizes[i].w == w);
		known = known || Sizes[i].w == w;
	}
	if (!known) {
		Select_Icon(IcoSize0, false);	// another size, set in the file: none of these ticked
	}
	Select_Icon(IcoFullScreen, Get_Setting(lines, "Video-General", "Fullscreen") == "True");
	std::string core = Get_Setting(lines, "Core", "R4300Emulator");
	int c = core.empty() ? 2 : atoi(core.c_str());
	for (int i = 0; i < 3; i++) {
		Select_Icon(IcoCore2 + i, (2 - i) == (c > 2 ? 2 : c));
	}
	Select_Icon(IcoSound, Get_Setting(lines, "UI-Console", "AudioPlugin") != "dummy");
}

void Open_Choices(void)
{
	if (ChoicesWindow < 0) {
		Create_Choices_Window();
	}
	if (ChoicesWindow < 0) {
		return;
	}
	Show_Choices();
	int state[9] = {ChoicesWindow};
	_kernel_swi_regs regs;
	regs.r[1] = (int)state;
	Swi(Wimp_GetWindowState, regs);
	bool const open = (state[8] & 0x10000) != 0;
	if (!open) {	// centred on the screen the first time it opens each time
		int size[4];
		static int const vars[] = {4, 5, 11, 12, -1};
		regs.r[0] = (int)vars;
		regs.r[1] = (int)size;
		int screen_w = 1600, screen_h = 1200;
		if (_kernel_swi(OS_ReadVduVariables, &regs, &regs) == nullptr) {
			screen_w = (size[2] + 1) << size[0];
			screen_h = (size[3] + 1) << size[1];
		}
		int const h = state[4] - state[2] > 0 ? state[4] - state[2] : 724;
		state[1] = (screen_w - ChoicesWidth) / 2;
		state[2] = (screen_h - h) / 2;
		state[3] = state[1] + ChoicesWidth;
		state[4] = state[2] + h;
	}
	state[7] = -1;
	regs.r[1] = (int)state;
	Swi(Wimp_OpenWindow, regs);
}

void Close_Choices(void)
{
	if (ChoicesWindow < 0) {
		return;
	}
	int block[1] = {ChoicesWindow};
	_kernel_swi_regs regs;
	regs.r[1] = (int)block;
	Swi(Wimp_CloseWindow, regs);
}

void Tell(std::string const & text, bool error);

void Save_Choices(void)
{
	std::vector<Setting> settings;
	for (int i = 0; i < SizeCount; i++) {
		if (Icon_Selected(IcoSize0 + i)) {
			settings.push_back({"Video-General", "ScreenWidth", std::to_string(Sizes[i].w)});
			settings.push_back({"Video-General", "ScreenHeight", std::to_string(Sizes[i].h)});
		}
	}
	settings.push_back({"Video-General", "Fullscreen", Icon_Selected(IcoFullScreen) ? "True" : "False"});
	for (int i = 0; i < 3; i++) {
		if (Icon_Selected(IcoCore2 + i)) {
			settings.push_back({"Core", "R4300Emulator", std::to_string(2 - i)});
		}
	}
	settings.push_back({"UI-Console", "AudioPlugin",
		Icon_Selected(IcoSound) ? "\"mupen64plus-audio-sdl\"" : "\"dummy\""});
	if (GameTask != 0) {
		// the game writes the file when it quits: these go in after that
		for (Setting const & setting : settings) {
			bool replaced = false;
			for (Setting & pending : PendingSettings) {
				if (pending.section == setting.section && pending.key == setting.key) {
					pending.value = setting.value;
					replaced = true;
				}
			}
			if (!replaced) {
				PendingSettings.push_back(setting);
			}
		}
		Close_Choices();
		Tell("Saved. They apply from the next game (they're written to the settings file when this one ends).",
			false);
		return;
	}
	if (!Write_Settings(settings)) {
		Tell("The settings file, <Choices$Write>.Mupen64Plus.mupen64plus/cfg, couldn't be written.", true);
		return;
	}
	Close_Choices();
}

void Edit_Cfg(void)
{
	Ensure_Cfg();
	Oscli("Filer_Run " + Os_Variable("Choices$Write") + ".Mupen64Plus.mupen64plus/cfg");
	if (GameTask != 0) {
		Tell("While a game is running, changes to the file can be lost: the emulator writes the file when the game "
			"ends. Edit it between games.", false);
	}
}

void Choices_Click(int icon, int buttons)
{
	if (icon == IcoFullScreen || icon == IcoSound) {
		Select_Icon(icon, !Icon_Selected(icon));
		return;
	}
	if (!(buttons & (4 | 1))) {
		return;
	}
	if (icon == IcoSave) {
		Save_Choices();
	} else if (icon == IcoCancel) {
		if (buttons & 1) {	// Adjust: back to what's saved, and stay open
			Show_Choices();
		} else {
			Close_Choices();
		}
	} else if (icon == IcoEdit) {
		Edit_Cfg();
	}
}

/* --- the game ---------------------------------------------------------------------------- */

void Play(std::string const & riscos_name)
{
	if (GameTask != 0) {
		Tell("A game is already running (" + Leaf(GameRom) + "). Close its window first.", false);
		return;
	}
	switch (Kind_Of(riscos_name)) {
		case MISSING:
			Tell(Leaf(riscos_name) + " can't be opened.", true);
			return;
		case NOT_A_ROM:
			Tell(Leaf(riscos_name) + " isn't an N64 ROM. Mupen64Plus plays .z64, .v64 and .n64 "
				"files (cartridge dumps), and zips holding one.", true);
			return;
		default:
			break;
	}
	char unix_name[1024];
	if (__unixify_std(riscos_name.c_str(), unix_name, sizeof(unix_name), __RISCOSIFY_FILETYPE_NOTSPECIFIED) == nullptr) {
		Tell("The name " + riscos_name + " is too long.", true);
		return;
	}
	std::string command = "Run " + Os_Variable("Mupen64Plus$Dir") + ".Game \"" + unix_name + "\"";
	fprintf(stderr, "launcher: starting the game: %s\n", command.c_str());
	_kernel_swi_regs regs;
	regs.r[0] = (int)command.c_str();
	if (!Swi(Wimp_StartTask, regs)) {
		Tell("The emulator couldn't be started.", true);
		return;
	}
	GameTask = regs.r[0];
	GameRom = riscos_name;
	Write_Last_Rom(riscos_name);
	Close_Message();
}

void Icon_Clicked(void)
{
	std::string last = Read_Last_Rom();
	if (GameTask != 0) {
		Tell(Leaf(GameRom) + " is running.", false);
		return;
	}
	if (!last.empty() && Kind_Of(last) != MISSING) {
		Show_Message("Play " + Leaf(last) + " again?\nOr drop another N64 ROM on the icon.", false,
			{{"Cancel", ACT_CLOSE}, {"Help", ACT_HELP}, {"Play", ACT_PLAY}});
		return;
	}
	Show_Message("Drop an N64 ROM on this icon to play it: a .z64, .v64 or .n64 file (a dump of a "
		"cartridge you own), or a zip holding one.", false, {{"Help", ACT_HELP}, {"OK", ACT_CLOSE}});
}

void Message_Button(int button)
{
	Action action = button >= 0 && button < ButtonCount ? ButtonAction[button] : ACT_CLOSE;
	if (button < 0) {	// the close icon
		action = ACT_CLOSE;
	}
	switch (action) {
		case ACT_HELP:
			Open_Help();
			break;
		case ACT_PLAY:
			Play(Read_Last_Rom());
			break;
		default:
			Close_Message();
			break;
	}
}

void Data_Load(int * block)
{
	std::string file = (char const *)&block[11];
	block[3] = block[2];	// acknowledge it (Message_DataLoadAck)
	block[4] = 4;
	_kernel_swi_regs regs;
	regs.r[0] = 17;
	regs.r[1] = (int)block;
	regs.r[2] = block[1];
	Swi(Wimp_SendMessage, regs);
	Play(file);
}

void Poll_Loop(void)
{
	int block[64];
	for (;;) {
		_kernel_swi_regs regs;
		regs.r[0] = 1;	// no null events
		regs.r[1] = (int)block;
		HandlerType own[Swapped_Count];
		Use_Default_Handlers(own);
		_kernel_oserror * error = _kernel_swi(Wimp_Poll, &regs, &regs);
		Use_Own_Handlers(own);
		if (error != nullptr) {
			fprintf(stderr, "launcher: Wimp_Poll: %s\n", error->errmess);
			return;
		}
		switch (regs.r[0]) {
			case 2:	// Open_Window_Request
				regs.r[1] = (int)block;
				Swi(Wimp_OpenWindow, regs);
				break;
			case 3:	// Close_Window_Request
				if (block[0] == ChoicesWindow) {
					Close_Choices();
					break;
				}
				if (block[0] == MessageWindow) {
					Message_Button(-1);
					break;
				}
				regs.r[1] = (int)block;
				Swi(Wimp_CloseWindow, regs);
				break;
			case 8:	// Key_Pressed
				if (block[0] == MessageWindow && (block[6] == 13 || block[6] == 27)) {
					Message_Button(block[6] == 13 ? ButtonCount - 1 : -1);
					break;
				}
				regs.r[0] = block[6];
				Swi(Wimp_ProcessKey, regs);
				break;
			case 6:	// Mouse_Click
				if (block[3] == MessageWindow && block[4] >= 0 && block[4] < ButtonCount && (block[2] & (4 | 1))) {
					Message_Button(block[4]);
					break;
				}
				if (block[3] == ChoicesWindow && ChoicesWindow >= 0) {
					Choices_Click(block[4], block[2]);
					break;
				}
				if (block[3] == -2 && block[4] == IconHandle) {
					if (block[2] & 2) {
						Open_Menu(block[0]);
					} else if (block[2] & (4 | 1)) {
						Icon_Clicked();
					}
				}
				break;
			case 9:	// Menu_Selection
				switch (block[0]) {
					case MenuQuit:
						Send_Command("stop");	// the game too
						return;
					case MenuHelp:
						Open_Help();
						break;
					case MenuChoices:
						Open_Choices();
						break;
					case MenuSave:
						Send_Command("save");
						break;
					case MenuLoad:
						Send_Command("load");
						break;
					case MenuSlot:
						if (block[1] >= 0 && block[1] <= 9) {
							char command[8];
							snprintf(command, sizeof(command), "slot %d", block[1]);
							Send_Command(command);
						}
						break;
					case MenuFullScreen:
						Send_Command("fullscreen");
						break;
					case MenuPause:
						Send_Command("pause");
						break;
					case MenuStop:
						Send_Command("stop");
						break;
					default:
						break;
				}
				{
					int pointer[5];
					regs.r[1] = (int)pointer;
					if (Swi(Wimp_GetPointerInfo, regs) && (pointer[2] & 1)) {
						Open_Menu(pointer[0] + 64);
					}
				}
				break;
			case 17: case 18:	// User_Message(_Recorded)
				if (block[4] == 0) {	// Message_Quit
					return;
				}
				if (block[4] == 3 && block[5] == -2 && block[6] == IconHandle) {	// Message_DataLoad
					Data_Load(block);
				}
				if (block[4] == 0x400C3 && GameTask != 0 && block[1] == GameTask) {	// Message_TaskCloseDown
					fprintf(stderr, "launcher: the game has finished\n");
					GameTask = 0;
					Set_Variable("Mupen64Plus$Command", nullptr);
					Set_Variable("Mupen64Plus$GameState", nullptr);
					if (!PendingSettings.empty()) {	// Choices saved while it ran
						if (!Write_Settings(PendingSettings)) {
							Tell("The settings file, <Choices$Write>.Mupen64Plus.mupen64plus/cfg, couldn't be "
								"written.", true);
						}
						PendingSettings.clear();
					}
					if (IconHandle < 0) {
						Put_Icon_On_Bar();
					}
				}
				break;
			default:
				break;
		}
	}
}

}	// namespace

int main(int argc, char ** argv)
{
	int messages[] = {3, 0x400C3, 0};
	_kernel_swi_regs regs;
	regs.r[0] = 380;
	regs.r[1] = 0x4B534154;
	regs.r[2] = (int)AppName;
	regs.r[3] = (int)messages;
	if (!Swi(Wimp_Initialise, regs)) {
		return 1;
	}
	TaskHandle = regs.r[1];
	Create_Info_Window();
	Build_Menu();
	Put_Icon_On_Bar();
	fprintf(stderr, "launcher: %s on the icon bar\n", LAUNCHER_VERSION);
	// a ROM given to !Run (as by a double-click, once ROMs have a file type): play it
	if (argc > 1) {
		Play(argv[1]);
	}
	Poll_Loop();
	regs.r[0] = TaskHandle;
	regs.r[1] = 0x4B534154;
	_kernel_swi(Wimp_CloseDown, &regs, &regs);
	return 0;
}
