/*
 * Dante CLI — Win32 native clone of the SwiftUI macOS app.
 *
 * Single-file C application. No Qt, no external libraries — pure Win32 +
 * GDI + ConPTY. Cross-compiles with mingw-w64 from macOS or builds with
 * MSVC on Windows.
 *
 * Layout (matches the SwiftUI version):
 *
 *   +-----------------------------------------------------------+
 *   | Sidebar |   TabBar      [+]    AI launchers       Stats   |  toolbar row
 *   |   ★ 📁  +---------------------------------------+
 *   |   ⚡ 🔑 |                                       |
 *   |--------|                                       |
 *   |Search… |    Terminal area (ConPTY pipe         |
 *   |        |    rendered into a cell grid with GDI)|
 *   | Items… |                                       |
 *   |        |                                       |
 *   +-----------------------------------------------------------+
 *   | Dante CLI 1.0.x · PowerShell · session 1 · cwd ~          |  status bar
 *   +-----------------------------------------------------------+
 *
 * Build:
 *   x86_64-w64-mingw32-windres dante_cli_bootstrap.rc -O coff -o res.o
 *   x86_64-w64-mingw32-gcc -O2 -s -municode -mwindows \
 *       -DUNICODE -D_UNICODE -Wall -Wextra \
 *       -o "Dante CLI.exe" dante_app.c res.o \
 *       -static -static-libgcc \
 *       -lcomctl32 -lshell32 -luser32 -lgdi32 -lkernel32 -lpsapi
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <psapi.h>
#include <winhttp.h>
#include <mmsystem.h>
#include <wincrypt.h>
#include <dpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <wchar.h>

/* ConPTY APIs were added in Windows 10 1809 (build 17763). Some older mingw
 * headers don't declare them — provide local prototypes when missing.    */
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

#ifndef IDC_STATIC
#define IDC_STATIC ((int)-1)
#endif

#define UNUSED(x) ((void)(x))

typedef VOID* HPCON;
typedef HRESULT (WINAPI *PFN_CreatePseudoConsole)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI *PFN_ResizePseudoConsole)(HPCON, COORD);
typedef VOID    (WINAPI *PFN_ClosePseudoConsole)(HPCON);

static PFN_CreatePseudoConsole pCreatePseudoConsole = NULL;
static PFN_ResizePseudoConsole pResizePseudoConsole = NULL;
static PFN_ClosePseudoConsole  pClosePseudoConsole  = NULL;

/* =========================================================================
 *                              CONSTANTS
 * ========================================================================= */

#define APP_VERSION_W   L"1.0.29"
#define APP_NAME_W      L"Dante CLI"
#define APP_WINDOW_CLS  L"DanteCLIMainWindow"

#define SIDEBAR_W       240
#define TABBAR_H        38
#define TOOLBAR_AI_H    36
#define STATUSBAR_H     26
#define SIDEBAR_HDR_H   44
#define CELL_PAD_X      2
#define MAX_TABS        32
#define MAX_FAVORITES   64
#define MAX_SNIPPETS    64
#define MAX_CREDS       64
#define MAX_COLS        400
#define MAX_ROWS        200
#define SCROLLBACK_CAP  5000
#define TERM_HEADER_H   38

/* Tokyo Night palette — matches the Swift default theme. */
#define COL_BG          RGB(0x1A, 0x1B, 0x26)
#define COL_BG_SIDE     RGB(0x16, 0x16, 0x1E)
#define COL_BG_TABBAR   RGB(0x1A, 0x1B, 0x26)
#define COL_BG_TERM     RGB(0x1A, 0x1B, 0x26)
#define COL_BG_STATUS   RGB(0x16, 0x16, 0x1E)
#define COL_DIV         RGB(0x29, 0x2E, 0x42)
#define COL_FG          RGB(0xC0, 0xCA, 0xF5)
#define COL_FG_DIM      RGB(0x56, 0x5F, 0x89)
#define COL_ACCENT      RGB(0x7A, 0xA2, 0xF7)
#define COL_ACCENT2     RGB(0xBB, 0x9A, 0xF7)
#define COL_GREEN       RGB(0x9E, 0xCE, 0x6A)
#define COL_ORANGE      RGB(0xFF, 0x9E, 0x64)
#define COL_RED         RGB(0xF7, 0x76, 0x8E)
#define COL_YELLOW      RGB(0xE0, 0xAF, 0x68)
#define COL_CYAN        RGB(0x7D, 0xCF, 0xFF)
#define COL_MAGENTA     RGB(0xBB, 0x9A, 0xF7)
#define COL_BG_CHIP     RGB(0x24, 0x28, 0x3B)
#define COL_BG_CHIP_H   RGB(0x32, 0x38, 0x52)
#define COL_TAB_ACTIVE  RGB(0x7A, 0xA2, 0xF7)

/* Terminal color schemes — 19 named themes matching the Swift app. */
typedef struct {
    const wchar_t* id;
    const wchar_t* displayName;
    COLORREF bg;
    COLORREF fg;
    COLORREF cursor;
    COLORREF ansi[16];
} ColorScheme;

static const ColorScheme kSchemes[] = {
    { L"tokyo-night", L"Tokyo Night",
        RGB(0x1A,0x1B,0x26), RGB(0xC0,0xCA,0xF5), RGB(0x7A,0xA2,0xF7), {
        RGB(0x15,0x16,0x1E), RGB(0xF7,0x76,0x8E), RGB(0x9E,0xCE,0x6A), RGB(0xE0,0xAF,0x68),
        RGB(0x7A,0xA2,0xF7), RGB(0xBB,0x9A,0xF7), RGB(0x7D,0xCF,0xFF), RGB(0xA9,0xB1,0xD6),
        RGB(0x41,0x48,0x68), RGB(0xF7,0x76,0x8E), RGB(0x9E,0xCE,0x6A), RGB(0xE0,0xAF,0x68),
        RGB(0x7A,0xA2,0xF7), RGB(0xBB,0x9A,0xF7), RGB(0x7D,0xCF,0xFF), RGB(0xC0,0xCA,0xF5)} },
    { L"dracula", L"Dracula",
        RGB(0x28,0x2A,0x36), RGB(0xF8,0xF8,0xF2), RGB(0xFF,0x79,0xC6), {
        RGB(0x21,0x22,0x2C), RGB(0xFF,0x55,0x55), RGB(0x50,0xFA,0x7B), RGB(0xF1,0xFA,0x8C),
        RGB(0xBD,0x93,0xF9), RGB(0xFF,0x79,0xC6), RGB(0x8B,0xE9,0xFD), RGB(0xF8,0xF8,0xF2),
        RGB(0x62,0x72,0xA4), RGB(0xFF,0x6E,0x6E), RGB(0x69,0xFF,0x94), RGB(0xFF,0xFF,0xA5),
        RGB(0xD6,0xAC,0xFF), RGB(0xFF,0x92,0xDF), RGB(0xA4,0xFF,0xFF), RGB(0xFF,0xFF,0xFF)} },
    { L"nord", L"Nord",
        RGB(0x2E,0x34,0x40), RGB(0xD8,0xDE,0xE9), RGB(0x88,0xC0,0xD0), {
        RGB(0x3B,0x42,0x52), RGB(0xBF,0x61,0x6A), RGB(0xA3,0xBE,0x8C), RGB(0xEB,0xCB,0x8B),
        RGB(0x81,0xA1,0xC1), RGB(0xB4,0x8E,0xAD), RGB(0x88,0xC0,0xD0), RGB(0xE5,0xE9,0xF0),
        RGB(0x4C,0x56,0x6A), RGB(0xBF,0x61,0x6A), RGB(0xA3,0xBE,0x8C), RGB(0xEB,0xCB,0x8B),
        RGB(0x81,0xA1,0xC1), RGB(0xB4,0x8E,0xAD), RGB(0x8F,0xBC,0xBB), RGB(0xEC,0xEF,0xF4)} },
    { L"one-dark", L"One Dark",
        RGB(0x28,0x2C,0x34), RGB(0xAB,0xB2,0xBF), RGB(0x52,0x8B,0xFF), {
        RGB(0x00,0x00,0x00), RGB(0xE0,0x6C,0x75), RGB(0x98,0xC3,0x79), RGB(0xE5,0xC0,0x7B),
        RGB(0x61,0xAF,0xEF), RGB(0xC6,0x78,0xDD), RGB(0x56,0xB6,0xC2), RGB(0xAB,0xB2,0xBF),
        RGB(0x5C,0x63,0x70), RGB(0xE0,0x6C,0x75), RGB(0x98,0xC3,0x79), RGB(0xE5,0xC0,0x7B),
        RGB(0x61,0xAF,0xEF), RGB(0xC6,0x78,0xDD), RGB(0x56,0xB6,0xC2), RGB(0xFF,0xFF,0xFF)} },
    { L"solarized-dark", L"Solarized Dark",
        RGB(0x00,0x2B,0x36), RGB(0x83,0x94,0x96), RGB(0x93,0xA1,0xA1), {
        RGB(0x07,0x36,0x42), RGB(0xDC,0x32,0x2F), RGB(0x85,0x99,0x00), RGB(0xB5,0x89,0x00),
        RGB(0x26,0x8B,0xD2), RGB(0xD3,0x36,0x82), RGB(0x2A,0xA1,0x98), RGB(0xEE,0xE8,0xD5),
        RGB(0x58,0x6E,0x75), RGB(0xCB,0x4B,0x16), RGB(0x58,0x6E,0x75), RGB(0x65,0x7B,0x83),
        RGB(0x83,0x94,0x96), RGB(0x6C,0x71,0xC4), RGB(0x93,0xA1,0xA1), RGB(0xFD,0xF6,0xE3)} },
    { L"solarized-light", L"Solarized Light",
        RGB(0xFD,0xF6,0xE3), RGB(0x65,0x7B,0x83), RGB(0x58,0x6E,0x75), {
        RGB(0xEE,0xE8,0xD5), RGB(0xDC,0x32,0x2F), RGB(0x85,0x99,0x00), RGB(0xB5,0x89,0x00),
        RGB(0x26,0x8B,0xD2), RGB(0xD3,0x36,0x82), RGB(0x2A,0xA1,0x98), RGB(0x07,0x36,0x42),
        RGB(0xFD,0xF6,0xE3), RGB(0xCB,0x4B,0x16), RGB(0x93,0xA1,0xA1), RGB(0x83,0x94,0x96),
        RGB(0x65,0x7B,0x83), RGB(0x6C,0x71,0xC4), RGB(0x58,0x6E,0x75), RGB(0x00,0x2B,0x36)} },
    { L"gruvbox-dark", L"Gruvbox Dark",
        RGB(0x28,0x28,0x28), RGB(0xEB,0xDB,0xB2), RGB(0xFE,0x80,0x19), {
        RGB(0x28,0x28,0x28), RGB(0xCC,0x24,0x1D), RGB(0x98,0x97,0x1A), RGB(0xD7,0x99,0x21),
        RGB(0x45,0x85,0x88), RGB(0xB1,0x62,0x86), RGB(0x68,0x9D,0x6A), RGB(0xA8,0x99,0x84),
        RGB(0x92,0x83,0x74), RGB(0xFB,0x49,0x34), RGB(0xB8,0xBB,0x26), RGB(0xFA,0xBD,0x2F),
        RGB(0x83,0xA5,0x98), RGB(0xD3,0x86,0x9B), RGB(0x8E,0xC0,0x7C), RGB(0xEB,0xDB,0xB2)} },
    { L"monokai", L"Monokai",
        RGB(0x27,0x28,0x22), RGB(0xF8,0xF8,0xF2), RGB(0xF8,0xF8,0xF0), {
        RGB(0x27,0x28,0x22), RGB(0xF9,0x26,0x72), RGB(0xA6,0xE2,0x2E), RGB(0xF4,0xBF,0x75),
        RGB(0x66,0xD9,0xEF), RGB(0xAE,0x81,0xFF), RGB(0xA1,0xEF,0xE4), RGB(0xF8,0xF8,0xF2),
        RGB(0x75,0x71,0x5E), RGB(0xF9,0x26,0x72), RGB(0xA6,0xE2,0x2E), RGB(0xF4,0xBF,0x75),
        RGB(0x66,0xD9,0xEF), RGB(0xAE,0x81,0xFF), RGB(0xA1,0xEF,0xE4), RGB(0xF9,0xF8,0xF5)} },
    { L"catppuccin-mocha", L"Catppuccin Mocha",
        RGB(0x1E,0x1E,0x2E), RGB(0xCD,0xD6,0xF4), RGB(0xF5,0xE0,0xDC), {
        RGB(0x45,0x47,0x5A), RGB(0xF3,0x8B,0xA8), RGB(0xA6,0xE3,0xA1), RGB(0xF9,0xE2,0xAF),
        RGB(0x89,0xB4,0xFA), RGB(0xF5,0xC2,0xE7), RGB(0x94,0xE2,0xD5), RGB(0xBA,0xC2,0xDE),
        RGB(0x58,0x5B,0x70), RGB(0xF3,0x8B,0xA8), RGB(0xA6,0xE3,0xA1), RGB(0xF9,0xE2,0xAF),
        RGB(0x89,0xB4,0xFA), RGB(0xF5,0xC2,0xE7), RGB(0x94,0xE2,0xD5), RGB(0xA6,0xAD,0xC8)} },
    { L"github-dark", L"GitHub Dark",
        RGB(0x0D,0x11,0x17), RGB(0xC9,0xD1,0xD9), RGB(0x58,0xA6,0xFF), {
        RGB(0x48,0x4F,0x58), RGB(0xFF,0x7B,0x72), RGB(0x3F,0xB9,0x50), RGB(0xD2,0x99,0x22),
        RGB(0x58,0xA6,0xFF), RGB(0xBC,0x8C,0xFF), RGB(0x39,0xC5,0xCF), RGB(0xB1,0xBA,0xC4),
        RGB(0x6E,0x76,0x81), RGB(0xFF,0xA1,0x98), RGB(0x56,0xD3,0x64), RGB(0xE3,0xB3,0x41),
        RGB(0x79,0xC0,0xFF), RGB(0xD2,0xA8,0xFF), RGB(0x56,0xD4,0xDD), RGB(0xF0,0xF6,0xFC)} },
    { L"material-dark", L"Material Dark",
        RGB(0x21,0x21,0x21), RGB(0xEE,0xFF,0xFF), RGB(0xFF,0xCB,0x6B), {
        RGB(0x00,0x00,0x00), RGB(0xF0,0x71,0x78), RGB(0xC3,0xE8,0x8D), RGB(0xFF,0xCB,0x6B),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x89,0xDD,0xFF), RGB(0xEE,0xFF,0xFF),
        RGB(0x54,0x54,0x54), RGB(0xFF,0x53,0x70), RGB(0xC3,0xE8,0x8D), RGB(0xFF,0xCB,0x6B),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x89,0xDD,0xFF), RGB(0xFF,0xFF,0xFF)} },
    { L"night-owl", L"Night Owl",
        RGB(0x01,0x16,0x27), RGB(0xD6,0xDE,0xEB), RGB(0x80,0xA4,0xC2), {
        RGB(0x01,0x16,0x27), RGB(0xEF,0x53,0x50), RGB(0x22,0xDA,0x6E), RGB(0xC5,0xE4,0x78),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x7F,0xDB,0xCA), RGB(0xFF,0xFF,0xFF),
        RGB(0x57,0x5B,0x66), RGB(0xEF,0x53,0x50), RGB(0x22,0xDA,0x6E), RGB(0xFF,0xEB,0x95),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x7F,0xDB,0xCA), RGB(0xFF,0xFF,0xFF)} },
    { L"synthwave84", L"Synthwave '84",
        RGB(0x26,0x21,0x35), RGB(0xFF,0x7E,0xDB), RGB(0xF9,0x7E,0x72), {
        RGB(0x26,0x21,0x35), RGB(0xF9,0x7E,0x72), RGB(0x72,0xF1,0xB8), RGB(0xFE,0xDE,0x5D),
        RGB(0x36,0xF9,0xF6), RGB(0xFF,0x7E,0xDB), RGB(0x36,0xF9,0xF6), RGB(0xFF,0xFF,0xFF),
        RGB(0x49,0x5E,0x80), RGB(0xF9,0x7E,0x72), RGB(0x72,0xF1,0xB8), RGB(0xFE,0xDE,0x5D),
        RGB(0x36,0xF9,0xF6), RGB(0xFF,0x7E,0xDB), RGB(0x36,0xF9,0xF6), RGB(0xFF,0xFF,0xFF)} },
    { L"cobalt", L"Cobalt",
        RGB(0x00,0x2B,0x4F), RGB(0xFF,0xFF,0xFF), RGB(0xFF,0xAB,0x00), {
        RGB(0x00,0x29,0x4D), RGB(0xFF,0x26,0x00), RGB(0x3A,0xD9,0x00), RGB(0xFF,0xC6,0x00),
        RGB(0x00,0x88,0xFF), RGB(0xFB,0x94,0xFF), RGB(0x00,0xD8,0xFF), RGB(0xFF,0xFF,0xFF),
        RGB(0x80,0x9D,0xB8), RGB(0xFF,0x26,0x00), RGB(0x3A,0xD9,0x00), RGB(0xFF,0xC6,0x00),
        RGB(0x00,0x88,0xFF), RGB(0xFB,0x94,0xFF), RGB(0x00,0xD8,0xFF), RGB(0xFF,0xFF,0xFF)} },
    { L"palenight", L"Palenight",
        RGB(0x29,0x2D,0x3E), RGB(0xC0,0xCA,0xF5), RGB(0xA6,0xAC,0xCD), {
        RGB(0x29,0x2D,0x3E), RGB(0xE3,0x4D,0x4D), RGB(0xC3,0xE8,0x8D), RGB(0xFF,0xCB,0x6B),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x89,0xDD,0xFF), RGB(0xFF,0xFF,0xFF),
        RGB(0x67,0x6E,0x95), RGB(0xFF,0x59,0x70), RGB(0xC3,0xE8,0x8D), RGB(0xFF,0xCB,0x6B),
        RGB(0x82,0xAA,0xFF), RGB(0xC7,0x92,0xEA), RGB(0x89,0xDD,0xFF), RGB(0xFF,0xFF,0xFF)} },
    { L"gruvbox-light", L"Gruvbox Light",
        RGB(0xFB,0xF1,0xC7), RGB(0x3C,0x38,0x36), RGB(0xAF,0x3A,0x03), {
        RGB(0xFB,0xF1,0xC7), RGB(0xCC,0x24,0x1D), RGB(0x79,0x74,0x0E), RGB(0xB5,0x76,0x14),
        RGB(0x07,0x66,0x78), RGB(0x8F,0x3F,0x71), RGB(0x42,0x7B,0x58), RGB(0x7C,0x6F,0x64),
        RGB(0x92,0x83,0x74), RGB(0x9D,0x00,0x06), RGB(0x79,0x74,0x0E), RGB(0xB5,0x76,0x14),
        RGB(0x07,0x66,0x78), RGB(0x8F,0x3F,0x71), RGB(0x42,0x7B,0x58), RGB(0x3C,0x38,0x36)} },
    { L"github-light", L"GitHub Light",
        RGB(0xFF,0xFF,0xFF), RGB(0x24,0x29,0x2F), RGB(0x05,0x69,0xD1), {
        RGB(0x24,0x29,0x2F), RGB(0xCF,0x22,0x2E), RGB(0x11,0x6B,0x29), RGB(0x95,0x3B,0x00),
        RGB(0x05,0x69,0xD1), RGB(0x83,0x25,0xC5), RGB(0x1B,0x7C,0x83), RGB(0x6E,0x77,0x81),
        RGB(0x57,0x60,0x6A), RGB(0xA4,0x05,0x10), RGB(0x11,0x6B,0x29), RGB(0x95,0x3B,0x00),
        RGB(0x05,0x69,0xD1), RGB(0x83,0x25,0xC5), RGB(0x1B,0x7C,0x83), RGB(0x24,0x29,0x2F)} },
    { L"apple-classic", L"Apple Classic",
        RGB(0x00,0x00,0x00), RGB(0xB6,0xB6,0xB6), RGB(0xC2,0xC2,0xC2), {
        RGB(0x00,0x00,0x00), RGB(0xC9,0x1B,0x00), RGB(0x00,0xC2,0x00), RGB(0xC7,0xC4,0x00),
        RGB(0x02,0x25,0xC7), RGB(0xC9,0x30,0xC7), RGB(0x00,0xC5,0xC7), RGB(0xC7,0xC7,0xC7),
        RGB(0x67,0x67,0x67), RGB(0xFF,0x6D,0x67), RGB(0x5F,0xF9,0x67), RGB(0xFE,0xFB,0x67),
        RGB(0x67,0x71,0xFF), RGB(0xFF,0x76,0xFF), RGB(0x5F,0xFD,0xFF), RGB(0xFF,0xFF,0xFF)} },
    { L"catppuccin-latte", L"Catppuccin Latte",
        RGB(0xEF,0xF1,0xF5), RGB(0x4C,0x4F,0x69), RGB(0xDC,0x8A,0x78), {
        RGB(0x5C,0x5F,0x77), RGB(0xD2,0x0F,0x39), RGB(0x40,0xA0,0x2B), RGB(0xDF,0x8E,0x1D),
        RGB(0x1E,0x66,0xF5), RGB(0xEA,0x76,0xCB), RGB(0x17,0x9D,0x9F), RGB(0xAC,0xB0,0xBE),
        RGB(0x6C,0x6F,0x85), RGB(0xD2,0x0F,0x39), RGB(0x40,0xA0,0x2B), RGB(0xDF,0x8E,0x1D),
        RGB(0x1E,0x66,0xF5), RGB(0xEA,0x76,0xCB), RGB(0x17,0x9D,0x9F), RGB(0x4C,0x4F,0x69)} },
};

#define SCHEME_COUNT ((int)(sizeof(kSchemes) / sizeof(kSchemes[0])))

static int g_schemeIdx = 0;  /* index into kSchemes; default Tokyo Night */

static const ColorScheme* current_scheme(void) {
    if (g_schemeIdx < 0 || g_schemeIdx >= SCHEME_COUNT) return &kSchemes[0];
    return &kSchemes[g_schemeIdx];
}

/* Resolve scheme by string id (case-insensitive). NULL if not found. */
static const ColorScheme* scheme_by_id(const wchar_t* id) {
    if (!id || !id[0]) return NULL;
    for (int i = 0; i < SCHEME_COUNT; ++i) {
        if (_wcsicmp(kSchemes[i].id, id) == 0) return &kSchemes[i];
    }
    return NULL;
}

#define kAnsiPalette (current_scheme()->ansi)

/* Tab chip colors (Apple-style, 12 solid). */
static const COLORREF kTabColors[12] = {
    RGB(0x8E, 0x8E, 0x93), /* neutral */
    RGB(0xFF, 0x45, 0x3A), /* red     */
    RGB(0xFF, 0x9F, 0x0A), /* orange  */
    RGB(0xFF, 0xD6, 0x0A), /* yellow  */
    RGB(0x30, 0xD1, 0x58), /* green   */
    RGB(0x63, 0xE6, 0xBE), /* mint    */
    RGB(0x64, 0xD2, 0xFF), /* cyan    */
    RGB(0x0A, 0x84, 0xFF), /* blue    */
    RGB(0x5E, 0x5C, 0xE6), /* indigo  */
    RGB(0xBF, 0x5A, 0xF2), /* purple  */
    RGB(0xFF, 0x37, 0x5F), /* pink    */
    RGB(0xAC, 0x8E, 0x68), /* brown   */
};

/* Split workspace — preset library with arbitrary rectangular cells laid out
 * in percentage coordinates (0..100) of the terminal area. Each preset has
 * a category for grouping in the popup menu. Up to 9 cells (Grid 3×3).    */

#define MAX_SPLIT_CELLS 9

typedef struct { short x, y, w, h; } SplitCell;

typedef enum {
    SCAT_SIMPLE = 0,
    SCAT_DASHBOARD,
    SCAT_IDE,
    SCAT_OPS,
    SCAT_COUNT
} SplitCategory;

static const wchar_t* kSplitCategoryNames[SCAT_COUNT] = {
    L"Simples", L"Dashboard", L"IDE", L"Operações"
};

typedef struct {
    const wchar_t* name;
    SplitCategory  category;
    int            cellCount;
    SplitCell      cells[MAX_SPLIT_CELLS];
} SplitPreset;

static const SplitPreset kPresets[] = {
    /* ── Simples ───────────────────────────────────────────────────────── */
    { L"Sem split (uma aba)",       SCAT_SIMPLE, 1, {
        {0,0,100,100} } },
    { L"Lado a lado (2)",           SCAT_SIMPLE, 2, {
        {0,0,50,100}, {50,0,50,100} } },
    { L"Empilhados (2)",            SCAT_SIMPLE, 2, {
        {0,0,100,50}, {0,50,100,50} } },
    { L"3 colunas",                 SCAT_SIMPLE, 3, {
        {0,0,33,100}, {33,0,34,100}, {67,0,33,100} } },
    { L"3 empilhados",              SCAT_SIMPLE, 3, {
        {0,0,100,33}, {0,33,100,34}, {0,67,100,33} } },
    { L"Quartos 2×2",               SCAT_SIMPLE, 4, {
        {0,0,50,50},  {50,0,50,50},
        {0,50,50,50}, {50,50,50,50} } },

    /* ── Dashboard ─────────────────────────────────────────────────────── */
    { L"Header + 3 colunas",        SCAT_DASHBOARD, 4, {
        {0,0,100,40},
        {0,40,33,60}, {33,40,34,60}, {67,40,33,60} } },
    { L"Cinema 1+4",                SCAT_DASHBOARD, 5, {
        {0,0,100,55},
        {0,55,25,45}, {25,55,25,45}, {50,55,25,45}, {75,55,25,45} } },
    { L"Mosaico 2+1",               SCAT_DASHBOARD, 3, {
        {0,0,50,50},  {0,50,50,50},
        {50,0,50,100} } },
    { L"Grid 3×2",                  SCAT_DASHBOARD, 6, {
        {0,0,33,50},   {33,0,34,50},   {67,0,33,50},
        {0,50,33,50},  {33,50,34,50},  {67,50,33,50} } },
    { L"Grid 3×3",                  SCAT_DASHBOARD, 9, {
        {0,0,33,33},   {33,0,34,33},   {67,0,33,33},
        {0,33,33,34},  {33,33,34,34},  {67,33,33,34},
        {0,67,33,33},  {33,67,34,33},  {67,67,33,33} } },

    /* ── IDE ───────────────────────────────────────────────────────────── */
    { L"Editor + Terminal",         SCAT_IDE, 2, {
        {0,0,100,70}, {0,70,100,30} } },
    { L"Editor + Sidebar",          SCAT_IDE, 2, {
        {0,0,35,100}, {35,0,65,100} } },
    { L"Editor + 2 painéis direita", SCAT_IDE, 3, {
        {0,0,60,100},
        {60,0,40,50}, {60,50,40,50} } },
    { L"Editor + Terminal + Logs",  SCAT_IDE, 3, {
        {0,0,60,60},   {60,0,40,60},
        {0,60,100,40} } },
    { L"Sidebar + Main + Inspector", SCAT_IDE, 3, {
        {0,0,22,100}, {22,0,56,100}, {78,0,22,100} } },
    { L"Master / Detail",           SCAT_IDE, 2, {
        {0,0,40,100}, {40,0,60,100} } },
    { L"Foco esquerdo 80%",         SCAT_IDE, 2, {
        {0,0,80,100}, {80,0,20,100} } },

    /* ── Operações ─────────────────────────────────────────────────────── */
    { L"Main + 4 painéis lateral",  SCAT_OPS, 5, {
        {0,0,75,100},
        {75,0,25,25},  {75,25,25,25}, {75,50,25,25}, {75,75,25,25} } },
    { L"Quadrante + faixas",        SCAT_OPS, 3, {
        {0,0,60,100},
        {60,0,40,50}, {60,50,40,50} } },
    { L"Editor + 4 logs",           SCAT_OPS, 5, {
        {0,0,60,100},
        {60,0,40,25},  {60,25,40,25}, {60,50,40,25}, {60,75,40,25} } },
};

#define PRESET_COUNT  ((int)(sizeof(kPresets) / sizeof(kPresets[0])))
#define PRESET_SINGLE 0  /* index 0 is always the single-pane layout */

/* Sidebar mode tabs. */
typedef enum {
    MODE_FAVORITES = 0,
    MODE_FILES     = 1,
    MODE_SNIPPETS  = 2,
    MODE_CREDS     = 3,
    MODE_COUNT
} SidebarMode;

static const wchar_t* kSidebarModeIcons[MODE_COUNT] = {
    L"★",   /* ★ */
    L"\U0001F4C1", /* 📁 */
    L"⚡",   /* ⚡ */
    L"\U0001F511", /* 🔑 */
};
static const wchar_t* kSidebarModeLabels[MODE_COUNT] = {
    L"Favoritos", L"Pastas", L"Snippets", L"Chaves",
};

/* =========================================================================
 *                         TERMINAL CELL GRID
 * ========================================================================= */

typedef struct {
    wchar_t ch;
    uint16_t attr;     /* bit flags */
    uint8_t  fgIdx;    /* index into ANSI 16 or extended */
    uint8_t  bgIdx;
    COLORREF fgRgb;    /* used when attr has ATTR_FG_RGB */
    COLORREF bgRgb;    /* used when attr has ATTR_BG_RGB */
} Cell;

#define ATTR_BOLD       0x0001
#define ATTR_ITALIC     0x0002
#define ATTR_UNDERLINE  0x0004
#define ATTR_REVERSE    0x0008
#define ATTR_FG_RGB     0x0010
#define ATTR_BG_RGB     0x0020
#define ATTR_FG_DEFAULT 0x0040
#define ATTR_BG_DEFAULT 0x0080

typedef struct {
    Cell* cells;      /* cols * rows */
    int cols;
    int rows;
    int cursorRow;
    int cursorCol;
    Cell currentAttr;
    int dirty;
    /* Scrollback */
    Cell* scrollback; /* cols * SCROLLBACK_CAP */
    int scrollbackLines;
} TerminalGrid;

/* =========================================================================
 *                          ANSI PARSER STATE
 * ========================================================================= */

typedef enum {
    AP_GROUND,
    AP_ESC,
    AP_CSI,
    AP_OSC,
    AP_UTF8,
} ApState;

typedef struct {
    ApState state;
    char paramBuf[64];
    int  paramLen;
    char oscBuf[512];
    int  oscLen;
    /* UTF-8 multibyte assembly */
    uint32_t utf8Code;
    int utf8Remaining;
} AnsiParser;

/* =========================================================================
 *                              SESSION
 * ========================================================================= */

typedef struct {
    HPCON  hPC;
    HANDLE hPipeIn;     /* we write child stdin */
    HANDLE hPipeOut;    /* we read child stdout */
    HANDLE hChildIn;
    HANDLE hChildOut;
    HANDLE hProcess;
    HANDLE hThread;
    HANDLE hJob;
    HANDLE hReaderThread;
    /* alive is set/cleared across threads. volatile prevents the reader
     * loop from caching it in a register, and InterlockedExchange around
     * the writes makes the transition observable on every platform.    */
    volatile LONG alive;
    DWORD  pid;
    int    ownerTabId;   /* stable id of the Tab owning this session — used in
                          * cross-thread output messages so we never deref
                          * potentially-freed Session* in the UI thread.    */
    wchar_t shellName[64];
    wchar_t cwd[MAX_PATH];
} Session;

typedef struct {
    int          id;
    wchar_t      title[128];
    wchar_t      emoji[8];
    int          colorIdx;   /* index into kTabColors, -1 = none */
    BOOL         pinned;
    Session*     session;
    TerminalGrid grid;
    AnsiParser   parser;
    /* Cached metrics refreshed by resmon_sample() on the 1 s timer. The
     * header paints from these — OpenProcess on every repaint was burning
     * ~540 syscalls/s in Grid 3×3.                                       */
    SIZE_T       cachedMemBytes;
    double       cachedCpuPct;
    /* Per-tab appearance override. Empty/0 = inherit global Settings. */
    wchar_t      customScheme[32];
    int          customFontSize;
} Tab;

/* =========================================================================
 *                            APP STATE
 * ========================================================================= */

typedef struct { wchar_t name[128]; wchar_t path[MAX_PATH]; wchar_t emoji[8]; } Favorite;
typedef struct { wchar_t name[128]; wchar_t cmd[512];     wchar_t emoji[8]; } Snippet;
typedef struct { wchar_t name[128]; wchar_t kind[16];     wchar_t user[64]; wchar_t host[128]; wchar_t emoji[8]; } Credential;

typedef struct {
    HWND     hWnd;
    HWND     hStatus;
    HFONT    hFontUI;
    HFONT    hFontUIBold;
    HFONT    hFontMono;
    HFONT    hFontMonoBold;
    HFONT    hFontEmoji;
    HFONT    hFontBig;
    int      cellW;
    int      cellH;
    Tab*     tabs[MAX_TABS];
    int      tabCount;
    int      activeTab;
    SidebarMode sidebarMode;
    int      tabBarScroll;
    int      tabBarHoverIdx;
    int      sidebarHoverIdx;
    int      sidebarItemHoverIdx;
    Favorite   favorites[MAX_FAVORITES];   int favoriteCount;
    Snippet    snippets[MAX_SNIPPETS];     int snippetCount;
    Credential creds[MAX_CREDS];           int credCount;
    /* Settings */
    wchar_t    groqApiKey[128];
    wchar_t    voiceLang[16];   /* ISO-639 like "pt", "en" */
    wchar_t    updateManifestUrl[512];  /* full HTTPS URL */
    int        fontPxOverride;  /* 0 = default */
    int        scrollbackLines;
    HBRUSH   brBg, brBgSide, brBgTabBar, brBgTerm, brBgStatus;
    HBRUSH   brBgChip, brBgChipH, brAccent;
    HPEN     penDiv, penAccent;
    BOOL     resizeScheduled;
    UINT_PTR resizeTimer;
    UINT_PTR statsTimer;
    UINT_PTR repaintTimer;
    UINT_PTR persistTimer;
    int      pendingRepaint;
    int      pendingPersist;
    /* Drag state for tab reorder */
    int      dragTabIdx;
    int      dragStartX;
    int      dragOffsetX;
    /* Modal state */
    BOOL     cheatsheetVisible;
    /* Sidebar list scroll */
    int      sidebarScroll;
    /* Split workspace */
    int          splitLayout;                       /* index into kPresets[] */
    int          splitSlots[MAX_SPLIT_CELLS];       /* indices into tabs[]; -1 = empty */
    int          splitActiveCell;                   /* which cell currently has focus */
    int          zoomedCell;                        /* -1 = no zoom; cell idx otherwise */
    RECT         zoomBtnRect[MAX_SPLIT_CELLS];      /* updated by draw_terminal_header */
    RECT         sizeBtnRect[MAX_SPLIT_CELLS];      /* resize button rects */
    RECT         unslotBtnRect[MAX_SPLIT_CELLS];    /* "✕" unslot button rects */
    SplitCell    customCells[MAX_SPLIT_CELLS];      /* w==0 → use preset cell */
    /* Files-mode browser state (sidebar MODE_FILES) */
    wchar_t      filesDir[MAX_PATH];                /* current folder path */
    wchar_t      filesEntries[256][MAX_PATH];       /* names of children */
    BOOL         filesIsDir[256];
    int          filesCount;
    int          filesScroll;
} App;

static App g_app;

/* =========================================================================
 *                       FORWARD DECLARATIONS
 * ========================================================================= */

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
static void layout_compute(RECT* outSide, RECT* outTabBar, RECT* outTerm, RECT* outToolbar, RECT* outStatus);
static void terminal_grid_init(TerminalGrid* g, int cols, int rows);
static void terminal_grid_resize(TerminalGrid* g, int cols, int rows);
static void terminal_grid_free(TerminalGrid* g);
static void grid_put_char(TerminalGrid* g, wchar_t ch);
static void grid_line_feed(TerminalGrid* g);
static void grid_carriage_return(TerminalGrid* g);
static void grid_backspace(TerminalGrid* g);
static void grid_tab_advance(TerminalGrid* g);
static void grid_set_cursor(TerminalGrid* g, int row, int col);
static void grid_erase_display(TerminalGrid* g, int mode);
static void grid_erase_line(TerminalGrid* g, int mode);
static void grid_scroll_up(TerminalGrid* g);
static void parser_init(AnsiParser* p);
static void parser_feed(Tab* t, const char* bytes, int n);
static Session* session_create(const wchar_t* shellId, int cols, int rows);
static void session_destroy(Session* s);
static void session_resize(Session* s, int cols, int rows);
static void session_write(Session* s, const void* data, int n);
static DWORD WINAPI reader_thread(LPVOID arg);
static void invalidate_terminal(void);
static void open_new_tab(const wchar_t* shellId);
static void close_tab(int idx);
static void inject_into_active(const wchar_t* text);
static void draw_sidebar(HDC dc, const RECT* rc);
static void draw_tabbar(HDC dc, const RECT* rc);
static void draw_terminal(HDC dc, const RECT* rc);
static void draw_toolbar(HDC dc, const RECT* rc);
static int  hit_test_tab(int x, int y, const RECT* tabBarRc);
static int  hit_test_sidebar_mode(int x, int y, const RECT* sideRc);
static int  hit_test_sidebar_item(int x, int y, const RECT* sideRc);
static int  hit_test_toolbar_action(int x, int y, const RECT* toolbarRc);
static int  hit_test_tab_close(int x, int y, const RECT* tabBarRc, int tabIdx);
static int  hit_test_new_tab(int x, int y, const RECT* tabBarRc);
static int  hit_test_stats_chip(int x, int y, const RECT* tabBarRc);
static void show_resource_monitor(int anchorX, int anchorY);
static void resmon_sample(void);
static COLORREF mix_color(COLORREF a, COLORREF b, double t);
static COLORREF contrast_text_color(COLORREF c);
static void update_status(void);
static void persist_save(void);
static void persist_load(void);
static void schedule_persist(void);
static void show_tab_context_menu(HWND hWnd, int tabIdx, int x, int y);
static void show_add_dialog(void);
static BOOL prompt_text(const wchar_t* title, const wchar_t* prompt, wchar_t* out, int outCap);
static void cheatsheet_toggle(void);
static void draw_cheatsheet(HDC dc, const RECT* cli);
static void show_settings_dialog(void);
static void show_split_menu(HWND hWnd, int x, int y);
static void set_split_layout(int layoutId);
static void show_layout_gallery(void);
static void draw_terminal_cell(HDC dc, const RECT* rc, int tabIdx, BOOL hasFocus, int cellIdx);
static int  active_tab_index(void);
static void toggle_zoom(int cellIdx);
static int  hit_test_zoom_btn(int x, int y);
static int  hit_test_size_btn(int x, int y);
static int  hit_test_unslot_btn(int x, int y);
static void unslot_cell(int cellIdx);
static void files_refresh(void);
static void files_set_dir(const wchar_t* path);
static void files_navigate_up(void);
static BOOL is_text_file(const wchar_t* path);
static void show_editor_preview(const wchar_t* path);
static const SplitCell* resolve_cell(int idx);
static void show_resize_popup(int cellIdx, int screenX, int screenY);
static RECT split_cell_rect(const RECT* parent, const SplitCell* c);
static wchar_t* utf8_to_w_dup(const char* s, int n);
static char* w_to_utf8_dup(const wchar_t* w);
static void show_about(void);
static void check_for_updates_async(void);
static void fill_rect_color(HDC dc, const RECT* r, COLORREF c);
static void draw_text_w(HDC dc, int x, int y, const wchar_t* s, COLORREF color, HFONT font);
static void draw_rounded_rect(HDC dc, RECT r, COLORREF fill, COLORREF border, int radius);

static int next_tab_id = 1;

/* =========================================================================
 *                             UTILITIES
 * ========================================================================= */

static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }
static int clamp_int(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

static void str_copy_w(wchar_t* dst, size_t cap, const wchar_t* src) {
    if (cap == 0) return;
    size_t i = 0;
    while (i + 1 < cap && src[i]) { dst[i] = src[i]; ++i; }
    dst[i] = 0;
}

static void* xcalloc(size_t n, size_t sz) {
    void* p = calloc(n, sz);
    if (!p) { MessageBoxW(NULL, L"Out of memory", APP_NAME_W, MB_ICONERROR); ExitProcess(2); }
    return p;
}

static wchar_t* expand_env(const wchar_t* src) {
    static wchar_t buf[MAX_PATH * 2];
    ExpandEnvironmentStringsW(src, buf, MAX_PATH * 2);
    return buf;
}

/* =========================================================================
 *                        TERMINAL GRID OPS
 * ========================================================================= */

static Cell make_default_cell(void) {
    Cell c = {0};
    c.ch = L' ';
    c.attr = ATTR_FG_DEFAULT | ATTR_BG_DEFAULT;
    return c;
}

static void terminal_grid_init(TerminalGrid* g, int cols, int rows) {
    g->cols = cols;
    g->rows = rows;
    g->cursorRow = 0;
    g->cursorCol = 0;
    g->currentAttr = make_default_cell();
    g->dirty = 1;
    g->scrollbackLines = 0;
    g->cells = (Cell*)xcalloc(cols * rows, sizeof(Cell));
    g->scrollback = (Cell*)xcalloc(cols * SCROLLBACK_CAP, sizeof(Cell));
    for (int i = 0; i < cols * rows; ++i) g->cells[i] = make_default_cell();
}

static void terminal_grid_free(TerminalGrid* g) {
    free(g->cells); free(g->scrollback);
    g->cells = NULL; g->scrollback = NULL;
}

static void terminal_grid_resize(TerminalGrid* g, int cols, int rows) {
    if (cols <= 0 || rows <= 0) return;
    if (cols == g->cols && rows == g->rows) return;

    Cell* nc = (Cell*)xcalloc(cols * rows, sizeof(Cell));
    for (int i = 0; i < cols * rows; ++i) nc[i] = make_default_cell();

    int copyRows = min_int(g->rows, rows);
    int copyCols = min_int(g->cols, cols);
    for (int r = 0; r < copyRows; ++r) {
        for (int c = 0; c < copyCols; ++c) {
            nc[r * cols + c] = g->cells[r * g->cols + c];
        }
    }

    Cell* nsb = (Cell*)xcalloc(cols * SCROLLBACK_CAP, sizeof(Cell));
    int sbCopyCols = min_int(g->cols, cols);
    for (int r = 0; r < g->scrollbackLines; ++r) {
        for (int c = 0; c < sbCopyCols; ++c) {
            nsb[r * cols + c] = g->scrollback[r * g->cols + c];
        }
        for (int c = sbCopyCols; c < cols; ++c) nsb[r * cols + c] = make_default_cell();
    }

    free(g->cells); free(g->scrollback);
    g->cells = nc; g->scrollback = nsb;
    g->cols = cols; g->rows = rows;
    g->cursorRow = clamp_int(g->cursorRow, 0, rows - 1);
    g->cursorCol = clamp_int(g->cursorCol, 0, cols - 1);
    g->dirty = 1;
}

static void grid_scroll_up(TerminalGrid* g) {
    /* Push the top line into scrollback. */
    if (g->scrollbackLines < SCROLLBACK_CAP) {
        memcpy(&g->scrollback[g->scrollbackLines * g->cols],
               &g->cells[0],
               g->cols * sizeof(Cell));
        g->scrollbackLines++;
    } else {
        memmove(&g->scrollback[0],
                &g->scrollback[g->cols],
                (SCROLLBACK_CAP - 1) * g->cols * sizeof(Cell));
        memcpy(&g->scrollback[(SCROLLBACK_CAP - 1) * g->cols],
               &g->cells[0],
               g->cols * sizeof(Cell));
    }
    memmove(&g->cells[0],
            &g->cells[g->cols],
            (g->rows - 1) * g->cols * sizeof(Cell));
    for (int c = 0; c < g->cols; ++c)
        g->cells[(g->rows - 1) * g->cols + c] = make_default_cell();
    g->dirty = 1;
}

static void grid_put_char(TerminalGrid* g, wchar_t ch) {
    if (g->cursorCol >= g->cols) {
        g->cursorCol = 0;
        g->cursorRow++;
        if (g->cursorRow >= g->rows) { grid_scroll_up(g); g->cursorRow = g->rows - 1; }
    }
    Cell c = g->currentAttr;
    c.ch = ch;
    g->cells[g->cursorRow * g->cols + g->cursorCol] = c;
    g->cursorCol++;
    g->dirty = 1;
}

static void grid_line_feed(TerminalGrid* g) {
    g->cursorRow++;
    if (g->cursorRow >= g->rows) { grid_scroll_up(g); g->cursorRow = g->rows - 1; }
    g->dirty = 1;
}

static void grid_carriage_return(TerminalGrid* g) { g->cursorCol = 0; g->dirty = 1; }

static void grid_backspace(TerminalGrid* g) {
    if (g->cursorCol > 0) g->cursorCol--;
    g->dirty = 1;
}

static void grid_tab_advance(TerminalGrid* g) {
    int next = ((g->cursorCol / 8) + 1) * 8;
    if (next > g->cols - 1) next = g->cols - 1;
    g->cursorCol = next;
    g->dirty = 1;
}

static void grid_set_cursor(TerminalGrid* g, int row, int col) {
    g->cursorRow = clamp_int(row - 1, 0, g->rows - 1);
    g->cursorCol = clamp_int(col - 1, 0, g->cols - 1);
    g->dirty = 1;
}

static void grid_erase_display(TerminalGrid* g, int mode) {
    int from = 0, to = g->rows * g->cols;
    if (mode == 0) from = g->cursorRow * g->cols + g->cursorCol;
    else if (mode == 1) to = g->cursorRow * g->cols + g->cursorCol;
    for (int i = from; i < to; ++i) g->cells[i] = make_default_cell();
    if (mode == 3) g->scrollbackLines = 0;
    g->dirty = 1;
}

static void grid_erase_line(TerminalGrid* g, int mode) {
    int from = 0, to = g->cols;
    if (mode == 0) from = g->cursorCol;
    else if (mode == 1) to = g->cursorCol + 1;
    for (int c = from; c < to; ++c) g->cells[g->cursorRow * g->cols + c] = make_default_cell();
    g->dirty = 1;
}

/* =========================================================================
 *                           ANSI PARSER
 * ========================================================================= */

static void parser_init(AnsiParser* p) {
    memset(p, 0, sizeof(*p));
    p->state = AP_GROUND;
}

static void parse_params(const char* buf, int* out, int* outCount, int max) {
    int count = 0, cur = 0;
    int any = 0;
    for (int i = 0; buf[i] && count < max; ++i) {
        char c = buf[i];
        if (c >= '0' && c <= '9') { cur = cur * 10 + (c - '0'); any = 1; }
        else if (c == ';') { out[count++] = any ? cur : 0; cur = 0; any = 0; }
    }
    if (count < max) out[count++] = any ? cur : 0;
    *outCount = count;
}

static void apply_sgr(Cell* attr, const int* p, int n) {
    if (n == 0 || (n == 1 && p[0] == 0)) { *attr = make_default_cell(); return; }
    for (int i = 0; i < n; ++i) {
        int code = p[i];
        if (code == 0) *attr = make_default_cell();
        else if (code == 1)  attr->attr |= ATTR_BOLD;
        else if (code == 3)  attr->attr |= ATTR_ITALIC;
        else if (code == 4)  attr->attr |= ATTR_UNDERLINE;
        else if (code == 7)  attr->attr |= ATTR_REVERSE;
        else if (code == 22) attr->attr &= ~ATTR_BOLD;
        else if (code == 23) attr->attr &= ~ATTR_ITALIC;
        else if (code == 24) attr->attr &= ~ATTR_UNDERLINE;
        else if (code == 27) attr->attr &= ~ATTR_REVERSE;
        else if (code >= 30 && code <= 37) { attr->fgIdx = (uint8_t)(code - 30); attr->attr &= ~(ATTR_FG_RGB | ATTR_FG_DEFAULT); }
        else if (code == 39) { attr->attr |= ATTR_FG_DEFAULT; attr->attr &= ~ATTR_FG_RGB; }
        else if (code >= 40 && code <= 47) { attr->bgIdx = (uint8_t)(code - 40); attr->attr &= ~(ATTR_BG_RGB | ATTR_BG_DEFAULT); }
        else if (code == 49) { attr->attr |= ATTR_BG_DEFAULT; attr->attr &= ~ATTR_BG_RGB; }
        else if (code >= 90 && code <= 97)   { attr->fgIdx = (uint8_t)(code - 90 + 8); attr->attr &= ~(ATTR_FG_RGB | ATTR_FG_DEFAULT); }
        else if (code >= 100 && code <= 107) { attr->bgIdx = (uint8_t)(code - 100 + 8); attr->attr &= ~(ATTR_BG_RGB | ATTR_BG_DEFAULT); }
        else if ((code == 38 || code == 48) && i + 1 < n) {
            int kind = p[i + 1];
            if (kind == 5 && i + 2 < n) {
                /* 256-indexed — approximate by mapping to nearest of our 16 */
                int idx = p[i + 2];
                if (code == 38) { attr->fgIdx = (uint8_t)(idx & 0x0F); attr->attr &= ~(ATTR_FG_RGB | ATTR_FG_DEFAULT); }
                else            { attr->bgIdx = (uint8_t)(idx & 0x0F); attr->attr &= ~(ATTR_BG_RGB | ATTR_BG_DEFAULT); }
                i += 2;
            } else if (kind == 2 && i + 4 < n) {
                COLORREF c = RGB(p[i + 2] & 0xFF, p[i + 3] & 0xFF, p[i + 4] & 0xFF);
                if (code == 38) { attr->fgRgb = c; attr->attr |= ATTR_FG_RGB; attr->attr &= ~ATTR_FG_DEFAULT; }
                else            { attr->bgRgb = c; attr->attr |= ATTR_BG_RGB; attr->attr &= ~ATTR_BG_DEFAULT; }
                i += 4;
            }
        }
    }
}

static void handle_csi(Tab* t, char final_byte) {
    const char* pbuf = t->parser.paramBuf;
    char prefix = 0;

    /* DEC private-mode sequences have a prefix byte: ? > = <. We capture
     * them but otherwise just absorb the sequence — PowerShell uses them
     * for cursor visibility (?25h/l), alt-screen (?1049h/l), bracketed
     * paste (?2004h/l), focus events (?1004h/l), etc. Without this guard
     * we'd ignore the prefix and re-interpret '25h' as junk text.        */
    if (pbuf[0] == '?' || pbuf[0] == '>' || pbuf[0] == '<' || pbuf[0] == '=') {
        prefix = pbuf[0];
        pbuf++;
    }

    int params[16] = {0};
    int n = 0;
    parse_params(pbuf, params, &n, 16);
    int p0 = (n > 0) ? max_int(params[0], 1) : 1;
    int p1 = (n > 1) ? max_int(params[1], 1) : 1;

    /* Anything with a private-mode prefix is silently absorbed — visually
     * harmless for now and prevents literal garbage from appearing. */
    if (prefix) { t->grid.dirty = 1; return; }

    switch (final_byte) {
        case 'A': t->grid.cursorRow = clamp_int(t->grid.cursorRow - p0, 0, t->grid.rows - 1); break;
        case 'B': t->grid.cursorRow = clamp_int(t->grid.cursorRow + p0, 0, t->grid.rows - 1); break;
        case 'C': t->grid.cursorCol = clamp_int(t->grid.cursorCol + p0, 0, t->grid.cols - 1); break;
        case 'D': t->grid.cursorCol = clamp_int(t->grid.cursorCol - p0, 0, t->grid.cols - 1); break;
        case 'G': t->grid.cursorCol = clamp_int(p0 - 1, 0, t->grid.cols - 1); break;
        case 'H': case 'f': grid_set_cursor(&t->grid, p0, p1); break;
        case 'J': grid_erase_display(&t->grid, n > 0 ? params[0] : 0); break;
        case 'K': grid_erase_line(&t->grid, n > 0 ? params[0] : 0); break;
        case 'm': apply_sgr(&t->grid.currentAttr, params, n); break;
        case 'h': case 'l': /* SM/RM without prefix — absorb silently */ break;
        case 's': case 'u': /* save/restore cursor — TODO */ break;
        case 'X': /* erase N chars */
            for (int i = 0; i < p0 && t->grid.cursorCol + i < t->grid.cols; ++i)
                t->grid.cells[t->grid.cursorRow * t->grid.cols + t->grid.cursorCol + i] = make_default_cell();
            break;
        case 'P': /* delete N chars */
            for (int i = t->grid.cursorCol; i + p0 < t->grid.cols; ++i)
                t->grid.cells[t->grid.cursorRow * t->grid.cols + i] = t->grid.cells[t->grid.cursorRow * t->grid.cols + i + p0];
            break;
        case '@': /* insert N blank chars */
            for (int i = t->grid.cols - 1; i - p0 >= t->grid.cursorCol; --i)
                t->grid.cells[t->grid.cursorRow * t->grid.cols + i] = t->grid.cells[t->grid.cursorRow * t->grid.cols + i - p0];
            for (int i = 0; i < p0 && t->grid.cursorCol + i < t->grid.cols; ++i)
                t->grid.cells[t->grid.cursorRow * t->grid.cols + t->grid.cursorCol + i] = make_default_cell();
            break;
        default: break;
    }
    t->grid.dirty = 1;
}

static void handle_osc(Tab* t) {
    int code = -1;
    const char* payload = t->parser.oscBuf;
    int sep = -1;
    for (int i = 0; i < t->parser.oscLen; ++i) if (t->parser.oscBuf[i] == ';') { sep = i; break; }
    if (sep > 0) {
        t->parser.oscBuf[sep] = 0;
        code = atoi(t->parser.oscBuf);
        payload = &t->parser.oscBuf[sep + 1];
    }
    if (code == 0 || code == 1 || code == 2) {
        wchar_t title[128];
        MultiByteToWideChar(CP_UTF8, 0, payload, -1, title, 128);
        str_copy_w(t->title, 128, title);
    } else if (code == 7) {
        wchar_t cwd[MAX_PATH];
        MultiByteToWideChar(CP_UTF8, 0, payload, -1, cwd, MAX_PATH);
        if (wcsncmp(cwd, L"file://", 7) == 0) {
            wchar_t* slash = wcschr(cwd + 7, L'/');
            if (slash && slash[1] != 0 && slash[3] == L':') str_copy_w(t->session->cwd, MAX_PATH, slash + 1);
            else if (slash) str_copy_w(t->session->cwd, MAX_PATH, slash);
        } else {
            str_copy_w(t->session->cwd, MAX_PATH, cwd);
        }
    }
}

static void parser_feed(Tab* t, const char* bytes, int n) {
    AnsiParser* p = &t->parser;
    for (int i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)bytes[i];

        if (p->state == AP_OSC) {
            if (c == 0x07 || c == 0x9C) { p->oscBuf[p->oscLen] = 0; handle_osc(t); p->state = AP_GROUND; p->oscLen = 0; }
            else if (c == 0x1B) { /* maybe ST */ }
            else if (p->oscLen < (int)sizeof(p->oscBuf) - 1) p->oscBuf[p->oscLen++] = (char)c;
            continue;
        }

        switch (p->state) {
            case AP_GROUND:
                if (c < 0x20) {
                    switch (c) {
                        case 0x07: MessageBeep(MB_OK); break;
                        case 0x08: grid_backspace(&t->grid); break;
                        case 0x09: grid_tab_advance(&t->grid); break;
                        case 0x0A: grid_line_feed(&t->grid); break;
                        case 0x0D: grid_carriage_return(&t->grid); break;
                        case 0x1B: p->state = AP_ESC; break;
                        default: break;
                    }
                } else if (c < 0x80) {
                    grid_put_char(&t->grid, (wchar_t)c);
                } else {
                    if ((c & 0xE0) == 0xC0) { p->utf8Code = c & 0x1F; p->utf8Remaining = 1; p->state = AP_UTF8; }
                    else if ((c & 0xF0) == 0xE0) { p->utf8Code = c & 0x0F; p->utf8Remaining = 2; p->state = AP_UTF8; }
                    else if ((c & 0xF8) == 0xF0) { p->utf8Code = c & 0x07; p->utf8Remaining = 3; p->state = AP_UTF8; }
                }
                break;
            case AP_UTF8:
                if ((c & 0xC0) == 0x80) {
                    p->utf8Code = (p->utf8Code << 6) | (c & 0x3F);
                    if (--p->utf8Remaining <= 0) {
                        wchar_t wc = (p->utf8Code <= 0xFFFF) ? (wchar_t)p->utf8Code : L'?';
                        grid_put_char(&t->grid, wc);
                        p->state = AP_GROUND;
                    }
                } else { p->state = AP_GROUND; }
                break;
            case AP_ESC:
                if (c == '[') { p->state = AP_CSI; p->paramLen = 0; }
                else if (c == ']') { p->state = AP_OSC; p->oscLen = 0; }
                else if (c == '\\') { p->state = AP_GROUND; }
                else { p->state = AP_GROUND; }
                break;
            case AP_CSI:
                /* Parameter bytes (0x30..0x3F) include digits, ';' AND the
                 * private-mode prefixes '?', '>', '=', '<'. Without capturing
                 * the prefix we'd bail out of the state machine and print
                 * "25h", "25l", "1049h" etc as literals — which is exactly
                 * what PowerShell sends for cursor visibility / alt-screen. */
                if (c >= 0x30 && c <= 0x3F) {
                    if (p->paramLen < (int)sizeof(p->paramBuf) - 1)
                        p->paramBuf[p->paramLen++] = (char)c;
                } else if (c >= 0x20 && c <= 0x2F) {
                    /* intermediate — ignore */
                } else if (c >= 0x40 && c <= 0x7E) {
                    p->paramBuf[p->paramLen] = 0;
                    handle_csi(t, (char)c);
                    p->state = AP_GROUND;
                    p->paramLen = 0;
                } else {
                    p->state = AP_GROUND;
                }
                break;
            default: p->state = AP_GROUND; break;
        }
    }
}

/* =========================================================================
 *                            CONPTY SESSION
 * ========================================================================= */

static BOOL load_conpty_apis(void) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    if (!k32) return FALSE;
    pCreatePseudoConsole = (PFN_CreatePseudoConsole)GetProcAddress(k32, "CreatePseudoConsole");
    pResizePseudoConsole = (PFN_ResizePseudoConsole)GetProcAddress(k32, "ResizePseudoConsole");
    pClosePseudoConsole  = (PFN_ClosePseudoConsole)GetProcAddress(k32, "ClosePseudoConsole");
    return pCreatePseudoConsole && pResizePseudoConsole && pClosePseudoConsole;
}

static void resolve_shell(const wchar_t* id, wchar_t* outCmd, size_t cap, wchar_t* outName, size_t nameCap) {
    if (wcscmp(id, L"pwsh") == 0) {
        wchar_t cand[MAX_PATH];
        DWORD got = GetEnvironmentVariableW(L"ProgramFiles", cand, MAX_PATH);
        if (got > 0) {
            wcscat_s(cand, MAX_PATH, L"\\PowerShell\\7\\pwsh.exe");
            if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                _snwprintf_s(outCmd, cap, _TRUNCATE, L"\"%s\" -NoLogo", cand);
                str_copy_w(outName, nameCap, L"PowerShell 7");
                return;
            }
        }
        /* fall back to powershell */
    }
    if (wcscmp(id, L"cmd") == 0) {
        wchar_t windir[MAX_PATH] = {0};
        GetEnvironmentVariableW(L"WINDIR", windir, MAX_PATH);
        _snwprintf_s(outCmd, cap, _TRUNCATE, L"%s\\System32\\cmd.exe", windir);
        str_copy_w(outName, nameCap, L"Command Prompt");
        return;
    }
    /* default: Windows PowerShell 5.1 */
    wchar_t windir[MAX_PATH] = {0};
    GetEnvironmentVariableW(L"WINDIR", windir, MAX_PATH);
    _snwprintf_s(outCmd, cap, _TRUNCATE,
                 L"%s\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -NoLogo",
                 windir);
    str_copy_w(outName, nameCap, L"Windows PowerShell");
}

static Session* session_create(const wchar_t* shellId, int cols, int rows) {
    if (!pCreatePseudoConsole) return NULL;

    Session* s = (Session*)xcalloc(1, sizeof(Session));

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, FALSE };
    if (!CreatePipe(&s->hChildIn,  &s->hPipeIn,  &sa, 0)) goto fail;
    if (!CreatePipe(&s->hPipeOut,  &s->hChildOut, &sa, 0)) goto fail;

    COORD size; size.X = (SHORT)cols; size.Y = (SHORT)rows;
    if (FAILED(pCreatePseudoConsole(size, s->hChildIn, s->hChildOut, 0, &s->hPC))) goto fail;

    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrSize);
    unsigned char* attrBuf = (unsigned char*)malloc(attrSize);
    LPPROC_THREAD_ATTRIBUTE_LIST attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)attrBuf;
    if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize)) goto fail;
    UpdateProcThreadAttribute(attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                              s->hPC, sizeof(s->hPC), NULL, NULL);

    STARTUPINFOEXW si = {0};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = attrList;

    wchar_t cmdLine[1024];
    resolve_shell(shellId, cmdLine, 1024, s->shellName, 64);

    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE,
                              EXTENDED_STARTUPINFO_PRESENT,
                              NULL, NULL, &si.StartupInfo, &pi);
    DeleteProcThreadAttributeList(attrList);
    free(attrBuf);

    if (!ok) goto fail;

    s->hProcess = pi.hProcess;
    s->hThread  = pi.hThread;
    s->pid      = pi.dwProcessId;
    InterlockedExchange(&s->alive, 1);

    s->hJob = CreateJobObjectW(NULL, NULL);
    if (s->hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {0};
        jeli.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        SetInformationJobObject(s->hJob, JobObjectExtendedLimitInformation,
                                &jeli, sizeof(jeli));
        AssignProcessToJobObject(s->hJob, pi.hProcess);
    }

    s->hReaderThread = CreateThread(NULL, 0, reader_thread, s, 0, NULL);
    return s;

fail:
    session_destroy(s);
    return NULL;
}

static void session_destroy(Session* s) {
    if (!s) return;
    InterlockedExchange(&s->alive, 0);
    /* Closing the pseudo console signals EOF on the child's stdin/stdout,
     * which unblocks ReadFile in the reader thread on the next iteration. */
    if (s->hPC && pClosePseudoConsole) pClosePseudoConsole(s->hPC);
    if (s->hPipeIn  && s->hPipeIn  != INVALID_HANDLE_VALUE) CloseHandle(s->hPipeIn);
    if (s->hChildIn && s->hChildIn != INVALID_HANDLE_VALUE) CloseHandle(s->hChildIn);
    if (s->hChildOut&& s->hChildOut!= INVALID_HANDLE_VALUE) CloseHandle(s->hChildOut);
    if (s->hPipeOut && s->hPipeOut != INVALID_HANDLE_VALUE) CloseHandle(s->hPipeOut);

    /* Wait up to 5 s for the reader thread to drain. If it doesn't (rare —
     * the child is misbehaving), DON'T free this struct: leak the few bytes
     * rather than risk a use-after-free if the thread keeps reading.       */
    BOOL threadDone = TRUE;
    if (s->hReaderThread) {
        if (WaitForSingleObject(s->hReaderThread, 5000) != WAIT_OBJECT_0) {
            threadDone = FALSE;
        }
        CloseHandle(s->hReaderThread);
    }
    if (s->hJob)     CloseHandle(s->hJob);
    if (s->hProcess) CloseHandle(s->hProcess);
    if (s->hThread)  CloseHandle(s->hThread);
    if (threadDone) free(s);
    /* else: intentional leak — far safer than a use-after-free crash. */
}

static void session_resize(Session* s, int cols, int rows) {
    if (!s || !s->hPC || !pResizePseudoConsole) return;
    COORD cz = { (SHORT)cols, (SHORT)rows };
    pResizePseudoConsole(s->hPC, cz);
}

static void session_write(Session* s, const void* data, int n) {
    if (!s || !s->hPipeIn) return;
    DWORD wrote = 0;
    WriteFile(s->hPipeIn, data, (DWORD)n, &wrote, NULL);
}

#define WM_DANTE_OUTPUT (WM_APP + 1)

/* Wire format: [int tabId][int nBytes][...nBytes of payload...] */
static DWORD WINAPI reader_thread(LPVOID arg) {
    Session* s = (Session*)arg;
    /* Reusable read buffer — thread-local, allocated once per session,
     * freed when the thread exits. Avoids 60+ malloc/s churn on shells
     * that print large blobs (find/, dir /s, etc).                     */
    char* buf = (char*)malloc(16 * 1024);
    if (!buf) {
        InterlockedExchange(&s->alive, 0);
        return 1;
    }
    while (InterlockedCompareExchange(&s->alive, 1, 1)) {
        DWORD nr = 0;
        BOOL ok = ReadFile(s->hPipeOut, buf, 16 * 1024, &nr, NULL);
        if (!ok || nr == 0) break;

        /* Heap-allocate the message envelope (UI thread frees it on dispatch).
         * Tab id (int) is used as the stable identifier — never a Session*. */
        int tabId = s->ownerTabId;
        char* heap = (char*)malloc(nr + sizeof(int) * 2);
        if (!heap) continue;
        memcpy(heap, &tabId, sizeof(int));
        int n = (int)nr;
        memcpy(heap + sizeof(int), &n, sizeof(int));
        memcpy(heap + sizeof(int) * 2, buf, nr);
        if (!PostMessageW(g_app.hWnd, WM_DANTE_OUTPUT, 0, (LPARAM)heap)) {
            free(heap);
            break;
        }
    }
    free(buf);
    InterlockedExchange(&s->alive, 0);
    return 0;
}

/* =========================================================================
 *                          PERSISTENCE (JSON)
 *
 * State lives in %APPDATA%\Dante CLI\state.json. Schema is intentionally
 * minimal — we hand-roll the writer (one record per line, indented) and a
 * tiny tokeniser for the reader. No external deps.
 * ========================================================================= */

static void state_path(wchar_t* out, size_t cap) {
    wchar_t base[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, base))) {
        _snwprintf_s(out, cap, _TRUNCATE, L"%s\\Dante CLI", base);
        CreateDirectoryW(out, NULL);
        _snwprintf_s(out, cap, _TRUNCATE, L"%s\\Dante CLI\\state.json", base);
    } else {
        str_copy_w(out, cap, L"state.json");
    }
}

/* =====================================================================
 *                       DEBUG LOG FILE
 *
 * %APPDATA%\Dante CLI\logs\debug-YYYYMMDD.log. Lazily opened on first write,
 * flushed every line. Used by app_log() macros for crashes/warnings.
 * ===================================================================== */

static FILE*           g_logFile = NULL;
static CRITICAL_SECTION g_logLock;
static BOOL            g_logLockInit = FALSE;

static void log_open_if_needed(void) {
    if (g_logFile) return;
    if (!g_logLockInit) { InitializeCriticalSection(&g_logLock); g_logLockInit = TRUE; }
    wchar_t base[MAX_PATH] = {0};
    if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, base))) return;
    wchar_t logdir[MAX_PATH];
    _snwprintf_s(logdir, MAX_PATH, _TRUNCATE, L"%s\\Dante CLI\\logs", base);
    CreateDirectoryW(logdir, NULL);
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t path[MAX_PATH];
    _snwprintf_s(path, MAX_PATH, _TRUNCATE,
                 L"%s\\debug-%04d%02d%02d.log",
                 logdir, st.wYear, st.wMonth, st.wDay);
    _wfopen_s(&g_logFile, path, L"a, ccs=UTF-8");
}

static void app_log(const wchar_t* level, const wchar_t* fmt, ...) {
    log_open_if_needed();
    if (!g_logFile) return;
    EnterCriticalSection(&g_logLock);
    SYSTEMTIME st; GetLocalTime(&st);
    fwprintf(g_logFile, L"[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] ",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level);
    va_list ap;
    va_start(ap, fmt);
    vfwprintf(g_logFile, fmt, ap);
    va_end(ap);
    fwprintf(g_logFile, L"\n");
    fflush(g_logFile);
    LeaveCriticalSection(&g_logLock);
}

#define LOG_INFO(...)  app_log(L"INF", __VA_ARGS__)
#define LOG_WARN(...)  app_log(L"WRN", __VA_ARGS__)
#define LOG_ERR(...)   app_log(L"ERR", __VA_ARGS__)

/* =====================================================================
 *                       DPAPI ENCRYPTION
 *
 * Windows Data Protection API. Encrypts a wchar_t string scoped to the
 * current user (only the same Windows user can decrypt). We use it for
 * the Groq API key so the JSON file never contains the raw secret.
 *
 * Format on disk:  "ENC1:" + base64(ciphertext)
 * Plaintext-prefix free strings fall through unchanged on load (backward
 * compat with older versions / state.json edited by hand).
 * ===================================================================== */

static const char* B64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char* base64_encode(const BYTE* data, DWORD len) {
    DWORD outLen = ((len + 2) / 3) * 4 + 1;
    char* out = (char*)malloc(outLen);
    if (!out) return NULL;
    DWORD o = 0;
    for (DWORD i = 0; i < len; i += 3) {
        DWORD b0 = data[i];
        DWORD b1 = (i + 1 < len) ? data[i + 1] : 0;
        DWORD b2 = (i + 2 < len) ? data[i + 2] : 0;
        out[o++] = B64_CHARS[(b0 >> 2) & 0x3F];
        out[o++] = B64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3F];
        out[o++] = (i + 1 < len) ? B64_CHARS[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=';
        out[o++] = (i + 2 < len) ? B64_CHARS[b2 & 0x3F] : '=';
    }
    out[o] = 0;
    return out;
}

static BYTE* base64_decode(const char* s, DWORD* outLen) {
    int table[256] = {0};
    for (int i = 0; i < 64; ++i) table[(unsigned char)B64_CHARS[i]] = i;
    table[(unsigned char)'='] = 0;
    size_t slen = strlen(s);
    DWORD groups = (DWORD)(slen / 4);
    BYTE* out = (BYTE*)malloc(groups * 3 + 1);
    if (!out) return NULL;
    DWORD o = 0;
    for (DWORD i = 0; i < groups; ++i) {
        DWORD v0 = table[(unsigned char)s[i*4 + 0]];
        DWORD v1 = table[(unsigned char)s[i*4 + 1]];
        DWORD v2 = table[(unsigned char)s[i*4 + 2]];
        DWORD v3 = table[(unsigned char)s[i*4 + 3]];
        out[o++] = (BYTE)((v0 << 2) | (v1 >> 4));
        if (s[i*4 + 2] != '=') out[o++] = (BYTE)((v1 << 4) | (v2 >> 2));
        if (s[i*4 + 3] != '=') out[o++] = (BYTE)((v2 << 6) | v3);
    }
    *outLen = o;
    return out;
}

/* Encrypt plaintext (wchar_t string) → "ENC1:<base64>" malloc'd UTF-8 string. */
static char* dpapi_encrypt_w(const wchar_t* plain) {
    if (!plain || !plain[0]) { char* e = (char*)malloc(1); e[0] = 0; return e; }
    DATA_BLOB in, out;
    in.pbData = (BYTE*)plain;
    in.cbData = (DWORD)((wcslen(plain) + 1) * sizeof(wchar_t));
    if (!CryptProtectData(&in, L"DanteCLI.GroqKey", NULL, NULL, NULL, 0, &out)) {
        LOG_WARN(L"CryptProtectData failed err=%lu", GetLastError());
        return NULL;
    }
    char* b64 = base64_encode(out.pbData, out.cbData);
    LocalFree(out.pbData);
    if (!b64) return NULL;
    size_t need = strlen(b64) + 6;
    char* tagged = (char*)malloc(need);
    snprintf(tagged, need, "ENC1:%s", b64);
    free(b64);
    return tagged;
}

/* Decrypt "ENC1:<base64>" back to wchar_t string in caller buffer. If the
 * input doesn't carry the prefix it's treated as plaintext (legacy / edited
 * by hand) and copied verbatim.                                          */
static void dpapi_decrypt_w(const wchar_t* enc, wchar_t* out, size_t outCap) {
    if (!enc || !enc[0]) { if (outCap) out[0] = 0; return; }
    if (wcsncmp(enc, L"ENC1:", 5) != 0) {
        str_copy_w(out, outCap, enc);
        return;
    }
    /* convert wchar_t base64 → char */
    int wlen = (int)wcslen(enc + 5);
    char* b64 = (char*)malloc(wlen + 1);
    for (int i = 0; i < wlen; ++i) b64[i] = (char)enc[5 + i];
    b64[wlen] = 0;

    DWORD blobLen = 0;
    BYTE* blob = base64_decode(b64, &blobLen);
    free(b64);
    if (!blob) { if (outCap) out[0] = 0; return; }

    DATA_BLOB in, decoded;
    in.pbData = blob;
    in.cbData = blobLen;
    if (!CryptUnprotectData(&in, NULL, NULL, NULL, NULL, 0, &decoded)) {
        LOG_WARN(L"CryptUnprotectData failed err=%lu", GetLastError());
        if (outCap) out[0] = 0;
        free(blob);
        return;
    }
    free(blob);
    /* decoded.pbData is a wchar_t* including the trailing NUL */
    str_copy_w(out, outCap, (const wchar_t*)decoded.pbData);
    SecureZeroMemory(decoded.pbData, decoded.cbData);
    LocalFree(decoded.pbData);
}

static void json_escape(const wchar_t* in, wchar_t* out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 6 < cap; ++i) {
        wchar_t c = in[i];
        if (c == L'"' || c == L'\\') { out[o++] = L'\\'; out[o++] = c; }
        else if (c == L'\n')          { out[o++] = L'\\'; out[o++] = L'n'; }
        else if (c == L'\r')          { out[o++] = L'\\'; out[o++] = L'r'; }
        else if (c == L'\t')          { out[o++] = L'\\'; out[o++] = L't'; }
        else if (c < 0x20)            { o += swprintf(&out[o], cap - o, L"\\u%04x", c); }
        else                          { out[o++] = c; }
    }
    out[o] = 0;
}

static int json_write_string(FILE* f, const wchar_t* s) {
    wchar_t buf[2048];
    json_escape(s, buf, 2048);
    return fwprintf(f, L"\"%ls\"", buf);
}

static void persist_save(void) {
    wchar_t path[MAX_PATH];
    state_path(path, MAX_PATH);

    /* Backup current state.json → state.json.bak before overwriting, so a
     * power-loss mid-write can be recovered by hand.                    */
    wchar_t bak[MAX_PATH];
    _snwprintf_s(bak, MAX_PATH, _TRUNCATE, L"%s.bak", path);
    CopyFileW(path, bak, FALSE);   /* harmless if the file doesn't exist */

    FILE* f = NULL;
    if (_wfopen_s(&f, path, L"w, ccs=UTF-8") != 0 || !f) {
        LOG_ERR(L"persist_save: cannot open %s", path);
        return;
    }

    /* Encrypt the Groq key via DPAPI so the JSON never has the raw secret. */
    char* keyEnc = dpapi_encrypt_w(g_app.groqApiKey);
    wchar_t keyEncW[1024] = {0};
    if (keyEnc) {
        MultiByteToWideChar(CP_UTF8, 0, keyEnc, -1, keyEncW, 1024);
        free(keyEnc);
    }

    fwprintf(f, L"{\n");
    fwprintf(f, L"  \"version\": 1,\n");
    fwprintf(f, L"  \"sidebarMode\": %d,\n", (int)g_app.sidebarMode);
    fwprintf(f, L"  \"scheme\": ");      json_write_string(f, kSchemes[clamp_int(g_schemeIdx, 0, SCHEME_COUNT - 1)].id); fwprintf(f, L",\n");
    fwprintf(f, L"  \"groqApiKey\": ");  json_write_string(f, keyEncW); fwprintf(f, L",\n");
    fwprintf(f, L"  \"voiceLang\": ");   json_write_string(f, g_app.voiceLang); fwprintf(f, L",\n");
    fwprintf(f, L"  \"updateManifestUrl\": "); json_write_string(f, g_app.updateManifestUrl); fwprintf(f, L",\n");
    fwprintf(f, L"  \"fontPx\": %d,\n",  g_app.fontPxOverride);
    fwprintf(f, L"  \"scrollback\": %d,\n", g_app.scrollbackLines);

    fwprintf(f, L"  \"tabs\": [\n");
    for (int i = 0; i < g_app.tabCount; ++i) {
        Tab* t = g_app.tabs[i];
        fwprintf(f, L"    {");
        fwprintf(f, L"\"title\": ");      json_write_string(f, t->title);
        fwprintf(f, L", \"emoji\": ");    json_write_string(f, t->emoji);
        fwprintf(f, L", \"color\": %d",   t->colorIdx);
        fwprintf(f, L", \"pinned\": %s",  t->pinned ? L"true" : L"false");
        fwprintf(f, L", \"shell\": \"powershell\"");
        if (t->customScheme[0]) {
            fwprintf(f, L", \"scheme\": "); json_write_string(f, t->customScheme);
        }
        if (t->customFontSize > 0) {
            fwprintf(f, L", \"fontSize\": %d", t->customFontSize);
        }
        fwprintf(f, L"}%ls\n", (i + 1 < g_app.tabCount) ? L"," : L"");
    }
    fwprintf(f, L"  ],\n");

    fwprintf(f, L"  \"favorites\": [\n");
    for (int i = 0; i < g_app.favoriteCount; ++i) {
        Favorite* fv = &g_app.favorites[i];
        fwprintf(f, L"    {\"name\": "); json_write_string(f, fv->name);
        fwprintf(f, L", \"path\": ");    json_write_string(f, fv->path);
        fwprintf(f, L", \"emoji\": ");   json_write_string(f, fv->emoji);
        fwprintf(f, L"}%ls\n", (i + 1 < g_app.favoriteCount) ? L"," : L"");
    }
    fwprintf(f, L"  ],\n");

    fwprintf(f, L"  \"snippets\": [\n");
    for (int i = 0; i < g_app.snippetCount; ++i) {
        Snippet* s = &g_app.snippets[i];
        fwprintf(f, L"    {\"name\": "); json_write_string(f, s->name);
        fwprintf(f, L", \"cmd\": ");     json_write_string(f, s->cmd);
        fwprintf(f, L", \"emoji\": ");   json_write_string(f, s->emoji);
        fwprintf(f, L"}%ls\n", (i + 1 < g_app.snippetCount) ? L"," : L"");
    }
    fwprintf(f, L"  ],\n");

    fwprintf(f, L"  \"credentials\": [\n");
    for (int i = 0; i < g_app.credCount; ++i) {
        Credential* c = &g_app.creds[i];
        fwprintf(f, L"    {\"name\": "); json_write_string(f, c->name);
        fwprintf(f, L", \"kind\": ");    json_write_string(f, c->kind);
        fwprintf(f, L", \"user\": ");    json_write_string(f, c->user);
        fwprintf(f, L", \"host\": ");    json_write_string(f, c->host);
        fwprintf(f, L", \"emoji\": ");   json_write_string(f, c->emoji);
        fwprintf(f, L"}%ls\n", (i + 1 < g_app.credCount) ? L"," : L"");
    }
    fwprintf(f, L"  ]\n}\n");
    fclose(f);
}

/* Tiny JSON reader — reads our own format. Looks for "key": "value" pairs
 * inside the object scope corresponding to a given array name. Robust
 * enough for state we wrote ourselves.                                  */
static const wchar_t* skip_ws(const wchar_t* p) {
    while (*p == L' ' || *p == L'\t' || *p == L'\n' || *p == L'\r') ++p;
    return p;
}

static const wchar_t* read_json_string(const wchar_t* p, wchar_t* out, size_t cap) {
    p = skip_ws(p);
    if (*p != L'"') { if (cap) out[0] = 0; return p; }
    ++p;
    size_t o = 0;
    while (*p && *p != L'"') {
        if (*p == L'\\' && p[1]) {
            wchar_t e = p[1];
            wchar_t v = e;
            if (e == L'n') v = L'\n';
            else if (e == L'r') v = L'\r';
            else if (e == L't') v = L'\t';
            else if (e == L'u' && p[2] && p[3] && p[4] && p[5]) {
                wchar_t hex[5] = {p[2], p[3], p[4], p[5], 0};
                v = (wchar_t)wcstoul(hex, NULL, 16);
                p += 4;
            }
            if (o + 1 < cap) out[o++] = v;
            p += 2;
        } else {
            if (o + 1 < cap) out[o++] = *p;
            ++p;
        }
    }
    if (o < cap) out[o] = 0;
    if (*p == L'"') ++p;
    return p;
}

static const wchar_t* find_key(const wchar_t* p, const wchar_t* key) {
    wchar_t needle[64];
    _snwprintf_s(needle, 64, _TRUNCATE, L"\"%s\"", key);
    const wchar_t* hit = wcsstr(p, needle);
    if (!hit) return NULL;
    hit = wcschr(hit, L':');
    return hit ? hit + 1 : NULL;
}

static void parse_object_fields(const wchar_t* obj_start, const wchar_t* obj_end,
                                 void* dest, const wchar_t** keys,
                                 wchar_t** outs, int* caps, int nFields) {
    for (int k = 0; k < nFields; ++k) {
        wchar_t needle[64];
        _snwprintf_s(needle, 64, _TRUNCATE, L"\"%s\"", keys[k]);
        const wchar_t* hit = obj_start;
        while (hit && hit < obj_end) {
            const wchar_t* h = wcsstr(hit, needle);
            if (!h || h >= obj_end) break;
            const wchar_t* colon = wcschr(h, L':');
            if (!colon || colon >= obj_end) break;
            read_json_string(colon + 1, outs[k], caps[k]);
            break;
        }
        if (outs[k] == NULL) {}
    }
    UNUSED(dest);
}

static void parse_array_objects(const wchar_t* json, const wchar_t* arr_key,
                                 void (*on_obj)(const wchar_t* obj_start, const wchar_t* obj_end)) {
    const wchar_t* p = find_key(json, arr_key);
    if (!p) return;
    p = skip_ws(p);
    if (*p != L'[') return;
    ++p;
    while (*p) {
        p = skip_ws(p);
        if (*p == L']') return;
        if (*p == L'{') {
            const wchar_t* start = p;
            int depth = 1;
            ++p;
            while (*p && depth > 0) {
                if (*p == L'"') { /* skip string */
                    ++p;
                    while (*p && *p != L'"') { if (*p == L'\\' && p[1]) ++p; ++p; }
                    if (*p) ++p;
                    continue;
                }
                if (*p == L'{') depth++;
                else if (*p == L'}') depth--;
                if (depth > 0) ++p;
            }
            const wchar_t* end = p;
            on_obj(start, end);
            if (*p == L'}') ++p;
        }
        p = skip_ws(p);
        if (*p == L',') ++p;
        else if (*p == L']') return;
        else ++p;
    }
}

static void on_tab_object(const wchar_t* s, const wchar_t* e) {
    if (g_app.tabCount >= MAX_TABS) return;
    wchar_t title[128] = L"Terminal";
    wchar_t emoji[8]   = L"\U0001F4BB";
    wchar_t colorStr[16] = L"0";
    wchar_t pinned[8] = L"false";

    wchar_t* outs[] = { title, emoji, colorStr, pinned };
    int caps[] = { 128, 8, 16, 8 };
    const wchar_t* keys[] = { L"title", L"emoji", L"color", L"pinned" };

    parse_object_fields(s, e, NULL, keys, outs, caps, 4);

    /* `color` and `pinned` are NOT strings — re-extract them as raw tokens. */
    const wchar_t* c = wcsstr(s, L"\"color\"");
    int colorIdx = 0;
    if (c && c < e) {
        c = wcschr(c, L':');
        if (c && c < e) { c++; while (*c == L' ') ++c; colorIdx = _wtoi(c); }
    }
    const wchar_t* pi = wcsstr(s, L"\"pinned\"");
    BOOL isPinned = FALSE;
    if (pi && pi < e) {
        pi = wcschr(pi, L':');
        if (pi && pi < e) isPinned = (wcsstr(pi, L"true") != NULL && wcsstr(pi, L"true") < e);
    }

    Tab* t = (Tab*)xcalloc(1, sizeof(Tab));
    t->id = next_tab_id++;
    str_copy_w(t->title, 128, title);
    str_copy_w(t->emoji, 8, emoji);
    t->colorIdx = clamp_int(colorIdx, 0, 11);
    t->pinned = isPinned;

    /* Per-tab look override */
    {
        const wchar_t* sk = wcsstr(s, L"\"scheme\"");
        if (sk && sk < e) {
            const wchar_t* col = wcschr(sk, L':');
            if (col && col < e) read_json_string(col + 1, t->customScheme, 32);
        }
        const wchar_t* fk = wcsstr(s, L"\"fontSize\"");
        if (fk && fk < e) {
            const wchar_t* col = wcschr(fk, L':');
            if (col && col < e) { col++; while (*col == L' ') ++col; t->customFontSize = _wtoi(col); }
        }
    }

    terminal_grid_init(&t->grid, 120, 30);
    parser_init(&t->parser);
    t->session = session_create(L"powershell", 120, 30);
    if (t->session) t->session->ownerTabId = t->id;
    g_app.tabs[g_app.tabCount++] = t;
}

static void on_favorite_object(const wchar_t* s, const wchar_t* e) {
    if (g_app.favoriteCount >= MAX_FAVORITES) return;
    Favorite* fv = &g_app.favorites[g_app.favoriteCount];
    memset(fv, 0, sizeof(*fv));
    wchar_t* outs[] = { fv->name, fv->path, fv->emoji };
    int caps[] = { 128, MAX_PATH, 8 };
    const wchar_t* keys[] = { L"name", L"path", L"emoji" };
    parse_object_fields(s, e, NULL, keys, outs, caps, 3);
    if (fv->name[0] || fv->path[0]) g_app.favoriteCount++;
}

static void on_snippet_object(const wchar_t* s, const wchar_t* e) {
    if (g_app.snippetCount >= MAX_SNIPPETS) return;
    Snippet* sn = &g_app.snippets[g_app.snippetCount];
    memset(sn, 0, sizeof(*sn));
    wchar_t* outs[] = { sn->name, sn->cmd, sn->emoji };
    int caps[] = { 128, 512, 8 };
    const wchar_t* keys[] = { L"name", L"cmd", L"emoji" };
    parse_object_fields(s, e, NULL, keys, outs, caps, 3);
    if (sn->name[0] || sn->cmd[0]) g_app.snippetCount++;
}

static void on_credential_object(const wchar_t* s, const wchar_t* e) {
    if (g_app.credCount >= MAX_CREDS) return;
    Credential* c = &g_app.creds[g_app.credCount];
    memset(c, 0, sizeof(*c));
    wchar_t* outs[] = { c->name, c->kind, c->user, c->host, c->emoji };
    int caps[] = { 128, 16, 64, 128, 8 };
    const wchar_t* keys[] = { L"name", L"kind", L"user", L"host", L"emoji" };
    parse_object_fields(s, e, NULL, keys, outs, caps, 5);
    if (c->name[0]) g_app.credCount++;
}

static void persist_load(void) {
    wchar_t path[MAX_PATH];
    state_path(path, MAX_PATH);
    FILE* f = NULL;
    if (_wfopen_s(&f, path, L"r, ccs=UTF-8") != 0 || !f) return;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1024 * 1024) { fclose(f); return; }

    wchar_t* buf = (wchar_t*)xcalloc((size_t)sz + 2, sizeof(wchar_t));
    size_t n = fread(buf, sizeof(wchar_t), (size_t)sz, f);
    buf[n] = 0;
    fclose(f);

    /* sidebarMode */
    const wchar_t* sm = find_key(buf, L"sidebarMode");
    if (sm) {
        sm = skip_ws(sm);
        g_app.sidebarMode = (SidebarMode)clamp_int(_wtoi(sm), 0, MODE_COUNT - 1);
    }

    /* scheme */
    const wchar_t* sch = find_key(buf, L"scheme");
    if (sch) {
        wchar_t id[64] = {0};
        read_json_string(sch, id, 64);
        for (int i = 0; i < SCHEME_COUNT; ++i) {
            if (wcscmp(kSchemes[i].id, id) == 0) { g_schemeIdx = i; break; }
        }
    }
    /* api key + voice lang + font + scrollback — zero the field first so a
     * shorter new value doesn't leave trailing chars from the previous one.
     * The key may be stored either as plain text (legacy) or DPAPI-wrapped
     * (ENC1:base64). dpapi_decrypt_w() handles both transparently.       */
    const wchar_t* gk = find_key(buf, L"groqApiKey");
    if (gk) {
        SecureZeroMemory(g_app.groqApiKey, sizeof(g_app.groqApiKey));
        wchar_t raw[1024] = {0};
        read_json_string(gk, raw, 1024);
        dpapi_decrypt_w(raw, g_app.groqApiKey, 128);
        SecureZeroMemory(raw, sizeof(raw));
    }
    const wchar_t* vl = find_key(buf, L"voiceLang");
    if (vl) {
        memset(g_app.voiceLang, 0, sizeof(g_app.voiceLang));
        read_json_string(vl, g_app.voiceLang, 16);
    }
    const wchar_t* um = find_key(buf, L"updateManifestUrl");
    if (um) {
        memset(g_app.updateManifestUrl, 0, sizeof(g_app.updateManifestUrl));
        read_json_string(um, g_app.updateManifestUrl, 512);
    }
    const wchar_t* fp = find_key(buf, L"fontPx");
    if (fp) { fp = skip_ws(fp); g_app.fontPxOverride = _wtoi(fp); }
    const wchar_t* sbk = find_key(buf, L"scrollback");
    if (sbk) { sbk = skip_ws(sbk); g_app.scrollbackLines = _wtoi(sbk); }

    /* Arrays */
    parse_array_objects(buf, L"favorites",   on_favorite_object);
    parse_array_objects(buf, L"snippets",    on_snippet_object);
    parse_array_objects(buf, L"credentials", on_credential_object);
    parse_array_objects(buf, L"tabs",        on_tab_object);

    free(buf);
}

static void schedule_persist(void) {
    g_app.pendingPersist = 1;
}

/* =========================================================================
 *                       SIMPLE PROMPT DIALOGS
 * ========================================================================= */

typedef struct {
    const wchar_t* title;
    const wchar_t* prompt;
    wchar_t* out;
    int outCap;
    HWND hEdit;
    HWND hOK;
    HWND hCancel;
    BOOL submitted;
} PromptCtx;

static PromptCtx* g_promptCtx = NULL;

/* Subclass the prompt's EDIT control so Enter submits as IDOK. Plain
 * IsDialogMessage won't fire BS_DEFPUSHBUTTON because our window isn't a
 * real dialog — the edit captures the keystroke first.                  */
static WNDPROC g_origPromptEditProc = NULL;

static LRESULT CALLBACK PromptEditSubclass(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDOK, 0), 0);
        return 0;
    }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE) {
        SendMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
        return 0;
    }
    return CallWindowProcW(g_origPromptEditProc, hWnd, msg, wParam, lParam);
}

static LRESULT CALLBACK PromptWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT f = g_app.hFontUI;

            HWND lbl = CreateWindowExW(0, L"STATIC", g_promptCtx->prompt,
                WS_CHILD | WS_VISIBLE, 16, 14, 380, 36,
                hWnd, NULL, GetModuleHandleW(NULL), NULL);
            SendMessageW(lbl, WM_SETFONT, (WPARAM)f, TRUE);

            g_promptCtx->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                16, 58, 380, 28, hWnd, (HMENU)100, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_promptCtx->hEdit, WM_SETFONT, (WPARAM)f, TRUE);
            /* Install the Enter/Escape subclass on the edit. */
            g_origPromptEditProc = (WNDPROC)SetWindowLongPtrW(
                g_promptCtx->hEdit, GWLP_WNDPROC, (LONG_PTR)PromptEditSubclass);

            g_promptCtx->hOK = CreateWindowExW(0, L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                212, 100, 90, 28, hWnd, (HMENU)IDOK, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_promptCtx->hOK, WM_SETFONT, (WPARAM)f, TRUE);

            g_promptCtx->hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                WS_CHILD | WS_VISIBLE,
                306, 100, 90, 28, hWnd, (HMENU)IDCANCEL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_promptCtx->hCancel, WM_SETFONT, (WPARAM)f, TRUE);

            SetFocus(g_promptCtx->hEdit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                GetWindowTextW(g_promptCtx->hEdit, g_promptCtx->out, g_promptCtx->outCap);
                g_promptCtx->submitted = TRUE;
                DestroyWindow(hWnd);
            } else if (LOWORD(wParam) == IDCANCEL) {
                g_promptCtx->submitted = FALSE;
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:
            g_promptCtx->submitted = FALSE;
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            /* Wake the modal GetMessage loop without injecting WM_QUIT — that
             * would also terminate the main app loop (the bug that closed the
             * whole app when the user clicked Save in Settings). Posting a
             * NULL message via thread-message is enough.                    */
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static BOOL prompt_text(const wchar_t* title, const wchar_t* prompt, wchar_t* out, int outCap) {
    PromptCtx ctx = { title, prompt, out, outCap, NULL, NULL, NULL, FALSE };
    g_promptCtx = &ctx;

    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = PromptWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"DantePromptDlg";
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int W = 420, H = 170;
    int x = pr.left + ((pr.right - pr.left) - W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - H) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"DantePromptDlg", title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, W, H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(dlg, SW_SHOW);
    EnableWindow(g_app.hWnd, FALSE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(dlg)) break;
    }

    EnableWindow(g_app.hWnd, TRUE);
    SetForegroundWindow(g_app.hWnd);
    g_promptCtx = NULL;
    return ctx.submitted && out[0] != 0;
}

/* =========================================================================
 *                       SETTINGS DIALOG
 *
 * Modal popup window with 3 sections: Appearance (scheme + font size),
 * Voice & AI (Groq API key, voice language), General (scrollback).
 * ========================================================================= */

typedef struct {
    HWND hScheme;
    HWND hFontSize;
    HWND hGroqKey;
    HWND hVoiceLang;
    HWND hScrollback;
    HWND hOK;
    HWND hCancel;
} SettingsCtx;

static SettingsCtx* g_settingsCtx = NULL;

static LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT f = g_app.hFontUI;
            int y = 18;

            CreateWindowExW(0, L"STATIC", L"Tema do terminal:",
                WS_CHILD | WS_VISIBLE, 18, y, 200, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            g_settingsCtx->hScheme = CreateWindowExW(0, L"COMBOBOX", NULL,
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                220, y - 2, 220, 200, hWnd, (HMENU)201,
                GetModuleHandleW(NULL), NULL);
            for (int i = 0; i < SCHEME_COUNT; ++i) {
                SendMessageW(g_settingsCtx->hScheme, CB_ADDSTRING, 0, (LPARAM)kSchemes[i].displayName);
            }
            SendMessageW(g_settingsCtx->hScheme, CB_SETCURSEL, g_schemeIdx, 0);
            y += 36;

            CreateWindowExW(0, L"STATIC", L"Tamanho da fonte (9-28pt, 0=padrão):",
                WS_CHILD | WS_VISIBLE, 18, y, 240, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            wchar_t fs[16];
            _snwprintf_s(fs, 16, _TRUNCATE, L"%d", g_app.fontPxOverride);
            g_settingsCtx->hFontSize = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", fs,
                WS_CHILD | WS_VISIBLE | ES_NUMBER,
                280, y - 2, 80, 26, hWnd, (HMENU)202,
                GetModuleHandleW(NULL), NULL);
            y += 36;

            CreateWindowExW(0, L"STATIC", L"Scrollback (linhas):",
                WS_CHILD | WS_VISIBLE, 18, y, 200, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            wchar_t sb[16];
            _snwprintf_s(sb, 16, _TRUNCATE, L"%d",
                         g_app.scrollbackLines > 0 ? g_app.scrollbackLines : 5000);
            g_settingsCtx->hScrollback = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sb,
                WS_CHILD | WS_VISIBLE | ES_NUMBER,
                220, y - 2, 120, 26, hWnd, (HMENU)203,
                GetModuleHandleW(NULL), NULL);
            y += 50;

            CreateWindowExW(0, L"STATIC", L"───  Voz & IA  ─────────────────────",
                WS_CHILD | WS_VISIBLE, 18, y, 450, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            y += 30;

            CreateWindowExW(0, L"STATIC", L"Groq API key (https://console.groq.com/keys):",
                WS_CHILD | WS_VISIBLE, 18, y, 460, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            y += 24;
            g_settingsCtx->hGroqKey = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", g_app.groqApiKey,
                WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                18, y, 460, 28, hWnd, (HMENU)204,
                GetModuleHandleW(NULL), NULL);
            y += 38;

            CreateWindowExW(0, L"STATIC", L"Idioma da transcrição de voz (pt, en):",
                WS_CHILD | WS_VISIBLE, 18, y, 280, 22, hWnd, NULL,
                GetModuleHandleW(NULL), NULL);
            const wchar_t* vlang = g_app.voiceLang[0] ? g_app.voiceLang : L"pt";
            g_settingsCtx->hVoiceLang = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", vlang,
                WS_CHILD | WS_VISIBLE,
                300, y - 2, 80, 26, hWnd, (HMENU)205,
                GetModuleHandleW(NULL), NULL);
            y += 48;

            g_settingsCtx->hOK = CreateWindowExW(0, L"BUTTON", L"Salvar",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                280, y, 90, 30, hWnd, (HMENU)IDOK,
                GetModuleHandleW(NULL), NULL);
            g_settingsCtx->hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                WS_CHILD | WS_VISIBLE,
                380, y, 90, 30, hWnd, (HMENU)IDCANCEL,
                GetModuleHandleW(NULL), NULL);

            SendMessageW(g_settingsCtx->hScheme,     WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hFontSize,   WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hScrollback, WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hGroqKey,    WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hVoiceLang,  WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hOK,         WM_SETFONT, (WPARAM)f, TRUE);
            SendMessageW(g_settingsCtx->hCancel,     WM_SETFONT, (WPARAM)f, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                int newScheme = (int)SendMessageW(g_settingsCtx->hScheme, CB_GETCURSEL, 0, 0);
                if (newScheme >= 0 && newScheme < SCHEME_COUNT) g_schemeIdx = newScheme;
                wchar_t buf[64];
                GetWindowTextW(g_settingsCtx->hFontSize, buf, 64);
                g_app.fontPxOverride = clamp_int(_wtoi(buf), 0, 48);
                GetWindowTextW(g_settingsCtx->hScrollback, buf, 64);
                int sb = _wtoi(buf);
                if (sb > 0) g_app.scrollbackLines = sb;
                GetWindowTextW(g_settingsCtx->hGroqKey, g_app.groqApiKey, 128);
                GetWindowTextW(g_settingsCtx->hVoiceLang, g_app.voiceLang, 16);
                /* Force-flush to disk now — the debounced timer (1.5 s) was
                 * losing the change because the user could close the app or
                 * something could race the timer. The API key must be safe
                 * the moment the user clicks Save.                        */
                g_app.pendingPersist = 0;
                persist_save();
                InvalidateRect(g_app.hWnd, NULL, FALSE);
                DestroyWindow(hWnd);
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:    DestroyWindow(hWnd); return 0;
        case WM_DESTROY:
            /* Same fix as the prompt dialog — do NOT PostQuitMessage, that
             * would kill the entire app right after the user clicks Save. */
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_settings_dialog(void) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"DanteSettingsDlg";
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    SettingsCtx ctx = {0};
    g_settingsCtx = &ctx;

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int W = 520, H = 430;
    int x = pr.left + ((pr.right - pr.left) - W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - H) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"DanteSettingsDlg", L"Configurações — Dante CLI",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, W, H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(dlg, SW_SHOW);
    EnableWindow(g_app.hWnd, FALSE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!IsWindow(dlg)) break;
    }
    EnableWindow(g_app.hWnd, TRUE);
    SetForegroundWindow(g_app.hWnd);
    g_settingsCtx = NULL;
}

static void show_add_dialog(void) {
    wchar_t name[128]  = {0};
    wchar_t value[512] = {0};

    switch (g_app.sidebarMode) {
        case MODE_FAVORITES: {
            if (!prompt_text(L"Novo favorito",
                L"Nome (ex: \"Projeto X\"):", name, 128)) return;
            if (!prompt_text(L"Novo favorito",
                L"Caminho da pasta (ex: C:\\Users\\eu\\proj):", value, 512)) return;
            if (g_app.favoriteCount >= MAX_FAVORITES) return;
            Favorite* fv = &g_app.favorites[g_app.favoriteCount++];
            memset(fv, 0, sizeof(*fv));
            str_copy_w(fv->name, 128, name);
            str_copy_w(fv->path, MAX_PATH, value);
            str_copy_w(fv->emoji, 8, L"\U0001F4C2");
            break;
        }
        case MODE_SNIPPETS: {
            if (!prompt_text(L"Novo snippet",
                L"Nome (ex: \"Listar containers\"):", name, 128)) return;
            if (!prompt_text(L"Novo snippet",
                L"Comando a injetar (ex: docker ps -a):", value, 512)) return;
            if (g_app.snippetCount >= MAX_SNIPPETS) return;
            Snippet* sn = &g_app.snippets[g_app.snippetCount++];
            memset(sn, 0, sizeof(*sn));
            str_copy_w(sn->name, 128, name);
            str_copy_w(sn->cmd, 512, value);
            str_copy_w(sn->emoji, 8, L"⚡");
            break;
        }
        case MODE_CREDS: {
            if (!prompt_text(L"Nova credencial",
                L"Nome (ex: \"Servidor prod\"):", name, 128)) return;
            if (!prompt_text(L"Nova credencial",
                L"Tipo (ssh, ftp, api, custom):", value, 64)) return;
            wchar_t user[64] = {0}, host[128] = {0};
            prompt_text(L"Nova credencial", L"Usuário (opcional):", user, 64);
            prompt_text(L"Nova credencial", L"Host (opcional):", host, 128);
            if (g_app.credCount >= MAX_CREDS) return;
            Credential* c = &g_app.creds[g_app.credCount++];
            memset(c, 0, sizeof(*c));
            str_copy_w(c->name, 128, name);
            str_copy_w(c->kind, 16, value);
            str_copy_w(c->user, 64, user);
            str_copy_w(c->host, 128, host);
            str_copy_w(c->emoji, 8, L"\U0001F511");
            break;
        }
        case MODE_FILES: {
            /* Navigate to the user's HOME folder. */
            wchar_t home[MAX_PATH] = {0};
            SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, home);
            files_set_dir(home);
            InvalidateRect(g_app.hWnd, NULL, FALSE);
            return;
        }
        default:
            return;
    }
    schedule_persist();
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

/* =========================================================================
 *                          TAB CONTEXT MENU
 * ========================================================================= */

enum {
    IDM_TAB_RENAME = 0x1100,
    IDM_TAB_DUPLICATE,
    IDM_TAB_PIN,
    IDM_TAB_CLOSE,
    IDM_TAB_CLOSE_OTHERS,
    IDM_TAB_NEW_PWSH7,
    IDM_TAB_NEW_CMD,
    IDM_TAB_EMOJI_PICK,
    IDM_TAB_EMOJI_NONE,
    IDM_TAB_APPEARANCE,
    IDM_TAB_COLOR_BASE   = 0x1200,    /* +0..11 */
    IDM_TAB_SCHEME_BASE  = 0x1300,    /* +0..SCHEME_COUNT-1 */
    IDM_TAB_FONTSIZE_BASE = 0x1400,   /* +9..28 inclusive */
    IDM_TAB_RESET_LOOK    = 0x14FF,   /* clear per-tab override */
};

/* Curated set of "developer-friendly" emoji shown in the picker. */
static const wchar_t* kEmojiPalette[] = {
    L"\U0001F4BB", L"\U0001F4C1", L"\U0001F4C2", L"⚡",       L"\U0001F525",
    L"\U0001F680", L"\U0001F40D", L"\U0001F981", L"\U0001F436", L"\U0001F431",
    L"\U0001F981", L"\U0001F427", L"\U0001F47E", L"\U0001F916", L"✨",
    L"\U0001F9EA", L"\U0001F50D", L"\U0001F511", L"\U0001F512", L"\U0001F4E6",
    L"\U0001F4DD", L"\U0001F4CA", L"\U0001F3AF", L"\U0001F4A1", L"\U0001F308",
    L"\U0001F31F", L"⭐",       L"\U0001F4CC", L"\U0001F4CD", L"\U0001F3F7",
    L"\U0001F4DF", L"\U0001F4DE", L"\U0001F4E1", L"\U0001F310", L"\U0001F30E",
    L"☕",     L"\U0001F37A", L"\U0001F355", L"\U0001F354", L"\U0001F354",
};
#define EMOJI_PALETTE_COUNT ((int)(sizeof(kEmojiPalette)/sizeof(kEmojiPalette[0])))

/* ---- Emoji picker popup ---------------------------------------------- */
typedef struct {
    HWND hWnd;
    int  selected;       /* picked index, -1 on cancel */
    int  hoverIdx;
    int  targetTabIdx;
} EmojiPickerCtx;

static EmojiPickerCtx* g_emojiCtx = NULL;

static RECT emoji_cell_rect(int idx) {
    /* 8 cols × 5 rows, 48 px each */
    int col = idx % 8;
    int row = idx / 8;
    RECT r;
    r.left = 14 + col * 50;
    r.top  = 56 + row * 50;
    r.right  = r.left + 44;
    r.bottom = r.top  + 44;
    return r;
}

static int emoji_hit_test(int x, int y) {
    for (int i = 0; i < EMOJI_PALETTE_COUNT; ++i) {
        RECT r = emoji_cell_rect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static LRESULT CALLBACK EmojiPickerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hWnd, &ps);
            RECT cli; GetClientRect(hWnd, &cli);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cli.right, cli.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
            fill_rect_color(mem, &cli, COL_BG_SIDE);

            draw_text_w(mem, 18, 18, L"Escolha um emoji para a aba",
                        COL_FG, g_app.hFontUIBold);

            for (int i = 0; i < EMOJI_PALETTE_COUNT; ++i) {
                RECT r = emoji_cell_rect(i);
                BOOL hover = (g_emojiCtx->hoverIdx == i);
                draw_rounded_rect(mem, r,
                                  hover ? COL_BG_CHIP_H : COL_BG_CHIP, 0, 8);
                SIZE sz;
                HFONT old = (HFONT)SelectObject(mem, g_app.hFontEmoji);
                SetBkMode(mem, TRANSPARENT);
                SetTextColor(mem, COL_FG);
                GetTextExtentPoint32W(mem, kEmojiPalette[i],
                                      (int)wcslen(kEmojiPalette[i]), &sz);
                TextOutW(mem,
                         r.left + ((r.right - r.left) - sz.cx) / 2,
                         r.top  + ((r.bottom - r.top) - sz.cy) / 2,
                         kEmojiPalette[i],
                         (int)wcslen(kEmojiPalette[i]));
                SelectObject(mem, old);
            }

            BitBlt(dc, 0, 0, cli.right, cli.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int h = emoji_hit_test(x, y);
            if (h != g_emojiCtx->hoverIdx) {
                g_emojiCtx->hoverIdx = h;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int idx = emoji_hit_test(x, y);
            if (idx >= 0) {
                g_emojiCtx->selected = idx;
                DestroyWindow(hWnd);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            return 0;
        case WM_CLOSE: DestroyWindow(hWnd); return 0;
        case WM_DESTROY:
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_emoji_picker(int tabIdx) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = EmojiPickerWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_HAND);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"DanteEmojiPicker";
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    EmojiPickerCtx ctx = { NULL, -1, -1, tabIdx };
    g_emojiCtx = &ctx;

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int W = 8 * 50 + 28;
    int H = 56 + 5 * 50 + 16;
    int x = pr.left + ((pr.right - pr.left) - W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - H) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"DanteEmojiPicker", L"Emoji da aba",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, W, H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ctx.hWnd = dlg;
    ShowWindow(dlg, SW_SHOW);
    SetFocus(dlg);
    EnableWindow(g_app.hWnd, FALSE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(dlg)) break;
    }
    EnableWindow(g_app.hWnd, TRUE);
    SetForegroundWindow(g_app.hWnd);

    if (ctx.selected >= 0 && tabIdx >= 0 && tabIdx < g_app.tabCount) {
        str_copy_w(g_app.tabs[tabIdx]->emoji, 8, kEmojiPalette[ctx.selected]);
        schedule_persist();
        InvalidateRect(g_app.hWnd, NULL, FALSE);
    }
    g_emojiCtx = NULL;
}

static void duplicate_tab(int idx) {
    if (idx < 0 || idx >= g_app.tabCount || g_app.tabCount >= MAX_TABS) return;
    Tab* src = g_app.tabs[idx];
    open_new_tab(L"powershell");
    Tab* nt = g_app.tabs[g_app.tabCount - 1];
    str_copy_w(nt->title, 128, src->title);
    str_copy_w(nt->emoji, 8, src->emoji);
    nt->colorIdx = src->colorIdx;
    schedule_persist();
}

static void show_tab_context_menu(HWND hWnd, int tabIdx, int x, int y) {
    if (tabIdx < 0 || tabIdx >= g_app.tabCount) return;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_TAB_RENAME, L"Renomear...");
    AppendMenuW(m, MF_STRING, IDM_TAB_DUPLICATE, L"Duplicar");
    AppendMenuW(m, MF_STRING | (g_app.tabs[tabIdx]->pinned ? MF_CHECKED : 0),
                IDM_TAB_PIN, L"Fixar");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);

    HMENU colors = CreatePopupMenu();
    static const wchar_t* names[12] = {
        L"Neutro", L"Vermelho", L"Laranja", L"Amarelo", L"Verde", L"Menta",
        L"Ciano", L"Azul", L"Índigo", L"Roxo", L"Pink", L"Marrom"
    };
    for (int i = 0; i < 12; ++i) {
        AppendMenuW(colors, MF_STRING | (g_app.tabs[tabIdx]->colorIdx == i ? MF_CHECKED : 0),
                    IDM_TAB_COLOR_BASE + i, names[i]);
    }
    AppendMenuW(m, MF_POPUP, (UINT_PTR)colors, L"Cor");
    AppendMenuW(m, MF_STRING, IDM_TAB_EMOJI_PICK, L"Emoji...");
    if (g_app.tabs[tabIdx]->emoji[0])
        AppendMenuW(m, MF_STRING, IDM_TAB_EMOJI_NONE, L"Remover emoji");

    /* Aparência — submenu com Tema + Tamanho de fonte (override per-tab). */
    {
        HMENU app_menu = CreatePopupMenu();

        HMENU schemes = CreatePopupMenu();
        for (int i = 0; i < SCHEME_COUNT; ++i) {
            UINT flags = MF_STRING;
            if (g_app.tabs[tabIdx]->customScheme[0] &&
                _wcsicmp(g_app.tabs[tabIdx]->customScheme, kSchemes[i].id) == 0)
                flags |= MF_CHECKED;
            AppendMenuW(schemes, flags,
                        IDM_TAB_SCHEME_BASE + i, kSchemes[i].displayName);
        }
        AppendMenuW(app_menu, MF_POPUP, (UINT_PTR)schemes, L"Tema");

        HMENU sizes = CreatePopupMenu();
        for (int s = 9; s <= 28; ++s) {
            wchar_t lbl[16];
            _snwprintf_s(lbl, 16, _TRUNCATE, L"%d pt", s);
            UINT flags = MF_STRING;
            if (g_app.tabs[tabIdx]->customFontSize == s) flags |= MF_CHECKED;
            AppendMenuW(sizes, flags, IDM_TAB_FONTSIZE_BASE + s, lbl);
        }
        AppendMenuW(app_menu, MF_POPUP, (UINT_PTR)sizes, L"Tamanho da fonte");

        if (g_app.tabs[tabIdx]->customScheme[0] ||
            g_app.tabs[tabIdx]->customFontSize > 0) {
            AppendMenuW(app_menu, MF_SEPARATOR, 0, NULL);
            AppendMenuW(app_menu, MF_STRING, IDM_TAB_RESET_LOOK,
                        L"Voltar ao padrão global");
        }
        AppendMenuW(m, MF_POPUP, (UINT_PTR)app_menu, L"Aparência");
    }

    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_TAB_NEW_PWSH7, L"Nova aba (PowerShell 7)");
    AppendMenuW(m, MF_STRING, IDM_TAB_NEW_CMD,   L"Nova aba (cmd.exe)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_TAB_CLOSE,        L"Fechar");
    AppendMenuW(m, MF_STRING, IDM_TAB_CLOSE_OTHERS, L"Fechar outras");

    POINT pt = { x, y };
    ClientToScreen(hWnd, &pt);

    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(m);

    if (cmd == IDM_TAB_RENAME) {
        wchar_t newName[128] = {0};
        str_copy_w(newName, 128, g_app.tabs[tabIdx]->title);
        if (prompt_text(L"Renomear aba", L"Novo título:", newName, 128)) {
            str_copy_w(g_app.tabs[tabIdx]->title, 128, newName);
            schedule_persist();
        }
    } else if (cmd == IDM_TAB_DUPLICATE) {
        duplicate_tab(tabIdx);
    } else if (cmd == IDM_TAB_PIN) {
        g_app.tabs[tabIdx]->pinned = !g_app.tabs[tabIdx]->pinned;
        schedule_persist();
    } else if (cmd == IDM_TAB_CLOSE) {
        close_tab(tabIdx);
    } else if (cmd == IDM_TAB_CLOSE_OTHERS) {
        /* close from the end so indices stay valid */
        for (int i = g_app.tabCount - 1; i >= 0; --i) if (i != tabIdx) close_tab(i);
    } else if (cmd == IDM_TAB_NEW_PWSH7) {
        open_new_tab(L"pwsh");
    } else if (cmd == IDM_TAB_NEW_CMD) {
        open_new_tab(L"cmd");
    } else if (cmd >= IDM_TAB_COLOR_BASE && cmd < IDM_TAB_COLOR_BASE + 12) {
        g_app.tabs[tabIdx]->colorIdx = cmd - IDM_TAB_COLOR_BASE;
        schedule_persist();
    } else if (cmd == IDM_TAB_EMOJI_PICK) {
        show_emoji_picker(tabIdx);
    } else if (cmd == IDM_TAB_EMOJI_NONE) {
        g_app.tabs[tabIdx]->emoji[0] = 0;
        schedule_persist();
    } else if (cmd >= IDM_TAB_SCHEME_BASE && cmd < IDM_TAB_SCHEME_BASE + SCHEME_COUNT) {
        int idx = cmd - IDM_TAB_SCHEME_BASE;
        str_copy_w(g_app.tabs[tabIdx]->customScheme, 32, kSchemes[idx].id);
        schedule_persist();
    } else if (cmd >= IDM_TAB_FONTSIZE_BASE + 9 && cmd <= IDM_TAB_FONTSIZE_BASE + 28) {
        g_app.tabs[tabIdx]->customFontSize = cmd - IDM_TAB_FONTSIZE_BASE;
        schedule_persist();
    } else if (cmd == IDM_TAB_RESET_LOOK) {
        g_app.tabs[tabIdx]->customScheme[0] = 0;
        g_app.tabs[tabIdx]->customFontSize = 0;
        schedule_persist();
    }

    InvalidateRect(hWnd, NULL, FALSE);
}

/* =========================================================================
 *                       ABOUT / UPDATE CHECK
 * ========================================================================= */

static void show_about(void) {
    wchar_t msg[1024];
    _snwprintf_s(msg, 1024, _TRUNCATE,
        L"Dante CLI %s\n\n"
        L"Terminal nativo para Windows.\n\n"
        L"Recursos:\n"
        L"  · ConPTY + parser ANSI próprio (cores 16/256/truecolor)\n"
        L"  · 19 esquemas de cor (Tokyo Night, Dracula, Nord, …)\n"
        L"  · Sidebar com Favoritos, Snippets, Credenciais persistidos\n"
        L"  · Split workspace (5 layouts) com foco por célula\n"
        L"  · Integração Groq Chat (Explicar) + Groq Whisper (Voz)\n"
        L"  · 100%% Win32, zero dependências externas, 340 KB\n\n"
        L"© 2026 Dante · MIT License\n"
        L"Pressione Ctrl+/ para ver atalhos.",
        APP_VERSION_W);
    MessageBoxW(g_app.hWnd, msg, L"Sobre o Dante CLI", MB_ICONINFORMATION | MB_OK);
}

#define WM_DANTE_UPDATE_RESULT (WM_APP + 6)

typedef struct { char* url; wchar_t* version; } UpdateInfo;

static DWORD WINAPI update_check_thread(LPVOID arg) {
    (void)arg;

    /* Use the user-configured URL if set, else the default GitHub manifest. */
    wchar_t host[256] = L"raw.githubusercontent.com";
    wchar_t path[512] = L"/dantetesta/dante-cli/main/updates/win.json";
    if (g_app.updateManifestUrl[0]) {
        /* Tiny URL parser: https://HOST/PATH */
        const wchar_t* u = g_app.updateManifestUrl;
        if (wcsncmp(u, L"https://", 8) == 0) u += 8;
        else if (wcsncmp(u, L"http://", 7) == 0) u += 7;
        const wchar_t* slash = wcschr(u, L'/');
        if (slash) {
            int hlen = (int)(slash - u);
            if (hlen > 0 && hlen < 256) {
                wcsncpy_s(host, 256, u, hlen);
                host[hlen] = 0;
            }
            str_copy_w(path, 512, slash);
        } else {
            str_copy_w(host, 256, u);
            str_copy_w(path, 512, L"/");
        }
    }

    HINTERNET hSes = WinHttpOpen(L"DanteCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSes) return 0;
    HINTERNET hCon = WinHttpConnect(hSes, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hCon) { WinHttpCloseHandle(hSes); return 0; }
    HINTERNET hReq = WinHttpOpenRequest(hCon, L"GET", path,
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes); return 0; }
    if (!WinHttpSendRequest(hReq, NULL, 0, NULL, 0, 0, 0)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes);
        return 0;
    }
    if (!WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes);
        return 0;
    }

    char buf[4096]; DWORD avail = 0, total = 0;
    while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0 && total < 3500) {
        DWORD got = 0;
        WinHttpReadData(hReq, buf + total, avail, &got);
        total += got;
    }
    buf[total] = 0;
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hCon); WinHttpCloseHandle(hSes);

    /* Parse {"version":"x.y.z","url":"..."} */
    const char* vk = strstr(buf, "\"version\"");
    if (vk) {
        vk = strchr(vk, ':');
        if (vk) {
            vk++;
            while (*vk == ' ' || *vk == '"') vk++;
            char ver[32] = {0}; int i = 0;
            while (*vk && *vk != '"' && i < 31) ver[i++] = *vk++;
            wchar_t* w = utf8_to_w_dup(ver, -1);
            PostMessageW(g_app.hWnd, WM_DANTE_UPDATE_RESULT, 0, (LPARAM)w);
        }
    }
    return 0;
}

static void check_for_updates_async(void) {
    HANDLE h = CreateThread(NULL, 0, update_check_thread, NULL, 0, NULL);
    if (h) CloseHandle(h);
}

/* =========================================================================
 *                       RESOURCE MONITOR
 *
 * Per-PID CPU/Mem sampling + a 60-sample sparkline of total CPU. Click on
 * the stats chip in the tab bar opens a non-modal popover that mirrors the
 * macOS "Consumo de Recursos" panel.
 * ========================================================================= */

#define CPU_HIST_LEN     60
#define MAX_TRACKED_PIDS 64

typedef struct {
    DWORD     pid;
    ULONGLONG lastKernel;
    ULONGLONG lastUser;
    ULONGLONG lastWall;     /* GetTickCount64 */
} CpuSample;

static CpuSample g_cpuSamples[MAX_TRACKED_PIDS];

static double    g_cpuHist[CPU_HIST_LEN];   /* total CPU% (0..100) history */
static int       g_cpuHistIdx = 0;
static double    g_lastTotalCpu = 0.0;
static SIZE_T    g_lastTotalMem = 0;
static SIZE_T    g_lastAppMem   = 0;
static SIZE_T    g_lastShellMem = 0;
static int       g_cpuCoreCount = 0;

static CpuSample* cpu_sample_for(DWORD pid) {
    CpuSample* free_slot = NULL;
    for (int i = 0; i < MAX_TRACKED_PIDS; ++i) {
        if (g_cpuSamples[i].pid == pid) return &g_cpuSamples[i];
        if (!free_slot && g_cpuSamples[i].pid == 0) free_slot = &g_cpuSamples[i];
    }
    if (!free_slot) free_slot = &g_cpuSamples[0]; /* LRU-ish */
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->pid = pid;
    return free_slot;
}

static double cpu_pct_for_pid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0.0;
    FILETIME ct = {0}, et = {0}, kt = {0}, ut = {0};
    if (!GetProcessTimes(h, &ct, &et, &kt, &ut)) { CloseHandle(h); return 0.0; }
    CloseHandle(h);

    ULARGE_INTEGER kli, uli;
    kli.LowPart = kt.dwLowDateTime; kli.HighPart = kt.dwHighDateTime;
    uli.LowPart = ut.dwLowDateTime; uli.HighPart = ut.dwHighDateTime;
    ULONGLONG nowK  = kli.QuadPart;
    ULONGLONG nowU  = uli.QuadPart;
    ULONGLONG nowW  = GetTickCount64();

    CpuSample* s = cpu_sample_for(pid);
    double pct = 0.0;
    if (s->lastWall != 0) {
        ULONGLONG dCpu  = (nowK - s->lastKernel) + (nowU - s->lastUser);
        ULONGLONG dTime = (nowW - s->lastWall) * 10000ULL;  /* ms → 100ns */
        if (dTime > 0 && g_cpuCoreCount > 0) {
            pct = (double)dCpu / (double)dTime /
                  (double)g_cpuCoreCount * 100.0;
            if (pct < 0.0) pct = 0.0;
            if (pct > 100.0) pct = 100.0;
        }
    }
    s->lastKernel = nowK; s->lastUser = nowU; s->lastWall = nowW;
    return pct;
}

static SIZE_T mem_for_pid(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                           FALSE, pid);
    if (!h) return 0;
    PROCESS_MEMORY_COUNTERS_EX pmc = {0};
    pmc.cb = sizeof(pmc);
    SIZE_T out = 0;
    if (GetProcessMemoryInfo(h, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        out = pmc.PrivateUsage;
    CloseHandle(h);
    return out;
}

/* Recompute everything once. Called every 1 s from the main timer. */
static void resmon_sample(void) {
    if (g_cpuCoreCount == 0) {
        SYSTEM_INFO si; GetSystemInfo(&si);
        g_cpuCoreCount = (int)si.dwNumberOfProcessors;
        if (g_cpuCoreCount < 1) g_cpuCoreCount = 1;
    }

    /* App process */
    DWORD appPid = GetCurrentProcessId();
    double appCpu = cpu_pct_for_pid(appPid);
    g_lastAppMem  = mem_for_pid(appPid);

    /* Shells — also cache per-tab metrics so the header paint can read them
     * without doing OpenProcess on every repaint. */
    double shCpu = 0.0;
    SIZE_T shMem = 0;
    for (int i = 0; i < g_app.tabCount; ++i) {
        Tab* t = g_app.tabs[i];
        if (!t || !t->session || !t->session->pid) continue;
        double tcpu = cpu_pct_for_pid(t->session->pid);
        SIZE_T tmem = mem_for_pid(t->session->pid);
        t->cachedCpuPct   = tcpu;
        t->cachedMemBytes = tmem;
        shCpu += tcpu;
        shMem += tmem;
    }

    g_lastTotalCpu  = appCpu + shCpu;
    if (g_lastTotalCpu > 100.0) g_lastTotalCpu = 100.0;
    g_lastShellMem  = shMem;
    g_lastTotalMem  = g_lastAppMem + g_lastShellMem;

    g_cpuHist[g_cpuHistIdx] = g_lastTotalCpu;
    g_cpuHistIdx = (g_cpuHistIdx + 1) % CPU_HIST_LEN;
}

static void fmt_bytes(SIZE_T bytes, wchar_t* out, size_t cap) {
    double v = (double)bytes;
    if (v < 1024.0)                _snwprintf_s(out, cap, _TRUNCATE, L"%.0f B",  v);
    else if (v < 1024.0 * 1024)    _snwprintf_s(out, cap, _TRUNCATE, L"%.0f KB", v / 1024.0);
    else if (v < 1024.0 * 1024 * 1024) {
        double mb = v / (1024.0 * 1024);
        if (mb < 1000) _snwprintf_s(out, cap, _TRUNCATE, L"%.0f M",  mb);
        else           _snwprintf_s(out, cap, _TRUNCATE, L"%.2f GB", mb / 1024.0);
    } else {
        _snwprintf_s(out, cap, _TRUNCATE, L"%.2f GB", v / (1024.0 * 1024 * 1024));
    }
}

/* ---- Popover window -------------------------------------------------- */

#define MON_W   540
#define MON_H   720

static HWND g_monitorHwnd = NULL;

static void monitor_paint(HDC dc, RECT cli) {
    /* Background */
    HBRUSH bg = CreateSolidBrush(COL_BG_SIDE);
    FillRect(dc, &cli, bg);
    DeleteObject(bg);

    int x = 22, y = 18;

    /* Header */
    draw_text_w(dc, x, y, L"\U0001F4CA  Consumo de Recursos",
                COL_FG, g_app.hFontBig);
    SIZE titleSz;
    HFONT oldF = (HFONT)SelectObject(dc, g_app.hFontBig);
    GetTextExtentPoint32W(dc, L"\U0001F4CA  Consumo de Recursos",
                          22, &titleSz);
    SelectObject(dc, oldF);
    draw_text_w(dc, MON_W - 150, y + 8, L"Atualiza a cada 1s",
                COL_FG_DIM, g_app.hFontUI);

    y += titleSz.cy + 12;

    HPEN pen = CreatePen(PS_SOLID, 1, COL_DIV);
    HGDIOBJ op = SelectObject(dc, pen);
    MoveToEx(dc, 16, y, NULL);
    LineTo(dc, MON_W - 16, y);
    SelectObject(dc, op);
    DeleteObject(pen);

    y += 18;

    /* CPU */
    draw_text_w(dc, x, y, L"⊙  CPU", COL_FG, g_app.hFontUIBold);
    wchar_t cpuStr[32];
    _snwprintf_s(cpuStr, 32, _TRUNCATE, L"%.1f%%", g_lastTotalCpu);
    HFONT oldB = (HFONT)SelectObject(dc, g_app.hFontBig);
    SIZE cpuSz;
    GetTextExtentPoint32W(dc, cpuStr, (int)wcslen(cpuStr), &cpuSz);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_lastTotalCpu > 80 ? COL_RED
                  : g_lastTotalCpu > 40 ? COL_ORANGE
                  : COL_GREEN);
    TextOutW(dc, MON_W - cpuSz.cx - 22, y - 4, cpuStr, (int)wcslen(cpuStr));
    SelectObject(dc, oldB);
    y += 26;

    /* Sparkline */
    RECT spark = { x, y, MON_W - 22, y + 70 };
    fill_rect_color(dc, &spark, RGB(0x10, 0x10, 0x18));
    HPEN linePen = CreatePen(PS_SOLID, 2, COL_GREEN);
    HGDIOBJ olp = SelectObject(dc, linePen);
    int sparkW = spark.right - spark.left;
    int sparkH = spark.bottom - spark.top;
    int prevX = -1, prevY = -1;
    for (int i = 0; i < CPU_HIST_LEN; ++i) {
        int slot = (g_cpuHistIdx + i) % CPU_HIST_LEN;
        double v = g_cpuHist[slot] / 100.0;
        int px = spark.left + (sparkW * i) / (CPU_HIST_LEN - 1);
        int py = spark.bottom - (int)(sparkH * v) - 1;
        if (prevX >= 0) {
            MoveToEx(dc, prevX, prevY, NULL);
            LineTo(dc, px, py);
        }
        prevX = px; prevY = py;
    }
    SelectObject(dc, olp);
    DeleteObject(linePen);
    y += 78;

    draw_text_w(dc, x, y,
                L"100% = 1 núcleo. Topo do gráfico = todos os núcleos saturados.",
                COL_FG_DIM, g_app.hFontUI);
    y += 26;

    /* Memory */
    draw_text_w(dc, x, y, L"\U0001F4BE  Memória", COL_FG, g_app.hFontUIBold);
    wchar_t memStr[32];
    fmt_bytes(g_lastTotalMem, memStr, 32);
    oldB = (HFONT)SelectObject(dc, g_app.hFontBig);
    SIZE memSz;
    GetTextExtentPoint32W(dc, memStr, (int)wcslen(memStr), &memSz);
    SetTextColor(dc, COL_RED);
    TextOutW(dc, MON_W - memSz.cx - 22, y - 4, memStr, (int)wcslen(memStr));
    SelectObject(dc, oldB);
    y += 26;

    /* Memory bar: App in blue, Shells in magenta */
    RECT mbar = { x, y, MON_W - 22, y + 18 };
    fill_rect_color(dc, &mbar, RGB(0x10, 0x10, 0x18));
    if (g_lastTotalMem > 0) {
        double appFrac   = (double)g_lastAppMem   / (double)g_lastTotalMem;
        double shellFrac = (double)g_lastShellMem / (double)g_lastTotalMem;
        int barW = mbar.right - mbar.left;
        int appW   = (int)(barW * appFrac);
        int shellW = (int)(barW * shellFrac);
        RECT a = { mbar.left, mbar.top, mbar.left + appW, mbar.bottom };
        RECT s = { a.right,    mbar.top, a.right + shellW, mbar.bottom };
        fill_rect_color(dc, &a, COL_ACCENT);
        fill_rect_color(dc, &s, COL_MAGENTA);
    }
    y += 26;

    /* Legend */
    wchar_t appLbl[32], shLbl[32];
    fmt_bytes(g_lastAppMem,  appLbl, 32);
    fmt_bytes(g_lastShellMem, shLbl, 32);
    HBRUSH dotA = CreateSolidBrush(COL_ACCENT);
    RECT da = { x, y + 5, x + 10, y + 15 };
    FillRect(dc, &da, dotA);
    DeleteObject(dotA);
    wchar_t legA[64];
    _snwprintf_s(legA, 64, _TRUNCATE, L"App  %s", appLbl);
    draw_text_w(dc, x + 16, y + 2, legA, COL_FG, g_app.hFontUI);

    HBRUSH dotS = CreateSolidBrush(COL_MAGENTA);
    RECT ds = { x + 160, y + 5, x + 170, y + 15 };
    FillRect(dc, &ds, dotS);
    DeleteObject(dotS);
    wchar_t legS[64];
    _snwprintf_s(legS, 64, _TRUNCATE, L"Shells  %s", shLbl);
    draw_text_w(dc, x + 176, y + 2, legS, COL_FG, g_app.hFontUI);
    y += 28;

    draw_text_w(dc, x, y,
                L"App = processo Dante CLI. Shells = soma de cada terminal +",
                COL_FG_DIM, g_app.hFontUI);
    draw_text_w(dc, x, y + 16,
                L"processos filhos (npm, node, git, etc.).",
                COL_FG_DIM, g_app.hFontUI);
    y += 38;

    /* Divider */
    HPEN p2 = CreatePen(PS_SOLID, 1, COL_DIV);
    HGDIOBJ op2 = SelectObject(dc, p2);
    MoveToEx(dc, 16, y, NULL);
    LineTo(dc, MON_W - 16, y);
    SelectObject(dc, op2);
    DeleteObject(p2);
    y += 18;

    /* Tabs header */
    draw_text_w(dc, x, y, L"\U0001F4BB  Terminais ativos",
                COL_FG, g_app.hFontUIBold);
    wchar_t cnt[32];
    _snwprintf_s(cnt, 32, _TRUNCATE, L"%d shell(s)", g_app.tabCount);
    SIZE cs;
    HFONT oldU = (HFONT)SelectObject(dc, g_app.hFontUI);
    GetTextExtentPoint32W(dc, cnt, (int)wcslen(cnt), &cs);
    SetTextColor(dc, COL_FG_DIM);
    TextOutW(dc, MON_W - cs.cx - 22, y + 2, cnt, (int)wcslen(cnt));
    SelectObject(dc, oldU);
    y += 26;

    /* Tab cards — 2 columns */
    int cardW = (MON_W - 22 - 22 - 12) / 2;
    int cardH = 56;
    for (int i = 0; i < g_app.tabCount && y + cardH < cli.bottom - 12; ++i) {
        Tab* t = g_app.tabs[i];
        if (!t) continue;
        int col = i % 2;
        int row = i / 2;
        RECT card;
        card.left  = x + col * (cardW + 12);
        card.top   = y + row * (cardH + 8);
        card.right = card.left + cardW;
        card.bottom= card.top + cardH;
        if (card.bottom >= cli.bottom - 8) break;

        COLORREF acc = (t->colorIdx >= 0) ? kTabColors[t->colorIdx] : COL_ACCENT;
        draw_rounded_rect(dc, card, COL_BG_CHIP, mix_color(acc, RGB(0,0,0), 0.40), 8);

        if (t->emoji[0])
            draw_text_w(dc, card.left + 10, card.top + 6,
                        t->emoji, COL_FG, g_app.hFontEmoji);
        wchar_t name[64];
        size_t nl = wcslen(t->title);
        if (nl > 18) { wcsncpy_s(name, 64, t->title, 17); name[17] = L'…'; name[18] = 0; }
        else         wcscpy_s(name, 64, t->title);
        draw_text_w(dc, card.left + 38, card.top + 6,
                    name, COL_FG, g_app.hFontUIBold);

        wchar_t info[64];
        wchar_t memBuf[16];
        fmt_bytes(t->cachedMemBytes, memBuf, 16);
        _snwprintf_s(info, 64, _TRUNCATE, L"%s  ·  %.0f%%",
                     memBuf, t->cachedCpuPct);
        draw_text_w(dc, card.left + 38, card.top + 28,
                    info, COL_FG_DIM, g_app.hFontUI);
    }
}

static LRESULT CALLBACK MonitorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hWnd, &ps);
            RECT cli; GetClientRect(hWnd, &cli);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cli.right, cli.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
            monitor_paint(mem, cli);
            BitBlt(dc, 0, 0, cli.right, cli.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) DestroyWindow(hWnd);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            g_monitorHwnd = NULL;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_resource_monitor(int anchorX, int anchorY) {
    if (g_monitorHwnd) {
        DestroyWindow(g_monitorHwnd);
        return;
    }
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = MonitorWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"DanteResourceMonitor";
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    resmon_sample();

    /* Anchor below the chip; flip if overflowing the screen. */
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = anchorX - MON_W + 20;
    int y = anchorY + 8;
    if (x < 8) x = 8;
    if (x + MON_W > sw - 8) x = sw - MON_W - 8;
    if (y + MON_H > sh - 8) y = anchorY - MON_H - 8;

    g_monitorHwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"DanteResourceMonitor", L"Consumo de Recursos",
        WS_POPUP | WS_BORDER,
        x, y, MON_W, MON_H,
        g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(g_monitorHwnd, SW_SHOW);
    SetForegroundWindow(g_monitorHwnd);
}

/* =====================================================================
 *                       EDITOR PREVIEW (text drop)
 *
 * When the user drops a text-like file on the window, instead of just
 * injecting the path into the active terminal we open a non-modal popup
 * with the contents in a multiline EDIT control (Cascadia Code, dark).
 * Capped at 512 KB and 32-bit safe. Read-only by default; toggle to
 * read-write via the "Editar" button — Ctrl+S salva no mesmo arquivo.
 * ===================================================================== */

static BOOL is_text_file(const wchar_t* path) {
    const wchar_t* dot = wcsrchr(path, L'.');
    if (!dot) return FALSE;
    static const wchar_t* exts[] = {
        L".txt", L".md", L".markdown", L".json", L".jsonc", L".yaml", L".yml",
        L".toml", L".ini", L".cfg", L".conf", L".log", L".csv", L".tsv",
        L".c", L".h", L".cpp", L".hpp", L".cc", L".hh", L".cs", L".java",
        L".py", L".rb", L".js", L".jsx", L".ts", L".tsx", L".go", L".rs",
        L".php", L".kt", L".swift", L".m", L".mm", L".sh", L".bash", L".zsh",
        L".ps1", L".psm1", L".bat", L".cmd",
        L".html", L".htm", L".css", L".scss", L".sass", L".xml", L".svg",
        L".sql", L".graphql", L".proto", L".env", L".gitignore",
        L".lua", L".pl", L".tcl", L".vim", L".el"
    };
    for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
        if (_wcsicmp(dot, exts[i]) == 0) return TRUE;
    }
    return FALSE;
}

typedef struct {
    HWND     hWnd;
    HWND     hEdit;
    HWND     hPathLabel;
    HWND     hSaveBtn;
    HWND     hCloseBtn;
    HWND     hToggleBtn;
    wchar_t  path[MAX_PATH];
    BOOL     readonly;
    BOOL     dirty;
} EditorCtx;

static EditorCtx* g_editorCtx = NULL;

static BOOL load_file_utf8_to_wstring(const wchar_t* path, wchar_t** outBuf, size_t* outChars) {
    *outBuf = NULL; *outChars = 0;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    if (sz.QuadPart > 512 * 1024) { CloseHandle(f); return FALSE; }
    DWORD n = (DWORD)sz.QuadPart;
    char* raw = (char*)malloc(n + 1);
    DWORD got = 0;
    if (!ReadFile(f, raw, n, &got, NULL)) { free(raw); CloseHandle(f); return FALSE; }
    raw[got] = 0;
    CloseHandle(f);

    /* Strip UTF-8 BOM if present. */
    char* start = raw;
    if (got >= 3 && (unsigned char)raw[0] == 0xEF &&
        (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        start += 3; got -= 3;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, start, (int)got, NULL, 0);
    if (wlen < 0) { free(raw); return FALSE; }
    wchar_t* buf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, start, (int)got, buf, wlen);
    buf[wlen] = 0;
    free(raw);

    /* EDIT control needs \r\n. Normalize any lone \n. */
    size_t crlfLen = 0;
    for (int i = 0; i < wlen; ++i) {
        if (buf[i] == L'\n' && (i == 0 || buf[i-1] != L'\r')) crlfLen++;
        crlfLen++;
    }
    wchar_t* norm = (wchar_t*)malloc((crlfLen + 1) * sizeof(wchar_t));
    size_t o = 0;
    for (int i = 0; i < wlen; ++i) {
        if (buf[i] == L'\n' && (i == 0 || buf[i-1] != L'\r')) norm[o++] = L'\r';
        norm[o++] = buf[i];
    }
    norm[o] = 0;
    free(buf);

    *outBuf = norm;
    *outChars = o;
    return TRUE;
}

static BOOL save_wstring_to_utf8(const wchar_t* path, const wchar_t* text) {
    int nutf = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (nutf <= 0) return FALSE;
    char* utf = (char*)malloc(nutf);
    WideCharToMultiByte(CP_UTF8, 0, text, -1, utf, nutf, NULL, NULL);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { free(utf); return FALSE; }
    DWORD wrote = 0;
    BOOL ok = WriteFile(f, utf, nutf - 1, &wrote, NULL);
    CloseHandle(f);
    free(utf);
    return ok;
}

static WNDPROC g_origEditorEditProc = NULL;

static LRESULT CALLBACK EditorEditSubclass(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == 'S' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (g_editorCtx && !g_editorCtx->readonly) {
            SendMessageW(GetParent(h), WM_COMMAND, MAKEWPARAM(601, 0), 0); /* Save */
        }
        return 0;
    }
    if (m == WM_KEYDOWN && w == VK_ESCAPE) {
        SendMessageW(GetParent(h), WM_CLOSE, 0, 0);
        return 0;
    }
    return CallWindowProcW(g_origEditorEditProc, h, m, w, l);
}

static void editor_relayout(HWND hWnd) {
    RECT cli; GetClientRect(hWnd, &cli);
    int topPad = 38, bottomPad = 56;
    if (g_editorCtx->hPathLabel)
        SetWindowPos(g_editorCtx->hPathLabel, NULL, 14, 10,
                     cli.right - 28, 24, SWP_NOZORDER);
    if (g_editorCtx->hEdit)
        SetWindowPos(g_editorCtx->hEdit, NULL,
                     14, topPad,
                     cli.right - 28, cli.bottom - topPad - bottomPad,
                     SWP_NOZORDER);
    if (g_editorCtx->hToggleBtn)
        SetWindowPos(g_editorCtx->hToggleBtn, NULL,
                     14, cli.bottom - 44, 110, 32, SWP_NOZORDER);
    if (g_editorCtx->hSaveBtn)
        SetWindowPos(g_editorCtx->hSaveBtn, NULL,
                     130, cli.bottom - 44, 90, 32, SWP_NOZORDER);
    if (g_editorCtx->hCloseBtn)
        SetWindowPos(g_editorCtx->hCloseBtn, NULL,
                     cli.right - 14 - 90, cli.bottom - 44, 90, 32, SWP_NOZORDER);
}

static LRESULT CALLBACK EditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT f = g_app.hFontUI;

            g_editorCtx->hPathLabel = CreateWindowExW(0, L"STATIC", g_editorCtx->path,
                WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
                0, 0, 0, 0, hWnd, NULL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_editorCtx->hPathLabel, WM_SETFONT, (WPARAM)f, TRUE);

            g_editorCtx->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_WANTRETURN |
                WS_VSCROLL | WS_HSCROLL | ES_AUTOVSCROLL | ES_AUTOHSCROLL |
                ES_READONLY,
                0, 0, 0, 0, hWnd, NULL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_editorCtx->hEdit, WM_SETFONT, (WPARAM)g_app.hFontMono, TRUE);
            SendMessageW(g_editorCtx->hEdit, EM_SETLIMITTEXT, (WPARAM)(512 * 1024), 0);

            g_editorCtx->hToggleBtn = CreateWindowExW(0, L"BUTTON", L"✎  Editar",
                WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)600, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_editorCtx->hToggleBtn, WM_SETFONT, (WPARAM)f, TRUE);

            g_editorCtx->hSaveBtn = CreateWindowExW(0, L"BUTTON", L"💾 Salvar",
                WS_CHILD | WS_DISABLED,
                0, 0, 0, 0, hWnd, (HMENU)601, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_editorCtx->hSaveBtn, WM_SETFONT, (WPARAM)f, TRUE);

            g_editorCtx->hCloseBtn = CreateWindowExW(0, L"BUTTON", L"Fechar",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDCANCEL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_editorCtx->hCloseBtn, WM_SETFONT, (WPARAM)f, TRUE);

            g_origEditorEditProc = (WNDPROC)SetWindowLongPtrW(
                g_editorCtx->hEdit, GWLP_WNDPROC, (LONG_PTR)EditorEditSubclass);

            /* Load file content */
            wchar_t* text = NULL; size_t n = 0;
            if (load_file_utf8_to_wstring(g_editorCtx->path, &text, &n) && text) {
                SetWindowTextW(g_editorCtx->hEdit, text);
                free(text);
            } else {
                SetWindowTextW(g_editorCtx->hEdit,
                    L"(Não foi possível abrir o arquivo — talvez seja maior que 512 KB ou tenha codificação não-UTF8.)");
            }
            g_editorCtx->dirty = FALSE;
            editor_relayout(hWnd);
            SetFocus(g_editorCtx->hEdit);
            return 0;
        }
        case WM_SIZE: editor_relayout(hWnd); return 0;
        case WM_COMMAND:
            if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_editorCtx->hEdit) {
                g_editorCtx->dirty = TRUE;
                return 0;
            }
            if (LOWORD(wParam) == 600) {
                /* Toggle read/write */
                g_editorCtx->readonly = !g_editorCtx->readonly;
                SendMessageW(g_editorCtx->hEdit, EM_SETREADONLY,
                             g_editorCtx->readonly, 0);
                SetWindowTextW(g_editorCtx->hToggleBtn,
                    g_editorCtx->readonly ? L"✎  Editar" : L"🔒 Somente leitura");
                EnableWindow(g_editorCtx->hSaveBtn, !g_editorCtx->readonly);
            } else if (LOWORD(wParam) == 601) {
                /* Save */
                int len = GetWindowTextLengthW(g_editorCtx->hEdit);
                wchar_t* buf = (wchar_t*)malloc((len + 2) * sizeof(wchar_t));
                GetWindowTextW(g_editorCtx->hEdit, buf, len + 1);
                BOOL ok = save_wstring_to_utf8(g_editorCtx->path, buf);
                free(buf);
                MessageBoxW(hWnd,
                    ok ? L"Arquivo salvo." : L"Falha ao salvar (arquivo pode estar protegido).",
                    L"Editor", ok ? MB_ICONINFORMATION : MB_ICONERROR);
                if (ok) g_editorCtx->dirty = FALSE;
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:
            if (g_editorCtx->dirty) {
                int r = MessageBoxW(hWnd,
                    L"Você tem alterações não salvas.\nFechar mesmo assim?",
                    L"Editor", MB_ICONQUESTION | MB_YESNO);
                if (r != IDYES) return 0;
            }
            DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            free(g_editorCtx);
            g_editorCtx = NULL;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_editor_preview(const wchar_t* path) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = EditorWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"DanteEditor";
        wc.hIcon = LoadIconW(GetModuleHandleW(NULL), L"DanteIcon");
        wc.hIconSm = wc.hIcon;
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    if (g_editorCtx && g_editorCtx->hWnd && IsWindow(g_editorCtx->hWnd)) {
        DestroyWindow(g_editorCtx->hWnd);
    }
    g_editorCtx = (EditorCtx*)xcalloc(1, sizeof(EditorCtx));
    str_copy_w(g_editorCtx->path, MAX_PATH, path);
    g_editorCtx->readonly = TRUE;

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int W = 900, H = 640;
    int x = pr.left + ((pr.right - pr.left) - W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - H) / 2;

    /* Use only the filename in the title for brevity. */
    const wchar_t* base = wcsrchr(path, L'\\');
    wchar_t title[MAX_PATH + 16];
    _snwprintf_s(title, MAX_PATH + 16, _TRUNCATE,
                 L"%s — Dante CLI Editor", base ? base + 1 : path);

    g_editorCtx->hWnd = CreateWindowExW(0, L"DanteEditor", title,
        WS_OVERLAPPEDWINDOW,
        x, y, W, H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(g_editorCtx->hWnd, SW_SHOW);
    SetForegroundWindow(g_editorCtx->hWnd);
}

/* =========================================================================
 *                       SPLIT WORKSPACE
 * ========================================================================= */

static void set_split_layout(int presetIdx) {
    if (presetIdx < 0 || presetIdx >= PRESET_COUNT) return;
    g_app.splitLayout = presetIdx;
    g_app.splitActiveCell = 0;
    g_app.zoomedCell = -1;     /* reset zoom whenever the layout changes */
    memset(g_app.customCells, 0, sizeof(g_app.customCells));
    int cells = kPresets[presetIdx].cellCount;
    /* Auto-fill slots with the first N tabs */
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) g_app.splitSlots[i] = -1;
    for (int i = 0; i < cells && i < g_app.tabCount; ++i) g_app.splitSlots[i] = i;
    InvalidateRect(g_app.hWnd, NULL, FALSE);
    update_status();
}

enum {
    IDM_SPLIT_BASE = 0x1300,   /* +0..PRESET_COUNT-1 */
};

/* =====================================================================
 *                       VISUAL LAYOUT GALLERY
 *
 * Custom popup window that mirrors the macOS gallery: header with title,
 * 4 category pills, and a 3-column grid of cards. Each card renders a
 * miniature of the layout (blue rects on a dark canvas) followed by the
 * name and "N painéis" subtitle. Click a card → applies the preset.
 * ===================================================================== */

#define GAL_W            960
#define GAL_H            720
#define GAL_HDR_H        72
#define GAL_PILLS_H      56
#define GAL_FOOTER_H     44
#define GAL_CARD_W       280
#define GAL_CARD_H       190
#define GAL_CARD_GAP     14
#define GAL_GRID_PAD     20
#define GAL_THUMB_W      180
#define GAL_THUMB_H      100

#define COL_GAL_BG       RGB(0x16, 0x16, 0x1E)
#define COL_GAL_CARD     RGB(0x1F, 0x23, 0x35)
#define COL_GAL_CARD_H   RGB(0x2A, 0x2F, 0x45)
#define COL_GAL_CARD_BD  RGB(0x29, 0x2E, 0x42)
#define COL_GAL_THUMB_BG RGB(0x10, 0x11, 0x18)
#define COL_GAL_THUMB_FG RGB(0x1F, 0x77, 0xFF)
#define COL_GAL_PILL     RGB(0x24, 0x28, 0x3B)
#define COL_GAL_PILL_AC  RGB(0x1F, 0x77, 0xFF)

typedef struct {
    HWND          hWnd;
    SplitCategory activeCat;
    int           hoverCardIdx;
    int           cardPresets[PRESET_COUNT];   /* indices into kPresets for current cat */
    int           cardCount;
} GalleryCtx;

static GalleryCtx* g_galCtx = NULL;

static void gallery_rebuild_cards(void) {
    g_galCtx->cardCount = 0;
    for (int i = 0; i < PRESET_COUNT; ++i) {
        if (i == PRESET_SINGLE) continue;
        if (kPresets[i].category != g_galCtx->activeCat) continue;
        g_galCtx->cardPresets[g_galCtx->cardCount++] = i;
    }
}

static RECT gallery_pill_rect(int idx) {
    /* 4 pills, ~110 px each, gap 8, anchored to the left after the back btn. */
    RECT r;
    int w = 124;
    int gap = 10;
    r.left = 24 + idx * (w + gap);
    r.top = GAL_HDR_H + 8;
    r.right = r.left + w;
    r.bottom = r.top + GAL_PILLS_H - 16;
    return r;
}

static RECT gallery_card_rect(int idx) {
    /* 3 columns, rows wrap. */
    int col = idx % 3;
    int row = idx / 3;
    RECT r;
    r.left = GAL_GRID_PAD + col * (GAL_CARD_W + GAL_CARD_GAP);
    r.top  = GAL_HDR_H + GAL_PILLS_H + row * (GAL_CARD_H + GAL_CARD_GAP) + 8;
    r.right  = r.left + GAL_CARD_W;
    r.bottom = r.top  + GAL_CARD_H;
    return r;
}

static void gallery_paint_thumbnail(HDC dc, RECT canvas, const SplitPreset* preset) {
    /* Dark canvas */
    HBRUSH bg = CreateSolidBrush(COL_GAL_THUMB_BG);
    FillRect(dc, &canvas, bg);
    DeleteObject(bg);

    /* Render each cell as a rounded blue rect, inset 4 px on all sides
     * so the cells look distinct.                                    */
    int W = canvas.right - canvas.left;
    int H = canvas.bottom - canvas.top;
    HBRUSH cellBr = CreateSolidBrush(COL_GAL_THUMB_FG);
    HPEN   noPen  = (HPEN)GetStockObject(NULL_PEN);
    HBRUSH oBr = (HBRUSH)SelectObject(dc, cellBr);
    HPEN   oPen = (HPEN)SelectObject(dc, noPen);

    for (int i = 0; i < preset->cellCount; ++i) {
        const SplitCell* c = &preset->cells[i];
        int x = canvas.left + (W * c->x) / 100 + 2;
        int y = canvas.top  + (H * c->y) / 100 + 2;
        int r = (c->x + c->w >= 100) ? canvas.right  : canvas.left + (W * (c->x + c->w)) / 100;
        int b = (c->y + c->h >= 100) ? canvas.bottom : canvas.top  + (H * (c->y + c->h)) / 100;
        r -= 2; b -= 2;
        if (r <= x || b <= y) continue;
        RoundRect(dc, x, y, r, b, 6, 6);
    }
    SelectObject(dc, oBr); SelectObject(dc, oPen);
    DeleteObject(cellBr);
}

static int gallery_hit_test_card(int x, int y) {
    for (int i = 0; i < g_galCtx->cardCount; ++i) {
        RECT r = gallery_card_rect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static int gallery_hit_test_pill(int x, int y) {
    for (int i = 0; i < SCAT_COUNT; ++i) {
        RECT r = gallery_pill_rect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static LRESULT CALLBACK GalleryWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hWnd, &ps);
            RECT cli; GetClientRect(hWnd, &cli);

            /* Double-buffer */
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cli.right, cli.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            /* Background */
            fill_rect_color(mem, &cli, COL_GAL_BG);

            /* Header — title + close hint */
            draw_text_w(mem, 28, 22, L"Galeria de layouts", COL_FG, g_app.hFontBig);
            draw_text_w(mem, 28, 52, L"Click num modelo pra aplicá-lo  ·  Esc para fechar",
                        COL_FG_DIM, g_app.hFontUI);

            /* Pills row */
            for (int i = 0; i < SCAT_COUNT; ++i) {
                RECT p = gallery_pill_rect(i);
                BOOL active = (g_galCtx->activeCat == (SplitCategory)i);
                draw_rounded_rect(mem, p,
                                  active ? COL_GAL_PILL_AC : COL_GAL_PILL,
                                  0, 18);
                SIZE sz;
                HFONT old = (HFONT)SelectObject(mem, g_app.hFontUI);
                SetBkMode(mem, TRANSPARENT);
                SetTextColor(mem, active ? RGB(255,255,255) : COL_FG);
                const wchar_t* lbl = kSplitCategoryNames[i];
                GetTextExtentPoint32W(mem, lbl, (int)wcslen(lbl), &sz);
                TextOutW(mem,
                         p.left + ((p.right - p.left) - sz.cx) / 2,
                         p.top  + ((p.bottom - p.top) - sz.cy) / 2,
                         lbl, (int)wcslen(lbl));
                SelectObject(mem, old);
            }

            /* Card grid */
            for (int i = 0; i < g_galCtx->cardCount; ++i) {
                int presetIdx = g_galCtx->cardPresets[i];
                const SplitPreset* preset = &kPresets[presetIdx];
                BOOL hover = (g_galCtx->hoverCardIdx == i);
                BOOL selected = (g_app.splitLayout == presetIdx);

                RECT card = gallery_card_rect(i);
                draw_rounded_rect(mem, card,
                                  hover ? COL_GAL_CARD_H : COL_GAL_CARD,
                                  selected ? COL_GAL_PILL_AC : COL_GAL_CARD_BD,
                                  12);

                /* Thumbnail centered */
                RECT thumb;
                thumb.left = card.left + ((card.right - card.left) - GAL_THUMB_W) / 2;
                thumb.top  = card.top  + 18;
                thumb.right  = thumb.left + GAL_THUMB_W;
                thumb.bottom = thumb.top  + GAL_THUMB_H;
                gallery_paint_thumbnail(mem, thumb, preset);

                /* Name */
                draw_text_w(mem, card.left + 16, card.top + 130,
                            preset->name, COL_FG, g_app.hFontUI);

                /* Subtitle: "N painéis" */
                wchar_t sub[32];
                _snwprintf_s(sub, 32, _TRUNCATE, L"%d %s",
                             preset->cellCount,
                             preset->cellCount == 1 ? L"painel" : L"painéis");
                draw_text_w(mem, card.left + 16, card.top + 152,
                            sub, COL_FG_DIM, g_app.hFontUI);
            }

            BitBlt(dc, 0, 0, cli.right, cli.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int hover = gallery_hit_test_card(x, y);
            if (hover != g_galCtx->hoverCardIdx) {
                g_galCtx->hoverCardIdx = hover;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int pill = gallery_hit_test_pill(x, y);
            if (pill >= 0) {
                g_galCtx->activeCat = (SplitCategory)pill;
                gallery_rebuild_cards();
                g_galCtx->hoverCardIdx = -1;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            int card = gallery_hit_test_card(x, y);
            if (card >= 0) {
                set_split_layout(g_galCtx->cardPresets[card]);
                DestroyWindow(hWnd);
                return 0;
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) { DestroyWindow(hWnd); return 0; }
            /* Arrow keys cycle through cards visually */
            if (wParam == VK_LEFT)  {
                int n = g_galCtx->cardCount; if (n > 0) {
                    g_galCtx->hoverCardIdx = (g_galCtx->hoverCardIdx <= 0)
                        ? n - 1 : g_galCtx->hoverCardIdx - 1;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }
            if (wParam == VK_RIGHT) {
                int n = g_galCtx->cardCount; if (n > 0) {
                    g_galCtx->hoverCardIdx = (g_galCtx->hoverCardIdx + 1) % n;
                    InvalidateRect(hWnd, NULL, FALSE);
                }
                return 0;
            }
            if (wParam == VK_RETURN && g_galCtx->hoverCardIdx >= 0) {
                set_split_layout(g_galCtx->cardPresets[g_galCtx->hoverCardIdx]);
                DestroyWindow(hWnd);
                return 0;
            }
            return 0;
        case WM_CLOSE:    DestroyWindow(hWnd); return 0;
        case WM_DESTROY:
            PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_layout_gallery(void) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = GalleryWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_HAND);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"DanteLayoutGallery";
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    GalleryCtx ctx = {0};
    ctx.activeCat = SCAT_SIMPLE;
    ctx.hoverCardIdx = -1;
    /* If current layout has a category, open on that tab. */
    if (g_app.splitLayout > 0 && g_app.splitLayout < PRESET_COUNT)
        ctx.activeCat = kPresets[g_app.splitLayout].category;
    g_galCtx = &ctx;
    gallery_rebuild_cards();

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int x = pr.left + ((pr.right - pr.left) - GAL_W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - GAL_H) / 2;

    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"DanteLayoutGallery", L"Galeria de layouts — Dante CLI",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, GAL_W, GAL_H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ctx.hWnd = dlg;

    ShowWindow(dlg, SW_SHOW);
    SetFocus(dlg);
    EnableWindow(g_app.hWnd, FALSE);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(dlg)) break;
    }
    EnableWindow(g_app.hWnd, TRUE);
    SetForegroundWindow(g_app.hWnd);
    g_galCtx = NULL;
}

/* Build a categorised popup: each category is a submenu, presets inside it
 * are sorted by their position in kPresets[]. */
static void show_split_menu(HWND hWnd, int x, int y) {
    HMENU root = CreatePopupMenu();

    /* "Sem split" goes to the top level for quick access. */
    AppendMenuW(root, MF_STRING | (g_app.splitLayout == PRESET_SINGLE ? MF_CHECKED : 0),
                IDM_SPLIT_BASE + PRESET_SINGLE, kPresets[PRESET_SINGLE].name);
    AppendMenuW(root, MF_SEPARATOR, 0, NULL);

    for (int cat = 0; cat < SCAT_COUNT; ++cat) {
        HMENU sub = CreatePopupMenu();
        int hasItems = 0;
        for (int i = 0; i < PRESET_COUNT; ++i) {
            if (i == PRESET_SINGLE) continue;
            if (kPresets[i].category != cat) continue;
            wchar_t label[160];
            _snwprintf_s(label, 160, _TRUNCATE, L"%s   (%d painéis)",
                         kPresets[i].name, kPresets[i].cellCount);
            AppendMenuW(sub, MF_STRING | (g_app.splitLayout == i ? MF_CHECKED : 0),
                        IDM_SPLIT_BASE + i, label);
            hasItems = 1;
        }
        if (hasItems) {
            AppendMenuW(root, MF_POPUP, (UINT_PTR)sub, kSplitCategoryNames[cat]);
        } else {
            DestroyMenu(sub);
        }
    }

    POINT pt = { x, y };
    ClientToScreen(hWnd, &pt);
    int cmd = TrackPopupMenu(root, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, hWnd, NULL);
    DestroyMenu(root);
    if (cmd >= IDM_SPLIT_BASE && cmd < IDM_SPLIT_BASE + PRESET_COUNT) {
        set_split_layout(cmd - IDM_SPLIT_BASE);
    }
}

/* =========================================================================
 *                       CHEATSHEET MODAL
 * ========================================================================= */

static void cheatsheet_toggle(void) {
    g_app.cheatsheetVisible = !g_app.cheatsheetVisible;
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

typedef struct { const wchar_t* keys; const wchar_t* desc; } CheatRow;

/* Voice module functions/state live further down — use accessor wrappers
 * here so we don't need to forward-declare the whole struct.            */
static BOOL  voice_is_recording(void);
static BOOL  voice_is_uploading(void);
static DWORD voice_elapsed_ms(void);
static void  voice_stop_and_upload(void);

/* Voice recording / uploading overlay — dim backdrop + big mic + Stop btn. */
static RECT g_voiceStopBtnRect = {0};

static void draw_voice_overlay(HDC dc, const RECT* cli) {
    BOOL rec = voice_is_recording();
    BOOL up  = voice_is_uploading();
    if (!rec && !up) {
        SetRectEmpty(&g_voiceStopBtnRect);
        return;
    }
    /* Dim backdrop */
    HBRUSH dim = CreateSolidBrush(RGB(0,0,0));
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 200, 0 };
    HDC tmp = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, cli->right, cli->bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(tmp, bmp);
    RECT all = *cli; FillRect(tmp, &all, dim);
    AlphaBlend(dc, 0, 0, cli->right, cli->bottom, tmp, 0, 0, cli->right, cli->bottom, bf);
    SelectObject(tmp, oldBmp); DeleteObject(bmp); DeleteDC(tmp); DeleteObject(dim);

    /* Panel */
    int W = 520, H = 280;
    int x = (cli->right - W) / 2;
    int y = (cli->bottom - H) / 2;
    RECT panel = { x, y, x + W, y + H };
    draw_rounded_rect(dc, panel, COL_BG_SIDE,
                      rec ? COL_RED : COL_ACCENT, 16);

    /* Big mic icon */
    HFONT bigEmoji = CreateFontW(-68, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Emoji");
    HFONT old = (HFONT)SelectObject(dc, bigEmoji);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, rec ? COL_RED : COL_ACCENT);
    SIZE sz;
    GetTextExtentPoint32W(dc, L"\U0001F3A4", 2, &sz);
    TextOutW(dc, x + ((W - sz.cx) / 2), y + 24, L"\U0001F3A4", 2);
    SelectObject(dc, old);
    DeleteObject(bigEmoji);

    /* Title */
    SetTextColor(dc, COL_FG);
    HFONT oldB = (HFONT)SelectObject(dc, g_app.hFontBig);
    const wchar_t* title = rec
        ? L"Fale agora, estou ouvindo!"
        : L"Transcrevendo…";
    GetTextExtentPoint32W(dc, title, (int)wcslen(title), &sz);
    TextOutW(dc, x + ((W - sz.cx) / 2), y + 116, title, (int)wcslen(title));
    SelectObject(dc, oldB);

    /* Subtitle + REC timer */
    HFONT oldU = (HFONT)SelectObject(dc, g_app.hFontUI);
    SetTextColor(dc, COL_FG_DIM);
    const wchar_t* sub = rec
        ? L"O texto reconhecido será inserido no terminal ativo."
        : L"Aguarde o retorno da Groq Whisper…";
    GetTextExtentPoint32W(dc, sub, (int)wcslen(sub), &sz);
    TextOutW(dc, x + ((W - sz.cx) / 2), y + 158, sub, (int)wcslen(sub));

    if (rec) {
        DWORD secs = voice_elapsed_ms() / 1000;
        wchar_t t[32];
        _snwprintf_s(t, 32, _TRUNCATE, L"REC  %02lu:%02lu", secs / 60, secs % 60);
        SetTextColor(dc, COL_RED);
        GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
        TextOutW(dc, x + ((W - sz.cx) / 2), y + 180, t, (int)wcslen(t));
    }
    SelectObject(dc, oldU);

    /* Stop / OK button */
    if (rec) {
        RECT stop = { x + (W - 220) / 2, y + H - 60, x + (W + 220) / 2, y + H - 24 };
        draw_rounded_rect(dc, stop, COL_RED, 0, 10);
        oldB = (HFONT)SelectObject(dc, g_app.hFontUIBold);
        SetTextColor(dc, RGB(255,255,255));
        const wchar_t* lbl = L"⏹  Parar e enviar";
        GetTextExtentPoint32W(dc, lbl, (int)wcslen(lbl), &sz);
        TextOutW(dc, stop.left + ((stop.right - stop.left) - sz.cx) / 2,
                 stop.top  + ((stop.bottom - stop.top) - sz.cy) / 2,
                 lbl, (int)wcslen(lbl));
        SelectObject(dc, oldB);
        g_voiceStopBtnRect = stop;
    } else {
        SetRectEmpty(&g_voiceStopBtnRect);
    }
}

static int hit_test_voice_stop(int x, int y) {
    RECT r = g_voiceStopBtnRect;
    if (r.right == r.left || r.bottom == r.top) return 0;
    return (x >= r.left && x < r.right && y >= r.top && y < r.bottom);
}

static void draw_cheatsheet(HDC dc, const RECT* cli) {
    static const CheatRow rows[] = {
        { L"Ctrl+T",        L"Nova aba (PowerShell)" },
        { L"Ctrl+W",        L"Fechar aba ativa" },
        { L"Ctrl+1..9",     L"Pular para aba N" },
        { L"Ctrl+Tab",      L"Próxima aba" },
        { L"Ctrl+Shift+Tab",L"Aba anterior" },
        { L"Ctrl+/",        L"Mostrar/ocultar este atalho" },
        { L"Ctrl+,",        L"Configurações" },
        { L"Ctrl+L",        L"Foco na busca da sidebar" },
        { L"Ctrl+Shift+C",  L"Lançar Claude no terminal ativo" },
        { L"Ctrl+Shift+G",  L"Lançar Gemini no terminal ativo" },
        { L"Ctrl+Shift+K",  L"Limpar linha do terminal" },
        { L"Ctrl+D",        L"Duplicar aba ativa" },
        { L"F1",            L"Sobre o Dante CLI" },
        { L"▦ Layout",      L"Toolbar → grade 1×2, 2×1, 2×2, 1×3" },
        { L"⚙ Config",      L"Tema, Groq API key, fonte, scrollback" },
        { L"\U0001F3A4 Voz", L"Grava → envia Groq Whisper → injeta texto" },
        { L"\U0001F4A1 Explicar", L"Manda última saída → Groq Chat → modal" },
        { L"Botão direito na aba", L"Menu de cor, renomear, duplicar, fechar" },
        { L"Duplo-clique na aba",  L"Renomear" },
        { L"Arrastar aba", L"Reordenar" },
        { L"Backspace",     L"Apagar 1 caractere" },
        { L"Alt+Backspace", L"Apagar palavra (boundary leve)" },
        { L"Ctrl+Backspace",L"Apagar palavra anterior (kill-word)" },
        { L"Ctrl+Shift+Z",  L"Zoom 90% da célula ativa (toggle)" },
        { L"Esc",           L"Sair do zoom (se ativo)" },
    };

    /* Dimmed backdrop */
    HBRUSH dim = CreateSolidBrush(RGB(0, 0, 0));
    int oldMode = SetROP2(dc, R2_COPYPEN);
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 180, 0 };
    HDC tmp = CreateCompatibleDC(dc);
    HBITMAP bmp = CreateCompatibleBitmap(dc, cli->right, cli->bottom);
    HBITMAP oldBmp = (HBITMAP)SelectObject(tmp, bmp);
    RECT all = *cli; FillRect(tmp, &all, dim);
    AlphaBlend(dc, 0, 0, cli->right, cli->bottom, tmp, 0, 0, cli->right, cli->bottom, bf);
    SelectObject(tmp, oldBmp); DeleteObject(bmp); DeleteDC(tmp); DeleteObject(dim);
    SetROP2(dc, oldMode);

    /* Panel */
    int W = 560, H = 450;
    int x = (cli->right - W) / 2;
    int y = (cli->bottom - H) / 2;
    RECT panel = { x, y, x + W, y + H };
    draw_rounded_rect(dc, panel, COL_BG_SIDE, COL_ACCENT, 14);

    draw_text_w(dc, x + 24, y + 18, L"Atalhos do Dante CLI", COL_ACCENT, g_app.hFontBig);
    draw_text_w(dc, x + 24, y + 56,
                L"Pressione Ctrl+/ ou Esc para fechar",
                COL_FG_DIM, g_app.hFontUI);

    int rowY = y + 92;
    int n = sizeof(rows) / sizeof(rows[0]);
    for (int i = 0; i < n; ++i) {
        draw_text_w(dc, x + 24,  rowY, rows[i].keys, COL_ACCENT2, g_app.hFontMono);
        draw_text_w(dc, x + 220, rowY, rows[i].desc, COL_FG,     g_app.hFontUI);
        rowY += 26;
    }
}

/* =========================================================================
 *                       GROQ HTTP CLIENT (chat + whisper)
 *
 * Uses WinHTTP for HTTPS. JSON requests are crafted by hand. Responses are
 * parsed with a tiny "find value of key" search since the Groq response
 * shape is well-known. Results are posted back to the UI thread.
 * ========================================================================= */

#define WM_DANTE_GROQ_RESULT (WM_APP + 2)
#define WM_DANTE_GROQ_ERROR  (WM_APP + 3)

typedef struct {
    char* prompt;        /* UTF-8 user content */
    char* system;        /* UTF-8 system prompt */
    char* apiKey;        /* UTF-8 */
} GroqChatReq;

static char* w_to_utf8_dup(const wchar_t* w) {
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char* out = (char*)malloc(len);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, len, NULL, NULL);
    return out;
}

static wchar_t* utf8_to_w_dup(const char* s, int n) {
    if (n < 0) n = (int)strlen(s);
    int len = MultiByteToWideChar(CP_UTF8, 0, s, n, NULL, 0);
    if (len < 0) return NULL;
    wchar_t* out = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, n, out, len);
    out[len] = 0;
    return out;
}

/* Append `src` to *bufp/buflen (heap-grown). */
static void str_append(char** bufp, size_t* cap, size_t* len, const char* src) {
    size_t add = strlen(src);
    if (*len + add + 1 > *cap) {
        *cap = (*cap + add + 1) * 2;
        *bufp = (char*)realloc(*bufp, *cap);
    }
    memcpy(*bufp + *len, src, add + 1);
    *len += add;
}

static char* json_escape_str(const char* s) {
    size_t cap = strlen(s) * 2 + 16;
    char* out = (char*)malloc(cap);
    size_t o = 0;
    for (size_t i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (o + 8 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c < 0x20)  { o += (size_t)snprintf(out + o, cap - o, "\\u%04x", c); }
        else                { out[o++] = (char)c; }
    }
    out[o] = 0;
    return out;
}

/* Locate the value of "content" key in the Groq chat response and return it
 * as a freshly allocated UTF-8 string. */
static char* groq_extract_content(const char* json) {
    const char* k = strstr(json, "\"content\"");
    if (!k) return NULL;
    k = strchr(k, ':');
    if (!k) return NULL;
    ++k;
    while (*k == ' ' || *k == '\t' || *k == '\n' || *k == '\r') ++k;
    if (*k != '"') return NULL;
    ++k;
    size_t cap = 1024;
    char* out = (char*)malloc(cap);
    size_t o = 0;
    while (*k && *k != '"') {
        if (o + 8 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
        if (*k == '\\' && k[1]) {
            char e = k[1];
            if      (e == 'n')  out[o++] = '\n';
            else if (e == 'r')  out[o++] = '\r';
            else if (e == 't')  out[o++] = '\t';
            else if (e == '"')  out[o++] = '"';
            else if (e == '\\') out[o++] = '\\';
            else if (e == 'u' && k[2] && k[3] && k[4] && k[5]) {
                char hex[5] = {k[2], k[3], k[4], k[5], 0};
                unsigned int code = (unsigned int)strtoul(hex, NULL, 16);
                /* Encode as UTF-8 */
                if (code < 0x80) out[o++] = (char)code;
                else if (code < 0x800) {
                    out[o++] = (char)(0xC0 | (code >> 6));
                    out[o++] = (char)(0x80 | (code & 0x3F));
                } else {
                    out[o++] = (char)(0xE0 | (code >> 12));
                    out[o++] = (char)(0x80 | ((code >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (code & 0x3F));
                }
                k += 4;
            }
            else                out[o++] = e;
            k += 2;
        } else {
            out[o++] = *k++;
        }
    }
    out[o] = 0;
    return out;
}

static char* https_post_json(const wchar_t* host, INTERNET_PORT port, const wchar_t* path,
                              const wchar_t* bearer, const char* body,
                              int* outStatus, int* outLen) {
    *outStatus = 0; *outLen = 0;
    HINTERNET hSession = WinHttpOpen(L"DanteCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return NULL;
    HINTERNET hConn = WinHttpConnect(hSession, host, port, 0);
    if (!hConn) { WinHttpCloseHandle(hSession); return NULL; }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path, NULL,
                                         WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession); return NULL; }

    wchar_t headers[1024];
    _snwprintf_s(headers, 1024, _TRUNCATE,
                 L"Content-Type: application/json\r\nAuthorization: Bearer %s\r\n",
                 bearer);

    BOOL sent = WinHttpSendRequest(hReq, headers, (DWORD)-1L,
                                    (LPVOID)body, (DWORD)strlen(body),
                                    (DWORD)strlen(body), 0);
    if (!sent) { WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession); return NULL; }
    if (!WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD status = 0; DWORD sz = sizeof(status);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    *outStatus = (int)status;

    char* out = NULL; size_t cap = 0, len = 0;
    DWORD avail = 0;
    do {
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        if (len + avail + 1 > cap) { cap = (len + avail + 1) * 2; out = (char*)realloc(out, cap); }
        DWORD got = 0;
        if (!WinHttpReadData(hReq, out + len, avail, &got)) break;
        len += got;
        out[len] = 0;
    } while (avail > 0);
    *outLen = (int)len;

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);
    return out;
}

static DWORD WINAPI groq_chat_thread(LPVOID arg) {
    GroqChatReq* req = (GroqChatReq*)arg;

    char* sysEsc = json_escape_str(req->system);
    char* userEsc = json_escape_str(req->prompt);

    size_t bodyCap = strlen(sysEsc) + strlen(userEsc) + 512;
    char* body = (char*)malloc(bodyCap);
    snprintf(body, bodyCap,
        "{\"model\":\"llama-3.3-70b-versatile\","
        "\"temperature\":0.2,"
        "\"messages\":["
        "{\"role\":\"system\",\"content\":\"%s\"},"
        "{\"role\":\"user\",\"content\":\"%s\"}"
        "]}", sysEsc, userEsc);

    wchar_t bearer[256];
    MultiByteToWideChar(CP_UTF8, 0, req->apiKey, -1, bearer, 256);

    int status = 0, respLen = 0;
    char* resp = https_post_json(L"api.groq.com", INTERNET_DEFAULT_HTTPS_PORT,
                                  L"/openai/v1/chat/completions",
                                  bearer, body, &status, &respLen);

    if (status == 200 && resp) {
        char* content = groq_extract_content(resp);
        if (content) {
            wchar_t* w = utf8_to_w_dup(content, -1);
            PostMessageW(g_app.hWnd, WM_DANTE_GROQ_RESULT, 0, (LPARAM)w);
            free(content);
        } else {
            wchar_t* w = utf8_to_w_dup("Resposta vazia/JSON inesperado.", -1);
            PostMessageW(g_app.hWnd, WM_DANTE_GROQ_ERROR, 0, (LPARAM)w);
        }
    } else {
        char msg[256];
        snprintf(msg, 256, "Erro HTTP %d ao falar com a Groq.\n\n%s",
                 status, resp ? resp : "(sem corpo)");
        wchar_t* w = utf8_to_w_dup(msg, -1);
        PostMessageW(g_app.hWnd, WM_DANTE_GROQ_ERROR, 0, (LPARAM)w);
    }

    free(resp); free(body); free(sysEsc); free(userEsc);
    free(req->prompt); free(req->system); free(req->apiKey); free(req);
    return 0;
}

static BOOL g_groqInFlight = FALSE;

static void groq_explain_active_terminal(void) {
    if (g_app.activeTab < 0) return;
    if (g_groqInFlight) {
        MessageBoxW(g_app.hWnd, L"Já existe uma chamada em andamento.",
                    APP_NAME_W, MB_ICONINFORMATION);
        return;
    }
    if (g_app.groqApiKey[0] == 0) {
        MessageBoxW(g_app.hWnd,
            L"Configure a Groq API key em ⚙ Configurações primeiro.\n\nObtenha em https://console.groq.com/keys",
            APP_NAME_W, MB_ICONWARNING);
        return;
    }

    /* Grab the last N rows of the terminal as plain text */
    Tab* t = g_app.tabs[g_app.activeTab];
    int rows = t->grid.rows;
    int cols = t->grid.cols;
    int firstNonEmpty = 0;
    for (int r = rows - 1; r >= 0; --r) {
        int rowHas = 0;
        for (int c = 0; c < cols; ++c) {
            wchar_t ch = t->grid.cells[r * cols + c].ch;
            if (ch && ch != L' ') { rowHas = 1; break; }
        }
        if (rowHas) { firstNonEmpty = r; }
        if (rows - 1 - r > 25 && firstNonEmpty > 0) break;
    }
    int startRow = max_int(0, firstNonEmpty - 24);

    wchar_t excerpt[8192]; excerpt[0] = 0;
    size_t pos = 0;
    for (int r = startRow; r < rows && pos < 7800; ++r) {
        for (int c = 0; c < cols && pos < 7800; ++c) {
            wchar_t ch = t->grid.cells[r * cols + c].ch;
            excerpt[pos++] = (ch == 0) ? L' ' : ch;
        }
        excerpt[pos++] = L'\n';
    }
    excerpt[pos] = 0;

    GroqChatReq* req = (GroqChatReq*)calloc(1, sizeof(*req));
    req->system = w_to_utf8_dup(
        L"Você é um especialista em terminal Windows e shells (cmd, PowerShell). "
        L"O usuário vai colar a saída recente do terminal. Responda em PORTUGUÊS "
        L"do Brasil, conciso, com bullets quando útil. Identifique erros, "
        L"sugira comandos corretivos e contextualize ferramentas.");
    req->prompt = w_to_utf8_dup(excerpt);
    req->apiKey = w_to_utf8_dup(g_app.groqApiKey);

    g_groqInFlight = TRUE;
    HANDLE h = CreateThread(NULL, 0, groq_chat_thread, req, 0, NULL);
    if (h) CloseHandle(h);

    MessageBoxW(g_app.hWnd,
        L"Consultando Groq…\n\nO resultado aparecerá em uma janela quando estiver pronto.",
        APP_NAME_W, MB_ICONINFORMATION);
}

/* Result modal — read-only multiline edit. */
typedef struct { HWND hEdit; HWND hCopy; HWND hClose; wchar_t* text; } GroqResultCtx;
static GroqResultCtx* g_groqResCtx = NULL;

static LRESULT CALLBACK GroqResWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_groqResCtx->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                g_groqResCtx->text ? g_groqResCtx->text : L"(vazio)",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                12, 12, 596, 376, hWnd, NULL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_groqResCtx->hEdit, WM_SETFONT, (WPARAM)g_app.hFontUI, TRUE);

            g_groqResCtx->hCopy = CreateWindowExW(0, L"BUTTON", L"Copiar tudo",
                WS_CHILD | WS_VISIBLE,
                12, 400, 110, 30, hWnd, (HMENU)301, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_groqResCtx->hCopy, WM_SETFONT, (WPARAM)g_app.hFontUI, TRUE);

            g_groqResCtx->hClose = CreateWindowExW(0, L"BUTTON", L"Fechar",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                518, 400, 90, 30, hWnd, (HMENU)IDCANCEL, GetModuleHandleW(NULL), NULL);
            SendMessageW(g_groqResCtx->hClose, WM_SETFONT, (WPARAM)g_app.hFontUI, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 301) {
                /* Copy text */
                if (g_groqResCtx->text && OpenClipboard(hWnd)) {
                    EmptyClipboard();
                    size_t n = wcslen(g_groqResCtx->text) + 1;
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n * sizeof(wchar_t));
                    if (h) {
                        wchar_t* p = (wchar_t*)GlobalLock(h);
                        memcpy(p, g_groqResCtx->text, n * sizeof(wchar_t));
                        GlobalUnlock(h);
                        SetClipboardData(CF_UNICODETEXT, h);
                    }
                    CloseClipboard();
                }
            } else if (LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hWnd);
            }
            return 0;
        case WM_CLOSE:    DestroyWindow(hWnd); return 0;
        case WM_DESTROY:
            free(g_groqResCtx->text);
            free(g_groqResCtx);
            g_groqResCtx = NULL;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* =========================================================================
 *                       VOICE RECORDING (waveIn + Whisper)
 *
 * Records 16kHz mono 16-bit PCM via the waveIn API, packs the captured
 * samples into a WAV blob, then uploads as multipart/form-data to the
 * Groq Whisper API. The transcription is injected into the active tab.
 * ========================================================================= */

#define VOICE_SAMPLE_RATE   16000
#define VOICE_BITS_PER_SAMPLE 16
#define VOICE_CHANNELS      1
#define VOICE_BUFFER_MS     200
#define VOICE_BUFFER_BYTES  ((VOICE_SAMPLE_RATE * VOICE_BUFFER_MS / 1000) * 2)
#define VOICE_NUM_BUFFERS   4
#define VOICE_MAX_SECONDS   60

typedef struct {
    HWAVEIN     hWaveIn;
    WAVEHDR     hdrs[VOICE_NUM_BUFFERS];
    char        bufs[VOICE_NUM_BUFFERS][VOICE_BUFFER_BYTES];
    BOOL        recording;
    BOOL        uploading;
    DWORD       startTick;
    char*       pcm;      /* heap-grown PCM stream */
    size_t      pcmCap;
    size_t      pcmLen;
    CRITICAL_SECTION lock;
} Voice;

static Voice g_voice;

#define WM_DANTE_VOICE_DONE  (WM_APP + 4)
#define WM_DANTE_VOICE_ERROR (WM_APP + 5)

static void voice_init(void) {
    InitializeCriticalSection(&g_voice.lock);
}

static BOOL  voice_is_recording(void) { return g_voice.recording; }
static BOOL  voice_is_uploading(void) { return g_voice.uploading; }
static DWORD voice_elapsed_ms(void)   { return GetTickCount() - g_voice.startTick; }

static void voice_append(const char* data, size_t n) {
    EnterCriticalSection(&g_voice.lock);
    if (g_voice.pcmLen + n > g_voice.pcmCap) {
        g_voice.pcmCap = (g_voice.pcmCap + n) * 2;
        g_voice.pcm = (char*)realloc(g_voice.pcm, g_voice.pcmCap);
    }
    memcpy(g_voice.pcm + g_voice.pcmLen, data, n);
    g_voice.pcmLen += n;
    LeaveCriticalSection(&g_voice.lock);
}

static void CALLBACK voice_in_proc(HWAVEIN hwi, UINT msg, DWORD_PTR inst, DWORD_PTR p1, DWORD_PTR p2) {
    (void)inst; (void)p2;
    if (msg == WIM_DATA) {
        WAVEHDR* h = (WAVEHDR*)p1;
        if (g_voice.recording && h->dwBytesRecorded > 0) {
            voice_append(h->lpData, h->dwBytesRecorded);
        }
        if (g_voice.recording) {
            waveInAddBuffer(hwi, h, sizeof(*h));
        }
    }
}

static BOOL voice_start(void) {
    if (g_voice.recording || g_voice.uploading) return FALSE;

    WAVEFORMATEX wf = {0};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = VOICE_CHANNELS;
    wf.nSamplesPerSec  = VOICE_SAMPLE_RATE;
    wf.wBitsPerSample  = VOICE_BITS_PER_SAMPLE;
    wf.nBlockAlign     = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    MMRESULT mr = waveInOpen(&g_voice.hWaveIn, WAVE_MAPPER, &wf,
                              (DWORD_PTR)voice_in_proc, 0, CALLBACK_FUNCTION);
    if (mr != MMSYSERR_NOERROR) return FALSE;

    g_voice.pcmLen = 0;
    if (!g_voice.pcm) {
        g_voice.pcmCap = VOICE_SAMPLE_RATE * 2 * 10;   /* ~10 s initial */
        g_voice.pcm = (char*)malloc(g_voice.pcmCap);
    }

    for (int i = 0; i < VOICE_NUM_BUFFERS; ++i) {
        ZeroMemory(&g_voice.hdrs[i], sizeof(WAVEHDR));
        g_voice.hdrs[i].lpData = g_voice.bufs[i];
        g_voice.hdrs[i].dwBufferLength = VOICE_BUFFER_BYTES;
        waveInPrepareHeader(g_voice.hWaveIn, &g_voice.hdrs[i], sizeof(WAVEHDR));
        waveInAddBuffer(g_voice.hWaveIn, &g_voice.hdrs[i], sizeof(WAVEHDR));
    }

    g_voice.recording = TRUE;
    g_voice.startTick = GetTickCount();
    waveInStart(g_voice.hWaveIn);
    InvalidateRect(g_app.hWnd, NULL, FALSE);
    return TRUE;
}

static char* build_wav_blob(size_t* outLen) {
    EnterCriticalSection(&g_voice.lock);
    size_t pcmLen = g_voice.pcmLen;
    LeaveCriticalSection(&g_voice.lock);

    /* Minimal RIFF + fmt + data */
    size_t blob = 44 + pcmLen;
    char* out = (char*)malloc(blob);
    memcpy(out, "RIFF", 4);
    uint32_t riff_size = (uint32_t)(blob - 8);
    memcpy(out + 4, &riff_size, 4);
    memcpy(out + 8, "WAVE", 4);
    memcpy(out + 12, "fmt ", 4);
    uint32_t fmt_size = 16;
    memcpy(out + 16, &fmt_size, 4);
    uint16_t audio_fmt = 1, channels = VOICE_CHANNELS;
    uint32_t sample_rate = VOICE_SAMPLE_RATE;
    uint16_t bps = VOICE_BITS_PER_SAMPLE;
    uint16_t block_align = channels * bps / 8;
    uint32_t byte_rate = sample_rate * block_align;
    memcpy(out + 20, &audio_fmt, 2);
    memcpy(out + 22, &channels, 2);
    memcpy(out + 24, &sample_rate, 4);
    memcpy(out + 28, &byte_rate, 4);
    memcpy(out + 32, &block_align, 2);
    memcpy(out + 34, &bps, 2);
    memcpy(out + 36, "data", 4);
    uint32_t data_size = (uint32_t)pcmLen;
    memcpy(out + 40, &data_size, 4);
    memcpy(out + 44, g_voice.pcm, pcmLen);

    *outLen = blob;
    return out;
}

typedef struct {
    char* wav;
    size_t wavLen;
    char* apiKey;
    char* lang;
} WhisperReq;

static char* groq_extract_text_field(const char* json) {
    /* Whisper response: {"text": "..."} */
    const char* k = strstr(json, "\"text\"");
    if (!k) return NULL;
    k = strchr(k, ':');
    if (!k) return NULL;
    ++k;
    while (*k == ' ' || *k == '\t') ++k;
    if (*k != '"') return NULL;
    ++k;
    size_t cap = 512; char* out = (char*)malloc(cap); size_t o = 0;
    while (*k && *k != '"') {
        if (o + 6 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
        if (*k == '\\' && k[1]) {
            char e = k[1];
            if (e == 'n') out[o++] = '\n';
            else if (e == 't') out[o++] = '\t';
            else if (e == '"') out[o++] = '"';
            else if (e == '\\') out[o++] = '\\';
            else if (e == 'u' && k[2] && k[3] && k[4] && k[5]) {
                char hex[5] = {k[2], k[3], k[4], k[5], 0};
                unsigned int code = (unsigned int)strtoul(hex, NULL, 16);
                if (code < 0x80) out[o++] = (char)code;
                else if (code < 0x800) {
                    out[o++] = (char)(0xC0 | (code >> 6));
                    out[o++] = (char)(0x80 | (code & 0x3F));
                } else {
                    out[o++] = (char)(0xE0 | (code >> 12));
                    out[o++] = (char)(0x80 | ((code >> 6) & 0x3F));
                    out[o++] = (char)(0x80 | (code & 0x3F));
                }
                k += 4;
            } else out[o++] = e;
            k += 2;
        } else {
            out[o++] = *k++;
        }
    }
    out[o] = 0;
    return out;
}

static char* https_post_multipart(const wchar_t* host, INTERNET_PORT port, const wchar_t* path,
                                    const wchar_t* bearer,
                                    const char* boundary,
                                    const char* body, size_t bodyLen,
                                    int* outStatus, int* outLen) {
    *outStatus = 0; *outLen = 0;
    HINTERNET hSession = WinHttpOpen(L"DanteCLI/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return NULL;
    HINTERNET hConn = WinHttpConnect(hSession, host, port, 0);
    if (!hConn) { WinHttpCloseHandle(hSession); return NULL; }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path, NULL,
                                         WINHTTP_NO_REFERER,
                                         WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession); return NULL; }

    wchar_t headers[2048];
    _snwprintf_s(headers, 2048, _TRUNCATE,
                 L"Content-Type: multipart/form-data; boundary=%S\r\n"
                 L"Authorization: Bearer %s\r\n",
                 boundary, bearer);

    if (!WinHttpSendRequest(hReq, headers, (DWORD)-1L,
                             (LPVOID)body, (DWORD)bodyLen, (DWORD)bodyLen, 0)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return NULL;
    }
    if (!WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
        return NULL;
    }
    DWORD status = 0; DWORD sz = sizeof(status);
    WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
    *outStatus = (int)status;

    char* out = NULL; size_t cap = 0, len = 0;
    DWORD avail = 0;
    do {
        if (!WinHttpQueryDataAvailable(hReq, &avail)) break;
        if (avail == 0) break;
        if (len + avail + 1 > cap) { cap = (len + avail + 1) * 2; out = (char*)realloc(out, cap); }
        DWORD got = 0;
        if (!WinHttpReadData(hReq, out + len, avail, &got)) break;
        len += got;
        out[len] = 0;
    } while (avail > 0);
    *outLen = (int)len;

    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSession);
    return out;
}

static DWORD WINAPI whisper_thread(LPVOID arg) {
    WhisperReq* req = (WhisperReq*)arg;

    const char* boundary = "----danteCLIBoundary7Z";
    /* Build multipart body */
    const char* fileHeader_fmt =
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
        "whisper-large-v3-turbo\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"response_format\"\r\n\r\n"
        "json\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"language\"\r\n\r\n"
        "%s\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"voice.wav\"\r\n"
        "Content-Type: audio/wav\r\n\r\n";

    char header[1024];
    snprintf(header, 1024, fileHeader_fmt, boundary, boundary, boundary, req->lang, boundary);

    char footer[64];
    snprintf(footer, 64, "\r\n--%s--\r\n", boundary);

    size_t bodyLen = strlen(header) + req->wavLen + strlen(footer);
    char* body = (char*)malloc(bodyLen);
    size_t pos = 0;
    memcpy(body + pos, header, strlen(header)); pos += strlen(header);
    memcpy(body + pos, req->wav, req->wavLen); pos += req->wavLen;
    memcpy(body + pos, footer, strlen(footer));

    wchar_t bearer[256];
    MultiByteToWideChar(CP_UTF8, 0, req->apiKey, -1, bearer, 256);

    int status = 0, respLen = 0;
    char* resp = https_post_multipart(L"api.groq.com", INTERNET_DEFAULT_HTTPS_PORT,
                                       L"/openai/v1/audio/transcriptions",
                                       bearer, boundary, body, bodyLen,
                                       &status, &respLen);

    if (status == 200 && resp) {
        char* text = groq_extract_text_field(resp);
        if (text) {
            wchar_t* w = utf8_to_w_dup(text, -1);
            PostMessageW(g_app.hWnd, WM_DANTE_VOICE_DONE, 0, (LPARAM)w);
            free(text);
        } else {
            wchar_t* w = utf8_to_w_dup("Não foi possível ler a transcrição.", -1);
            PostMessageW(g_app.hWnd, WM_DANTE_VOICE_ERROR, 0, (LPARAM)w);
        }
    } else {
        char msg[256];
        snprintf(msg, 256, "Erro HTTP %d ao falar com a Groq (Whisper).\n\n%s",
                 status, resp ? resp : "(sem corpo)");
        wchar_t* w = utf8_to_w_dup(msg, -1);
        PostMessageW(g_app.hWnd, WM_DANTE_VOICE_ERROR, 0, (LPARAM)w);
    }

    free(resp); free(body);
    free(req->wav); free(req->apiKey); free(req->lang); free(req);
    return 0;
}

static void voice_stop_and_upload(void) {
    if (!g_voice.recording) return;
    g_voice.recording = FALSE;
    waveInStop(g_voice.hWaveIn);
    waveInReset(g_voice.hWaveIn);
    for (int i = 0; i < VOICE_NUM_BUFFERS; ++i) {
        waveInUnprepareHeader(g_voice.hWaveIn, &g_voice.hdrs[i], sizeof(WAVEHDR));
    }
    waveInClose(g_voice.hWaveIn);
    g_voice.hWaveIn = NULL;

    /* Validate: at least ~0.4s of audio (16000 * 2 * 0.4 = 12800 bytes) */
    if (g_voice.pcmLen < 12800) {
        MessageBoxW(g_app.hWnd,
            L"Áudio muito curto (< 0.4s). Tente novamente, segurando o botão por mais tempo.",
            APP_NAME_W, MB_ICONWARNING);
        InvalidateRect(g_app.hWnd, NULL, FALSE);
        return;
    }

    if (g_app.groqApiKey[0] == 0) {
        MessageBoxW(g_app.hWnd,
            L"Configure a Groq API key em ⚙ Configurações primeiro.",
            APP_NAME_W, MB_ICONWARNING);
        InvalidateRect(g_app.hWnd, NULL, FALSE);
        return;
    }

    size_t wavLen = 0;
    char* wav = build_wav_blob(&wavLen);

    WhisperReq* req = (WhisperReq*)calloc(1, sizeof(*req));
    req->wav = wav;
    req->wavLen = wavLen;
    req->apiKey = w_to_utf8_dup(g_app.groqApiKey);
    req->lang = w_to_utf8_dup(g_app.voiceLang[0] ? g_app.voiceLang : L"pt");
    g_voice.uploading = TRUE;
    HANDLE h = CreateThread(NULL, 0, whisper_thread, req, 0, NULL);
    if (h) CloseHandle(h);
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

static void voice_toggle(void) {
    if (g_voice.recording) {
        voice_stop_and_upload();
    } else if (g_voice.uploading) {
        MessageBoxW(g_app.hWnd, L"Aguardando resposta da Groq...",
                    APP_NAME_W, MB_ICONINFORMATION);
    } else {
        if (g_app.groqApiKey[0] == 0) {
            MessageBoxW(g_app.hWnd,
                L"Configure a Groq API key em ⚙ Configurações primeiro.",
                APP_NAME_W, MB_ICONWARNING);
            return;
        }
        if (!voice_start()) {
            MessageBoxW(g_app.hWnd,
                L"Não foi possível abrir o dispositivo de microfone.",
                APP_NAME_W, MB_ICONERROR);
        }
    }
}

static void show_groq_result(const wchar_t* title, wchar_t* text /* owned */) {
    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = GroqResWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"DanteGroqResult";
        RegisterClassExW(&wc);
        registered = TRUE;
    }
    g_groqResCtx = (GroqResultCtx*)calloc(1, sizeof(*g_groqResCtx));
    g_groqResCtx->text = text;

    RECT pr; GetWindowRect(g_app.hWnd, &pr);
    int W = 640, H = 480;
    int x = pr.left + ((pr.right - pr.left) - W) / 2;
    int y = pr.top  + ((pr.bottom - pr.top) - H) / 2;

    HWND dlg = CreateWindowExW(0, L"DanteGroqResult", title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, W, H, g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(dlg, SW_SHOW);
}

/* =========================================================================
 *                              TABS
 * ========================================================================= */

static void open_new_tab(const wchar_t* shellId) {
    if (g_app.tabCount >= MAX_TABS) return;

    Tab* t = (Tab*)xcalloc(1, sizeof(Tab));
    t->id = next_tab_id++;
    int idx = g_app.tabCount;
    swprintf(t->title, 128, L"Terminal %d", idx + 1);
    str_copy_w(t->emoji, 8, (idx % 2 == 0) ? L"\U0001F4BB" : L"⚡");
    t->colorIdx = idx % 12;
    terminal_grid_init(&t->grid, 120, 30);
    parser_init(&t->parser);

    t->session = session_create(shellId ? shellId : L"powershell", 120, 30);
    if (t->session) t->session->ownerTabId = t->id;

    g_app.tabs[g_app.tabCount++] = t;
    g_app.activeTab = g_app.tabCount - 1;
    schedule_persist();
    InvalidateRect(g_app.hWnd, NULL, FALSE);
    update_status();
}

static void close_tab(int idx) {
    if (idx < 0 || idx >= g_app.tabCount) return;
    Tab* t = g_app.tabs[idx];
    session_destroy(t->session);
    terminal_grid_free(&t->grid);
    free(t);
    for (int i = idx; i < g_app.tabCount - 1; ++i) g_app.tabs[i] = g_app.tabs[i + 1];
    g_app.tabCount--;

    /* Patch splitSlots — every slot pointing at an index >= idx must shift
     * down. Slots pointing AT the removed tab become empty (-1). Without
     * this, closing a tab while split was active rendered the wrong tab in
     * the cell, or worse, accessed g_app.tabs[stale] which could be NULL. */
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) {
        if (g_app.splitSlots[i] == idx) g_app.splitSlots[i] = -1;
        else if (g_app.splitSlots[i] > idx) g_app.splitSlots[i]--;
    }

    if (g_app.tabCount == 0) {
        open_new_tab(L"powershell");
        return;
    }
    if (g_app.activeTab >= g_app.tabCount) g_app.activeTab = g_app.tabCount - 1;
    if (g_app.activeTab > idx) g_app.activeTab--;
    else if (g_app.activeTab == idx && g_app.activeTab >= g_app.tabCount)
        g_app.activeTab = g_app.tabCount - 1;
    schedule_persist();
    InvalidateRect(g_app.hWnd, NULL, FALSE);
    update_status();
}

static void inject_into_active(const wchar_t* text) {
    if (g_app.activeTab < 0 || g_app.activeTab >= g_app.tabCount) return;
    Tab* t = g_app.tabs[g_app.activeTab];
    if (!t || !t->session) return;
    char utf[2048];
    int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, utf, sizeof(utf), NULL, NULL);
    if (n > 0) session_write(t->session, utf, n - 1);
}

static void invalidate_terminal(void) {
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

/* =========================================================================
 *                              LAYOUT
 * ========================================================================= */

static void layout_compute(RECT* outSide, RECT* outTabBar, RECT* outTerm, RECT* outToolbar, RECT* outStatus) {
    RECT cr; GetClientRect(g_app.hWnd, &cr);
    int W = cr.right - cr.left;
    int H = cr.bottom - cr.top;

    outSide->left = 0; outSide->top = 0; outSide->right = SIDEBAR_W; outSide->bottom = H - STATUSBAR_H;

    outTabBar->left = SIDEBAR_W; outTabBar->top = 0;
    outTabBar->right = W; outTabBar->bottom = TABBAR_H;

    outToolbar->left = SIDEBAR_W; outToolbar->top = H - STATUSBAR_H - TOOLBAR_AI_H;
    outToolbar->right = W; outToolbar->bottom = H - STATUSBAR_H;

    outTerm->left = SIDEBAR_W; outTerm->top = TABBAR_H;
    outTerm->right = W; outTerm->bottom = outToolbar->top;

    outStatus->left = 0; outStatus->top = H - STATUSBAR_H;
    outStatus->right = W; outStatus->bottom = H;
}

/* =========================================================================
 *                              DRAW
 * ========================================================================= */

static void fill_rect_color(HDC dc, const RECT* r, COLORREF c) {
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void draw_text_w(HDC dc, int x, int y, const wchar_t* s, COLORREF color, HFONT font) {
    HFONT old = (HFONT)SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    TextOutW(dc, x, y, s, (int)wcslen(s));
    SelectObject(dc, old);
}

static void draw_rounded_rect(HDC dc, RECT r, COLORREF fill, COLORREF border, int radius) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN  pen = border ? CreatePen(PS_SOLID, 1, border) : (HPEN)GetStockObject(NULL_PEN);
    HBRUSH oldBr = (HBRUSH)SelectObject(dc, br);
    HPEN   oldPen = (HPEN)SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldBr); SelectObject(dc, oldPen);
    DeleteObject(br); if (border) DeleteObject(pen);
}

/* ---------- Sidebar ---------- */

static int sidebar_mode_button_rect(int idx, const RECT* sideRc, RECT* out) {
    int btnW = (sideRc->right - sideRc->left - 24) / MODE_COUNT;
    int btnH = 32;
    out->left = sideRc->left + 12 + idx * btnW;
    out->right = out->left + btnW - 4;
    out->top = sideRc->top + 6;
    out->bottom = out->top + btnH;
    return 1;
}

static void draw_sidebar(HDC dc, const RECT* rc) {
    fill_rect_color(dc, rc, COL_BG_SIDE);

    /* Vertical divider on the right */
    RECT div = { rc->right - 1, rc->top, rc->right, rc->bottom };
    fill_rect_color(dc, &div, COL_DIV);

    /* Mode tabs at the top */
    for (int i = 0; i < MODE_COUNT; ++i) {
        RECT br;
        sidebar_mode_button_rect(i, rc, &br);
        BOOL active = (g_app.sidebarMode == (SidebarMode)i);
        BOOL hover = (g_app.sidebarHoverIdx == i);
        COLORREF bg = active ? COL_TAB_ACTIVE : (hover ? COL_BG_CHIP_H : COL_BG_CHIP);
        COLORREF fg = active ? RGB(0x16, 0x16, 0x1E) : COL_FG;
        draw_rounded_rect(dc, br, bg, 0, 8);
        SIZE sz;
        HFONT f = g_app.hFontEmoji;
        HFONT old = (HFONT)SelectObject(dc, f);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        GetTextExtentPoint32W(dc, kSidebarModeIcons[i], (int)wcslen(kSidebarModeIcons[i]), &sz);
        TextOutW(dc, br.left + ((br.right - br.left) - sz.cx) / 2,
                 br.top + ((br.bottom - br.top) - sz.cy) / 2,
                 kSidebarModeIcons[i], (int)wcslen(kSidebarModeIcons[i]));
        SelectObject(dc, old);
    }

    /* Header label */
    {
        const wchar_t* label = kSidebarModeLabels[g_app.sidebarMode];
        draw_text_w(dc, rc->left + 16, rc->top + SIDEBAR_HDR_H + 8,
                    label, COL_FG, g_app.hFontUI);
    }

    /* Search box (visual only) */
    RECT search = { rc->left + 12, rc->top + SIDEBAR_HDR_H + 38,
                    rc->right - 12, rc->top + SIDEBAR_HDR_H + 70 };
    draw_rounded_rect(dc, search, COL_BG_CHIP, COL_DIV, 6);
    draw_text_w(dc, search.left + 10, search.top + 7,
                L"\U0001F50D  Buscar...", COL_FG_DIM, g_app.hFontUI);

    /* Items list */
    int y = search.bottom + 12;
    int itemCount = 0;
    switch (g_app.sidebarMode) {
        case MODE_FAVORITES: itemCount = g_app.favoriteCount; break;
        case MODE_SNIPPETS:  itemCount = g_app.snippetCount;  break;
        case MODE_CREDS:     itemCount = g_app.credCount;     break;
        case MODE_FILES: itemCount = g_app.filesCount; break;
        default: break;
    }

    if (itemCount == 0) {
        const wchar_t* empty =
            (g_app.sidebarMode == MODE_FAVORITES) ?
                L"Nenhum favorito ainda.\n\nClique em ⊕ Adicionar para guardar um caminho frequente que abrirá uma nova aba já com cd <pasta>." :
            (g_app.sidebarMode == MODE_SNIPPETS) ?
                L"Nenhum snippet ainda.\n\nSnippets são blocos de comando reutilizáveis. Um clique injeta no terminal ativo." :
                L"Nenhuma credencial ainda.\n\nGuarde SSH/FTP/API/Custom — clicar injeta um bloco comentado para a IA usar como contexto.";
        RECT tip = { rc->left + 16, y, rc->right - 16, rc->bottom - 60 };
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontUI);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, COL_FG_DIM);
        DrawTextW(dc, empty, -1, &tip, DT_WORDBREAK | DT_LEFT);
        SelectObject(dc, old);
    } else if (g_app.sidebarMode == MODE_FILES) {
        /* Show the current directory path at the top, then list. */
        wchar_t shortPath[64];
        size_t pl = wcslen(g_app.filesDir);
        if (pl > 38) {
            wcscpy_s(shortPath, 64, L"…");
            wcsncat_s(shortPath, 64, g_app.filesDir + (pl - 37), 37);
        } else {
            wcscpy_s(shortPath, 64, g_app.filesDir);
        }
        draw_text_w(dc, rc->left + 16, y, shortPath, COL_ACCENT, g_app.hFontUI);
        y += 22;

        HFONT oldF = (HFONT)SelectObject(dc, g_app.hFontUI);
        SetBkMode(dc, TRANSPARENT);
        int rowH = 28;
        for (int i = 0; i < itemCount && y + rowH < rc->bottom - 48; ++i) {
            RECT row = { rc->left + 12, y, rc->right - 12, y + rowH - 2 };
            BOOL hover = (g_app.sidebarItemHoverIdx == i);
            draw_rounded_rect(dc, row, hover ? COL_BG_CHIP_H : COL_BG_CHIP, 0, 6);
            const wchar_t* icon = (i == 0) ? L"↰" :
                                  g_app.filesIsDir[i] ? L"\U0001F4C1" : L"\U0001F4C4";
            draw_text_w(dc, row.left + 8, row.top + 3,
                        icon, COL_FG, g_app.hFontEmoji);
            wchar_t shown[48];
            size_t nl = wcslen(g_app.filesEntries[i]);
            if (nl > 30) {
                wcsncpy_s(shown, 48, g_app.filesEntries[i], 29);
                shown[29] = L'…'; shown[30] = 0;
            } else {
                wcscpy_s(shown, 48, g_app.filesEntries[i]);
            }
            draw_text_w(dc, row.left + 36, row.top + 5,
                        shown, g_app.filesIsDir[i] ? COL_FG : COL_FG_DIM,
                        g_app.hFontUI);
            y += rowH;
        }
        SelectObject(dc, oldF);
    } else {
        /* Render each item as a card */
        HFONT oldName = (HFONT)SelectObject(dc, g_app.hFontUI);
        SetBkMode(dc, TRANSPARENT);
        int rowH = 46;
        for (int i = 0; i < itemCount && y + rowH < rc->bottom - 48; ++i) {
            RECT row = { rc->left + 12, y, rc->right - 12, y + rowH - 4 };
            BOOL hover = (g_app.sidebarItemHoverIdx == i);
            draw_rounded_rect(dc, row, hover ? COL_BG_CHIP_H : COL_BG_CHIP, 0, 8);

            const wchar_t* emoji = L"";
            const wchar_t* title = L"";
            wchar_t sub[256] = {0};

            if (g_app.sidebarMode == MODE_FAVORITES) {
                emoji = g_app.favorites[i].emoji;
                title = g_app.favorites[i].name;
                str_copy_w(sub, 256, g_app.favorites[i].path);
            } else if (g_app.sidebarMode == MODE_SNIPPETS) {
                emoji = g_app.snippets[i].emoji;
                title = g_app.snippets[i].name;
                str_copy_w(sub, 256, g_app.snippets[i].cmd);
            } else if (g_app.sidebarMode == MODE_CREDS) {
                emoji = g_app.creds[i].emoji;
                title = g_app.creds[i].name;
                _snwprintf_s(sub, 256, _TRUNCATE, L"%s · %s@%s",
                             g_app.creds[i].kind, g_app.creds[i].user, g_app.creds[i].host);
            }

            draw_text_w(dc, row.left + 10, row.top + 6, emoji, COL_FG, g_app.hFontEmoji);
            draw_text_w(dc, row.left + 36, row.top + 6, title, COL_FG, g_app.hFontUI);

            /* Truncate sub */
            wchar_t shortened[64];
            size_t sl = wcslen(sub);
            if (sl > 32) {
                wcsncpy_s(shortened, 64, sub, 31);
                shortened[31] = L'…';
                shortened[32] = 0;
            } else {
                wcscpy_s(shortened, 64, sub);
            }
            draw_text_w(dc, row.left + 36, row.top + 24, shortened, COL_FG_DIM, g_app.hFontUI);

            y += rowH;
        }
        SelectObject(dc, oldName);
    }

    /* Footer button — text depends on mode. Files mode = navigate to Home. */
    RECT add = { rc->left + 12, rc->bottom - 44, rc->right - 12, rc->bottom - 12 };
    draw_rounded_rect(dc, add, COL_BG_CHIP, COL_ACCENT, 8);
    SIZE sz;
    HFONT old2 = (HFONT)SelectObject(dc, g_app.hFontUI);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, COL_ACCENT);
    const wchar_t* lbl =
        (g_app.sidebarMode == MODE_FILES) ? L"\U0001F3E0  Ir para Home"
                                          : L"⊕  Adicionar";
    GetTextExtentPoint32W(dc, lbl, (int)wcslen(lbl), &sz);
    TextOutW(dc, add.left + ((add.right - add.left) - sz.cx) / 2,
             add.top + ((add.bottom - add.top) - sz.cy) / 2,
             lbl, (int)wcslen(lbl));
    SelectObject(dc, old2);
}

/* ---------- TabBar ---------- */

static int tab_chip_x(int idx) {
    int x = SIDEBAR_W + 10 - g_app.tabBarScroll;
    for (int i = 0; i < idx; ++i) x += 156;
    return x;
}

static void draw_tabbar(HDC dc, const RECT* rc) {
    fill_rect_color(dc, rc, COL_BG_TABBAR);

    /* Bottom divider */
    RECT div = { rc->left, rc->bottom - 1, rc->right, rc->bottom };
    fill_rect_color(dc, &div, COL_DIV);

    for (int i = 0; i < g_app.tabCount; ++i) {
        Tab* t = g_app.tabs[i];
        int x = tab_chip_x(i);
        RECT chip = { x, rc->top + 5, x + 146, rc->bottom - 5 };
        BOOL active = (i == g_app.activeTab);
        BOOL hover  = (i == g_app.tabBarHoverIdx);

        COLORREF acc = (t->colorIdx >= 0) ? kTabColors[t->colorIdx] : COL_ACCENT;
        COLORREF bg = active ? acc : (hover ? COL_BG_CHIP_H : COL_BG_CHIP);
        if (active) {
            /* Attached-to-body look: only top corners rounded */
            RECT body = chip;
            body.bottom += 2;
            HBRUSH br = CreateSolidBrush(bg);
            HPEN pen = CreatePen(PS_SOLID, 1, bg);
            HBRUSH oBr = (HBRUSH)SelectObject(dc, br);
            HPEN  oPen = (HPEN)SelectObject(dc, pen);
            RoundRect(dc, body.left, body.top, body.right, body.bottom, 10, 10);
            /* Mask off the bottom corners by drawing a small rect */
            RECT mask = { body.left, body.bottom - 4, body.right, body.bottom };
            FillRect(dc, &mask, br);
            SelectObject(dc, oBr); SelectObject(dc, oPen);
            DeleteObject(br); DeleteObject(pen);
        } else {
            draw_rounded_rect(dc, chip, bg, 0, 14);
        }

        /* Pin indicator */
        int textX = chip.left + 10;
        if (t->pinned) {
            draw_text_w(dc, textX, chip.top + 6, L"\U0001F4CC", COL_ORANGE, g_app.hFontEmoji);
            textX += 18;
        }
        /* Emoji */
        if (t->emoji[0]) {
            draw_text_w(dc, textX, chip.top + 6, t->emoji,
                        active ? RGB(0x16,0x16,0x1E) : COL_FG, g_app.hFontEmoji);
            textX += 22;
        }

        /* Title (truncated) */
        wchar_t shown[64];
        size_t tl = wcslen(t->title);
        if (tl > 12) {
            wcsncpy_s(shown, 64, t->title, 11);
            shown[11] = L'…';
            shown[12] = 0;
        } else {
            wcscpy_s(shown, 64, t->title);
        }
        draw_text_w(dc, textX, chip.top + 8, shown,
                    active ? RGB(0x16,0x16,0x1E) : COL_FG, g_app.hFontUI);

        /* Close button (× at right) */
        if (!t->pinned) {
            RECT closeR = { chip.right - 24, chip.top + 6, chip.right - 6, chip.bottom - 6 };
            draw_text_w(dc, closeR.left + 4, closeR.top + 2, L"×",
                        active ? RGB(0x16,0x16,0x1E) : COL_FG_DIM, g_app.hFontUI);
        }
    }

    /* "+" button */
    int plusX = tab_chip_x(g_app.tabCount) + 4;
    RECT plus = { plusX, rc->top + 7, plusX + 32, rc->bottom - 7 };
    draw_rounded_rect(dc, plus, COL_BG_CHIP, 0, 8);
    HFONT old = (HFONT)SelectObject(dc, g_app.hFontBig);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, COL_ACCENT);
    DrawTextW(dc, L"+", -1, &plus, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old);

    /* Stats chip on the right edge */
    wchar_t memBuf[16], cpuBuf[16];
    fmt_bytes(g_lastTotalMem, memBuf, 16);
    _snwprintf_s(cpuBuf, 16, _TRUNCATE, L"%.0f%%", g_lastTotalCpu);
    wchar_t combined[64];
    _snwprintf_s(combined, 64, _TRUNCATE, L"\U0001F4CA  %s  ·  %s",
                 memBuf, cpuBuf);
    SIZE chipSz;
    HFONT oldF = (HFONT)SelectObject(dc, g_app.hFontUI);
    GetTextExtentPoint32W(dc, combined, (int)wcslen(combined), &chipSz);
    SelectObject(dc, oldF);
    int chipW = chipSz.cx + 24;
    RECT chip = { rc->right - chipW - 10, rc->top + 7,
                  rc->right - 10,          rc->bottom - 7 };
    COLORREF cpuColor = g_lastTotalCpu > 80 ? COL_RED
                      : g_lastTotalCpu > 40 ? COL_ORANGE
                      : COL_GREEN;
    draw_rounded_rect(dc, chip, COL_BG_CHIP, cpuColor, 12);
    draw_text_w(dc, chip.left + 12, chip.top + 8, combined,
                COL_FG, g_app.hFontUI);
}

/* ---------- Terminal ---------- */

static COLORREF resolve_fg(const Cell* c) {
    if (c->attr & ATTR_FG_RGB) return c->fgRgb;
    if (c->attr & ATTR_FG_DEFAULT) return current_scheme()->fg;
    return kAnsiPalette[c->fgIdx & 0x0F];
}
static COLORREF resolve_bg(const Cell* c) {
    if (c->attr & ATTR_BG_RGB) return c->bgRgb;
    if (c->attr & ATTR_BG_DEFAULT) return current_scheme()->bg;
    return kAnsiPalette[c->bgIdx & 0x0F];
}

static void measure_cell(HDC dc) {
    /* Recreate the mono font if user changed the size. */
    if (g_app.fontPxOverride > 0) {
        static int lastSize = 0;
        if (g_app.fontPxOverride != lastSize) {
            if (g_app.hFontMono)     DeleteObject(g_app.hFontMono);
            if (g_app.hFontMonoBold) DeleteObject(g_app.hFontMonoBold);
            g_app.hFontMono = CreateFontW(-g_app.fontPxOverride, 0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
            g_app.hFontMonoBold = CreateFontW(-g_app.fontPxOverride, 0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
            lastSize = g_app.fontPxOverride;
        }
    }
    HFONT old = (HFONT)SelectObject(dc, g_app.hFontMono);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    SIZE s;
    GetTextExtentPoint32W(dc, L"M", 1, &s);
    g_app.cellW = max_int(s.cx, tm.tmAveCharWidth);
    g_app.cellH = tm.tmHeight;
    SelectObject(dc, old);
}

static int active_tab_index(void) {
    if (g_app.splitLayout == PRESET_SINGLE) return g_app.activeTab;
    const SplitPreset* preset = &kPresets[g_app.splitLayout];
    int slot = clamp_int(g_app.splitActiveCell, 0, preset->cellCount - 1);
    int idx = g_app.splitSlots[slot];
    if (idx < 0 || idx >= g_app.tabCount) return g_app.activeTab;
    return idx;
}

/* Pick a foreground colour that contrasts with the given background. */
static COLORREF contrast_text_color(COLORREF c) {
    int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    /* Relative luminance (sRGB approximation). */
    double L = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0;
    return L > 0.55 ? RGB(0x1A, 0x1B, 0x26) : RGB(0xFF, 0xFF, 0xFF);
}

/* Mix two colours additively, t ∈ [0..1]. */
static COLORREF mix_color(COLORREF a, COLORREF b, double t) {
    int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    int r = (int)(ar + (br - ar) * t);
    int g = (int)(ag + (bg - ag) * t);
    int x = (int)(ab + (bb - ab) * t);
    return RGB(r, g, x);
}

/* Header bar above each terminal cell. Same colour as the tab chip — this
 * is the visual link the user wanted: tab colour ↔ terminal head colour.
 * Contents: emoji + title (bold) + cwd (dim) + memory chip on the right. */
static void draw_terminal_header(HDC dc, RECT hdr, Tab* t, BOOL hasFocus, int cellIdx) {
    COLORREF acc   = (t->colorIdx >= 0) ? kTabColors[t->colorIdx] : COL_ACCENT;
    COLORREF fg    = contrast_text_color(acc);
    COLORREF fgDim = mix_color(fg, acc, 0.40);

    fill_rect_color(dc, &hdr, acc);

    /* Hamburger icon — 3 lines on the left. */
    HPEN pen = CreatePen(PS_SOLID, 2, fg);
    HGDIOBJ op = SelectObject(dc, pen);
    int hx = hdr.left + 14;
    int hy = hdr.top  + 12;
    for (int i = 0; i < 3; ++i) {
        MoveToEx(dc, hx, hy + i * 6, NULL);
        LineTo(dc, hx + 16, hy + i * 6);
    }
    SelectObject(dc, op);
    DeleteObject(pen);

    int x = hdr.left + 44;

    /* Emoji */
    if (t->emoji[0]) {
        draw_text_w(dc, x, hdr.top + 8, t->emoji, fg, g_app.hFontEmoji);
        x += 26;
    }

    /* Title (bold) */
    HFONT oldFont = (HFONT)SelectObject(dc, g_app.hFontUIBold);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    TextOutW(dc, x, hdr.top + 10, t->title, (int)wcslen(t->title));
    SIZE titleSz;
    GetTextExtentPoint32W(dc, t->title, (int)wcslen(t->title), &titleSz);
    SelectObject(dc, oldFont);

    /* cwd (dim, lighter weight) — clipped to remaining space */
    if (t->session && t->session->cwd[0]) {
        int cwdX = x + titleSz.cx + 12;
        int chipReserve = 110;
        RECT cwdRc = { cwdX, hdr.top + 11, hdr.right - chipReserve, hdr.bottom - 6 };
        if (cwdRc.right > cwdRc.left + 30) {
            HFONT oldF = (HFONT)SelectObject(dc, g_app.hFontUI);
            SetTextColor(dc, fgDim);
            DrawTextW(dc, t->session->cwd, -1, &cwdRc,
                      DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS | DT_VCENTER);
            SelectObject(dc, oldF);
        }
    }

    /* Right-edge buttons cluster: ✕ unslot (split only), ⇲ resize (split only),
     * ⤢ zoom (split only). Each stores its hit rect for the global handler. */
    int rightCursor = hdr.right - 10;

    /* Unslot button — removes the tab from this slot without closing it.
     * The slot becomes empty (placeholder). The Tab stays alive elsewhere. */
    if (g_app.splitLayout != PRESET_SINGLE && cellIdx >= 0 && cellIdx < MAX_SPLIT_CELLS) {
        RECT btn = { rightCursor - 28, hdr.top + 6, rightCursor, hdr.bottom - 6 };
        COLORREF btnBg = mix_color(acc, RGB(0,0,0), 0.35);
        draw_rounded_rect(dc, btn, btnBg, 0, 8);
        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontUIBold);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        GetTextExtentPoint32W(dc, L"✕", 1, &sz);
        TextOutW(dc, btn.left + ((btn.right - btn.left) - sz.cx) / 2,
                 btn.top  + ((btn.bottom - btn.top) - sz.cy) / 2,
                 L"✕", 1);
        SelectObject(dc, old);
        g_app.unslotBtnRect[cellIdx] = btn;
        rightCursor = btn.left - 6;
    }

    /* Zoom button */
    if (g_app.splitLayout != PRESET_SINGLE && cellIdx >= 0 && cellIdx < MAX_SPLIT_CELLS) {
        RECT btn = { rightCursor - 28, hdr.top + 6, rightCursor, hdr.bottom - 6 };
        BOOL isZoomed = (g_app.zoomedCell == cellIdx);
        COLORREF btnBg = mix_color(acc, RGB(0,0,0), isZoomed ? 0.55 : 0.35);
        draw_rounded_rect(dc, btn, btnBg, 0, 8);
        /* Use simple geometric glyphs that always render on Segoe UI. */
        const wchar_t* icon = isZoomed ? L"▭" : L"▢";
        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontEmoji);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        GetTextExtentPoint32W(dc, icon, (int)wcslen(icon), &sz);
        TextOutW(dc, btn.left + ((btn.right - btn.left) - sz.cx) / 2,
                 btn.top  + ((btn.bottom - btn.top) - sz.cy) / 2,
                 icon, (int)wcslen(icon));
        SelectObject(dc, old);
        g_app.zoomBtnRect[cellIdx] = btn;
        rightCursor = btn.left - 6;
    }

    /* Resize button (↔) */
    if (g_app.splitLayout != PRESET_SINGLE && cellIdx >= 0 && cellIdx < MAX_SPLIT_CELLS) {
        RECT btn = { rightCursor - 28, hdr.top + 6, rightCursor, hdr.bottom - 6 };
        COLORREF btnBg = mix_color(acc, RGB(0,0,0), 0.35);
        draw_rounded_rect(dc, btn, btnBg, 0, 8);
        /* "↔" reads as "resize" universally, renders cleanly on Segoe UI.  */
        const wchar_t* icon = L"↔";
        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontEmoji);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        GetTextExtentPoint32W(dc, icon, (int)wcslen(icon), &sz);
        TextOutW(dc, btn.left + ((btn.right - btn.left) - sz.cx) / 2,
                 btn.top  + ((btn.bottom - btn.top) - sz.cy) / 2,
                 icon, (int)wcslen(icon));
        SelectObject(dc, old);
        g_app.sizeBtnRect[cellIdx] = btn;
        rightCursor = btn.left - 6;
    }

    int zoomBtnRight = rightCursor;

    /* Memory chip on the right — reads from the cache populated by the
     * 1 s timer instead of doing OpenProcess every paint.              */
    if (t->cachedMemBytes > 0) {
        wchar_t chip[32];
        double mb = (double)t->cachedMemBytes / (1024.0 * 1024.0);
        _snwprintf_s(chip, 32, _TRUNCATE,
                     mb < 1000 ? L"%.0f M" : L"%.1f G",
                     mb < 1000 ? mb : mb / 1024.0);
        SIZE sz;
        HFONT oldF = (HFONT)SelectObject(dc, g_app.hFontUI);
        GetTextExtentPoint32W(dc, chip, (int)wcslen(chip), &sz);
        RECT chipRc = {
            zoomBtnRight - sz.cx - 12, hdr.top + 8,
            zoomBtnRight,              hdr.bottom - 8
        };
        COLORREF chipBg = mix_color(acc, RGB(0,0,0), 0.35);
        draw_rounded_rect(dc, chipRc, chipBg, 0, 10);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, fg);
        TextOutW(dc, chipRc.left + 6, chipRc.top + 3,
                 chip, (int)wcslen(chip));
        SelectObject(dc, oldF);
    }

    /* Subtle separator at the bottom of the header. */
    HBRUSH sep = CreateSolidBrush(mix_color(acc, RGB(0,0,0), 0.20));
    RECT sepR = { hdr.left, hdr.bottom - 1, hdr.right, hdr.bottom };
    FillRect(dc, &sepR, sep);
    DeleteObject(sep);

    /* Mark "active cell" with a slight extra inset border. */
    if (hasFocus && g_app.splitLayout != PRESET_SINGLE) {
        HPEN bp = CreatePen(PS_SOLID, 2, fg);
        HGDIOBJ obp = SelectObject(dc, bp);
        MoveToEx(dc, hdr.left, hdr.bottom, NULL);
        LineTo(dc, hdr.right, hdr.bottom);
        SelectObject(dc, obp);
        DeleteObject(bp);
    }
}

/* Pick the active colour scheme for a given tab (override if set, else global). */
static const ColorScheme* tab_scheme(const Tab* t) {
    if (t && t->customScheme[0]) {
        const ColorScheme* s = scheme_by_id(t->customScheme);
        if (s) return s;
    }
    return current_scheme();
}

/* Pick the font height for a given tab (override or global fontPxOverride or default). */
static int tab_font_px(const Tab* t) {
    if (t && t->customFontSize > 0) return t->customFontSize;
    if (g_app.fontPxOverride > 0)   return g_app.fontPxOverride;
    return 16;                                       /* fallback */
}

/* Render one terminal cell. cellIdx ∈ [0..MAX_SPLIT_CELLS-1] or -1 (single
 * tab mode). hasFocus draws the focus border in split mode.            */
static void draw_terminal_cell(HDC dc, const RECT* rc, int tabIdx, BOOL hasFocus, int cellIdx) {
    const Tab* tForScheme = (tabIdx >= 0 && tabIdx < g_app.tabCount) ? g_app.tabs[tabIdx] : NULL;
    const ColorScheme* sc = tab_scheme(tForScheme);
    fill_rect_color(dc, rc, sc->bg);

    if (tabIdx < 0 || tabIdx >= g_app.tabCount) {
        /* Empty slot */
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontUI);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, COL_FG_DIM);
        DrawTextW(dc, L"⊕\nslot vazio", -1, (RECT*)rc,
                  DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        SelectObject(dc, old);
        return;
    }
    Tab* t = g_app.tabs[tabIdx];
    if (!t) return;

    /* Header bar — same colour as the tab chip. Replaces the old 3-px strip. */
    RECT header = *rc;
    header.bottom = header.top + TERM_HEADER_H;
    draw_terminal_header(dc, header, t, hasFocus, cellIdx);

    /* Body rect for the terminal content (below the header). */
    RECT body = *rc;
    body.top += TERM_HEADER_H;
    fill_rect_color(dc, &body, sc->bg);
    rc = &body;
    COLORREF acc = (t->colorIdx >= 0) ? kTabColors[t->colorIdx] : COL_ACCENT;

    /* Focus border (subtle) */
    if (hasFocus && g_app.splitLayout != PRESET_SINGLE) {
        HPEN pen = CreatePen(PS_SOLID, 1, acc);
        HGDIOBJ op = SelectObject(dc, pen);
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, rc->left, rc->top, rc->right - 1, rc->bottom - 1);
        SelectObject(dc, op); SelectObject(dc, ob);
        DeleteObject(pen);
    }

    /* Inner padding for the terminal text — gives the content room from the
     * window edge instead of touching it. 20 px horizontal, 14 px vertical
     * (top/bottom). Reflected in both grid sizing and pixel drawing. */
    const int TERM_PAD_X = 20;
    const int TERM_PAD_Y = 14;

    /* Per-tab font override: when t->customFontSize > 0 we build a temporary
     * font and use it instead of g_app.hFontMono. cellW/cellH are also
     * remeasured locally so the grid sizing matches.                     */
    HFONT localMono = NULL, localMonoBold = NULL;
    int localW = g_app.cellW, localH = g_app.cellH;
    if (t->customFontSize > 0) {
        localMono = CreateFontW(-t->customFontSize, 0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
        localMonoBold = CreateFontW(-t->customFontSize, 0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
        HFONT oldM = (HFONT)SelectObject(dc, localMono);
        TEXTMETRICW tm;
        GetTextMetricsW(dc, &tm);
        SIZE s; GetTextExtentPoint32W(dc, L"M", 1, &s);
        localW = max_int(s.cx, tm.tmAveCharWidth);
        localH = tm.tmHeight;
        SelectObject(dc, oldM);
    }

    int areaW = (rc->right - rc->left) - 2 * TERM_PAD_X;
    int areaH = (rc->bottom - rc->top) - 2 * TERM_PAD_Y;
    if (g_app.cellW == 0 || g_app.cellH == 0) measure_cell(dc);
    if (t->customFontSize == 0) { localW = g_app.cellW; localH = g_app.cellH; }
    int cols = max_int(20, areaW / max_int(1, localW));
    int rows = max_int(5,  areaH / max_int(1, localH));
    cols = min_int(cols, MAX_COLS);
    rows = min_int(rows, MAX_ROWS);
    if (cols != t->grid.cols || rows != t->grid.rows) {
        terminal_grid_resize(&t->grid, cols, rows);
        if (t->session) session_resize(t->session, cols, rows);
    }

    HFONT baseMono = localMono ? localMono : g_app.hFontMono;
    HFONT baseMonoBold = localMonoBold ? localMonoBold : g_app.hFontMonoBold;
    HFONT old = (HFONT)SelectObject(dc, baseMono);
    SetBkMode(dc, OPAQUE);

    int baseX = rc->left + TERM_PAD_X;
    int baseY = rc->top  + TERM_PAD_Y;

    for (int r = 0; r < rows; ++r) {
        int y = baseY + r * localH;
        for (int c = 0; c < cols; ++c) {
            const Cell* cell = &t->grid.cells[r * cols + c];
            COLORREF fg = resolve_fg(cell);
            COLORREF bg = resolve_bg(cell);
            if (cell->attr & ATTR_REVERSE) { COLORREF tmp = fg; fg = bg; bg = tmp; }
            int x = baseX + c * localW;

            /* bg fill if needed */
            if (bg != sc->bg) {
                RECT bgR = { x, y, x + localW, y + localH };
                fill_rect_color(dc, &bgR, bg);
            }
            if (cell->ch != L' ' && cell->ch != 0) {
                HFONT useFont = (cell->attr & ATTR_BOLD) ? baseMonoBold : baseMono;
                HFONT prev = (HFONT)SelectObject(dc, useFont);
                SetTextColor(dc, fg);
                SetBkColor(dc, bg);
                TextOutW(dc, x, y, &cell->ch, 1);
                SelectObject(dc, prev);
            }
        }
    }

    /* Cursor */
    int cx = baseX + t->grid.cursorCol * localW;
    int cy = baseY + t->grid.cursorRow * localH;
    RECT cur = { cx, cy, cx + localW, cy + localH };
    HBRUSH cb = CreateSolidBrush(sc->cursor);
    FrameRect(dc, &cur, cb);
    DeleteObject(cb);

    SelectObject(dc, old);
    if (localMono)     DeleteObject(localMono);
    if (localMonoBold) DeleteObject(localMonoBold);

    t->grid.dirty = 0;
}

/* Pick the active cell definition — user override if present, else preset. */
static const SplitCell* resolve_cell(int idx) {
    if (idx < 0 || idx >= MAX_SPLIT_CELLS) return NULL;
    if (g_app.customCells[idx].w > 0) return &g_app.customCells[idx];
    if (g_app.splitLayout < 0 || g_app.splitLayout >= PRESET_COUNT) return NULL;
    const SplitPreset* p = &kPresets[g_app.splitLayout];
    if (idx >= p->cellCount) return NULL;
    return &p->cells[idx];
}

/* Resolve a SplitCell (percent coords) against a pixel rect. We round in a
 * way that the rightmost / bottommost cell always reaches the edge of the
 * workspace, avoiding 1px gaps. */
static RECT split_cell_rect(const RECT* parent, const SplitCell* c) {
    int W = parent->right - parent->left;
    int H = parent->bottom - parent->top;
    RECT r;
    r.left   = parent->left + W * c->x / 100;
    r.top    = parent->top  + H * c->y / 100;
    r.right  = (c->x + c->w >= 100) ? parent->right  : parent->left + W * (c->x + c->w) / 100;
    r.bottom = (c->y + c->h >= 100) ? parent->bottom : parent->top  + H * (c->y + c->h) / 100;
    /* Inset 2px between adjacent cells so the divider is visible */
    if (c->x + c->w < 100) r.right -= 2;
    if (c->y + c->h < 100) r.bottom -= 2;
    return r;
}

static void draw_terminal(HDC dc, const RECT* rc) {
    /* Invalidate header button rects so stale clicks don't fire after a
     * layout change. They'll be repopulated by draw_terminal_header below. */
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) {
        SetRectEmpty(&g_app.zoomBtnRect[i]);
        SetRectEmpty(&g_app.sizeBtnRect[i]);
        SetRectEmpty(&g_app.unslotBtnRect[i]);
    }

    if (g_app.splitLayout == PRESET_SINGLE) {
        draw_terminal_cell(dc, rc, g_app.activeTab, TRUE, -1);
        return;
    }

    const SplitPreset* preset = &kPresets[g_app.splitLayout];

    /* ZOOM MODE: render only the zoomed cell at ~90% of the workspace,
     * centered, with a thin dim border so the user knows it's modal.   */
    if (g_app.zoomedCell >= 0 && g_app.zoomedCell < preset->cellCount) {
        /* Dim background to hint that other cells are still alive */
        fill_rect_color(dc, rc, RGB(0x0A, 0x0B, 0x12));
        int W = rc->right - rc->left;
        int H = rc->bottom - rc->top;
        int pad = 5; /* 5% padding around → 90% effective */
        RECT zr;
        zr.left   = rc->left + (W * pad) / 100;
        zr.top    = rc->top  + (H * pad) / 100;
        zr.right  = rc->right - (W * pad) / 100;
        zr.bottom = rc->bottom - (H * pad) / 100;
        int tabIdx = g_app.splitSlots[g_app.zoomedCell];
        draw_terminal_cell(dc, &zr, tabIdx, TRUE, g_app.zoomedCell);
        return;
    }

    for (int i = 0; i < preset->cellCount; ++i) {
        const SplitCell* c = resolve_cell(i);
        if (!c) continue;
        RECT cellRc = split_cell_rect(rc, c);
        int tabIdx = g_app.splitSlots[i];
        draw_terminal_cell(dc, &cellRc, tabIdx, i == g_app.splitActiveCell, i);
    }
}

/* ---------- Toolbar (AI launchers) ---------- */

typedef struct {
    const wchar_t* icon;
    const wchar_t* label;
    const wchar_t* command;   /* may be NULL for built-in actions */
    COLORREF color;
    int action;               /* 0 = inject command + \r, 1 = clear line, 2 = mic stub, 3 = explain stub */
} ToolbarAction;

static const ToolbarAction kToolbarActions[] = {
    { L"\U0001F916", L"Claude",   L"claude",  RGB(0xD9, 0x76, 0x06), 0 },
    { L"✨",     L"Gemini",   L"gemini",  RGB(0x1F, 0x77, 0xFF), 0 },
    { L"\U0001F9EA", L"Codex",    L"codex",   RGB(0x16, 0xA3, 0x4A), 0 },
    { L"\U0001F3A4", L"Voz",      NULL,       RGB(0xF7, 0x76, 0x8E), 2 },
    { L"✂",     L"Limpar",   NULL,       RGB(0xC0, 0xCA, 0xF5), 1 },
    { L"\U0001F4A1", L"Explicar", NULL,       RGB(0xE0, 0xAF, 0x68), 3 },
    { L"▦",     L"Layout",   NULL,       RGB(0xBB, 0x9A, 0xF7), 5 },
    { L"↩",     L"Sair split", NULL,    RGB(0xF7, 0x76, 0x8E), 6 },
    { L"⚙",     L"Config",   NULL,       RGB(0x7A, 0xA2, 0xF7), 4 },
};
#define TOOLBAR_ACTION_COUNT (sizeof(kToolbarActions)/sizeof(kToolbarActions[0]))

static void toolbar_action_rect(int idx, const RECT* rc, RECT* out) {
    int btnW = 110;
    int gap = 6;
    int x = rc->left + 10 + idx * (btnW + gap);
    out->left = x; out->right = x + btnW;
    out->top = rc->top + 4; out->bottom = rc->bottom - 4;
}

static void draw_toolbar(HDC dc, const RECT* rc) {
    fill_rect_color(dc, rc, COL_BG_SIDE);
    RECT divTop = { rc->left, rc->top, rc->right, rc->top + 1 };
    fill_rect_color(dc, &divTop, COL_DIV);

    for (size_t i = 0; i < TOOLBAR_ACTION_COUNT; ++i) {
        /* "Sair split" only appears when split is active. */
        if (kToolbarActions[i].action == 6 && g_app.splitLayout == PRESET_SINGLE)
            continue;

        RECT br;
        toolbar_action_rect((int)i, rc, &br);

        /* Mic button changes color + label while recording */
        BOOL micRecording = (kToolbarActions[i].action == 2 && g_voice.recording);
        BOOL micUploading = (kToolbarActions[i].action == 2 && g_voice.uploading);

        COLORREF bg = COL_BG_CHIP;
        COLORREF border = COL_DIV;
        if (micRecording) { bg = RGB(0x66, 0x1A, 0x22); border = RGB(0xF7, 0x76, 0x8E); }
        else if (micUploading) { bg = RGB(0x1A, 0x40, 0x66); border = RGB(0x7A, 0xA2, 0xF7); }

        draw_rounded_rect(dc, br, bg, border, 8);

        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontEmoji);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, kToolbarActions[i].color);
        GetTextExtentPoint32W(dc, kToolbarActions[i].icon, (int)wcslen(kToolbarActions[i].icon), &sz);
        int iconY = br.top + ((br.bottom - br.top) - sz.cy) / 2;
        TextOutW(dc, br.left + 10, iconY, kToolbarActions[i].icon, (int)wcslen(kToolbarActions[i].icon));

        SelectObject(dc, g_app.hFontUI);
        SetTextColor(dc, COL_FG);
        wchar_t label[64];
        if (micRecording) {
            int sec = (int)((GetTickCount() - g_voice.startTick) / 1000);
            _snwprintf_s(label, 64, _TRUNCATE, L"REC %02d:%02d", sec / 60, sec % 60);
        } else if (micUploading) {
            wcscpy_s(label, 64, L"Enviando…");
        } else {
            wcscpy_s(label, 64, kToolbarActions[i].label);
        }
        TextOutW(dc, br.left + 34, br.top + 7, label, (int)wcslen(label));
        SelectObject(dc, old);
    }
}

/* =========================================================================
 *                           STATUS BAR
 * ========================================================================= */

static void update_status(void) {
    wchar_t s[256];
    PROCESS_MEMORY_COUNTERS_EX pmc = {0};
    pmc.cb = sizeof(pmc);
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    double mb = (double)pmc.PrivateUsage / (1024.0 * 1024.0);
    Tab* t = (g_app.activeTab >= 0 && g_app.activeTab < g_app.tabCount) ? g_app.tabs[g_app.activeTab] : NULL;
    const wchar_t* shell = (t && t->session) ? t->session->shellName : L"—";
    const wchar_t* cwd   = (t && t->session && t->session->cwd[0]) ? t->session->cwd : L"~";

    _snwprintf_s(s, 256, _TRUNCATE,
                 L"Dante CLI %s   ·   %s   ·   %s   ·   RAM %.1f MB",
                 APP_VERSION_W, shell, cwd, mb);
    if (g_app.hStatus) SendMessageW(g_app.hStatus, SB_SETTEXTW, 0, (LPARAM)s);
}

/* =========================================================================
 *                           HIT TESTING
 * ========================================================================= */

static int hit_test_tab(int x, int y, const RECT* rc) {
    if (y < rc->top || y > rc->bottom) return -1;
    for (int i = 0; i < g_app.tabCount; ++i) {
        int cx = tab_chip_x(i);
        if (x >= cx && x < cx + 146) return i;
    }
    return -1;
}
static int hit_test_tab_close(int x, int y, const RECT* rc, int idx) {
    if (idx < 0 || idx >= g_app.tabCount) return 0;
    int cx = tab_chip_x(idx);
    RECT closeR = { cx + 146 - 24, rc->top + 11, cx + 146 - 4, rc->top + 28 };
    return (x >= closeR.left && x < closeR.right && y >= closeR.top && y < closeR.bottom);
}
static int hit_test_new_tab(int x, int y, const RECT* rc) {
    int px = tab_chip_x(g_app.tabCount) + 4;
    return (x >= px && x < px + 32 && y >= rc->top + 7 && y < rc->bottom - 7);
}

/* Stats chip lives on the right edge of the tab bar; recomputed every paint
 * so any width difference is absorbed naturally. We approximate the rect
 * here by reserving 140 px from the right.                              */
static int hit_test_stats_chip(int x, int y, const RECT* rc) {
    if (y < rc->top + 7 || y >= rc->bottom - 7) return 0;
    if (x < rc->right - 150 || x >= rc->right - 10) return 0;
    return 1;
}

/* Returns cell index (0..MAX_SPLIT_CELLS-1) whose zoom button was clicked,
 * or -1 if none. Rects are populated by draw_terminal_header.            */
static int hit_test_zoom_btn(int x, int y) {
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) {
        RECT r = g_app.zoomBtnRect[i];
        if (r.right == r.left || r.bottom == r.top) continue;   /* empty */
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static void toggle_zoom(int cellIdx) {
    if (g_app.splitLayout == PRESET_SINGLE) return;
    if (cellIdx < 0) cellIdx = g_app.splitActiveCell;
    if (g_app.zoomedCell == cellIdx) g_app.zoomedCell = -1;     /* same → off */
    else                              g_app.zoomedCell = cellIdx; /* others → switch */
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

static int hit_test_size_btn(int x, int y) {
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) {
        RECT r = g_app.sizeBtnRect[i];
        if (r.right == r.left || r.bottom == r.top) continue;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static int hit_test_unslot_btn(int x, int y) {
    for (int i = 0; i < MAX_SPLIT_CELLS; ++i) {
        RECT r = g_app.unslotBtnRect[i];
        if (r.right == r.left || r.bottom == r.top) continue;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

/* Empty the slot without closing the Tab. The tab stays alive elsewhere. */
static void unslot_cell(int cellIdx) {
    if (cellIdx < 0 || cellIdx >= MAX_SPLIT_CELLS) return;
    g_app.splitSlots[cellIdx] = -1;
    if (g_app.zoomedCell == cellIdx) g_app.zoomedCell = -1;
    InvalidateRect(g_app.hWnd, NULL, FALSE);
    schedule_persist();
}

/* ---- Resize popup --------------------------------------------------- */
#define RESIZE_W 320
#define RESIZE_H 340

typedef struct {
    int cellIdx;
    HWND hWnd;
    int hoverIdx;     /* 0=up, 1=down, 2=left, 3=right, 4=shrinkU, 5=shrinkD, 6=shrinkL, 7=shrinkR, 8=reset */
} ResizeCtx;

static ResizeCtx* g_resizeCtx = NULL;

static const int RESIZE_DELTA = 10;   /* percent points per click */

static void resize_apply(int cellIdx, int dxL, int dyT, int dxR, int dyB) {
    if (cellIdx < 0 || cellIdx >= MAX_SPLIT_CELLS) return;
    const SplitCell* base = resolve_cell(cellIdx);
    if (!base) return;
    SplitCell c = *base;
    c.x = (short)clamp_int(c.x + dxL, 0, 90);
    c.y = (short)clamp_int(c.y + dyT, 0, 90);
    c.w = (short)clamp_int(c.w - dxL + dxR, 10, 100);
    c.h = (short)clamp_int(c.h - dyT + dyB, 10, 100);
    if (c.x + c.w > 100) c.w = 100 - c.x;
    if (c.y + c.h > 100) c.h = 100 - c.y;
    g_app.customCells[cellIdx] = c;
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

static void resize_reset(int cellIdx) {
    if (cellIdx < 0 || cellIdx >= MAX_SPLIT_CELLS) return;
    memset(&g_app.customCells[cellIdx], 0, sizeof(SplitCell));
    InvalidateRect(g_app.hWnd, NULL, FALSE);
}

static RECT resize_btn_rect(int slot) {
    /* Grid:
     *        [ ^ ]         (0)
     *  [ ← ] [ size ] [ → ]  (2)(label)(3)
     *        [ v ]         (1)
     *
     *        [ v ]         (5 shrink down)
     *  [ → ] [ size ] [ ← ]  (6)(label)(7)
     *        [ ^ ]         (4 shrink up)
     *
     *        [ Voltar ao preset ]  (8) */
    RECT r = {0, 0, 56, 36};
    int cx = RESIZE_W / 2;
    switch (slot) {
        case 0: r.left = cx - 28;  r.top =  72; break;   /* expand up */
        case 1: r.left = cx - 28;  r.top = 152; break;   /* expand down */
        case 2: r.left = cx - 92;  r.top = 112; break;   /* expand left */
        case 3: r.left = cx + 36;  r.top = 112; break;   /* expand right */
        case 4: r.left = cx - 28;  r.top = 222; break;   /* shrink up */
        case 5: r.left = cx - 28;  r.top = 252; break;   /* shrink down */
        case 6: r.left = cx - 92;  r.top = 237; break;   /* shrink left */
        case 7: r.left = cx + 36;  r.top = 237; break;   /* shrink right */
        case 8: r.left = 20;       r.top = RESIZE_H - 44; r.right = RESIZE_W - 20; r.bottom = r.top + 32; return r;
        default: break;
    }
    r.right  = r.left + 56;
    r.bottom = r.top  + 36;
    return r;
}

static int resize_hit_test(int x, int y) {
    for (int i = 0; i < 9; ++i) {
        RECT r = resize_btn_rect(i);
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return i;
    }
    return -1;
}

static void resize_paint(HDC dc, RECT cli) {
    fill_rect_color(dc, &cli, COL_BG_SIDE);
    draw_text_w(dc, 20, 16, L"Tamanho desta célula", COL_FG, g_app.hFontUIBold);

    const SplitCell* c = resolve_cell(g_resizeCtx->cellIdx);
    if (c) {
        wchar_t info[64];
        _snwprintf_s(info, 64, _TRUNCATE, L"%d×%d %% da área",
                     c->w, c->h);
        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontUI);
        GetTextExtentPoint32W(dc, info, (int)wcslen(info), &sz);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, COL_ACCENT);
        TextOutW(dc, RESIZE_W - sz.cx - 20, 18, info, (int)wcslen(info));
        SelectObject(dc, old);
    }

    draw_text_w(dc, 20, 44, L"Expandir", COL_GREEN, g_app.hFontUIBold);
    draw_text_w(dc, 20, 200, L"Reduzir", COL_ORANGE, g_app.hFontUIBold);

    /* Draw buttons 0..7 */
    static const wchar_t* labels[8] = {
        L"↑", L"↓", L"←", L"→",
        L"↑", L"↓", L"←", L"→"
    };
    for (int i = 0; i < 8; ++i) {
        RECT r = resize_btn_rect(i);
        BOOL hover = (g_resizeCtx->hoverIdx == i);
        COLORREF bg = hover ? COL_BG_CHIP_H : COL_BG_CHIP;
        COLORREF bd = (i < 4) ? COL_GREEN : COL_ORANGE;
        draw_rounded_rect(dc, r, bg, mix_color(bd, RGB(0,0,0), 0.5), 8);
        SIZE sz;
        HFONT old = (HFONT)SelectObject(dc, g_app.hFontBig);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, bd);
        GetTextExtentPoint32W(dc, labels[i], 1, &sz);
        TextOutW(dc, r.left + ((r.right - r.left) - sz.cx) / 2,
                 r.top  + ((r.bottom - r.top) - sz.cy) / 2 - 2,
                 labels[i], 1);
        SelectObject(dc, old);
    }

    /* Reset button */
    RECT reset = resize_btn_rect(8);
    BOOL hover = (g_resizeCtx->hoverIdx == 8);
    draw_rounded_rect(dc, reset, hover ? COL_BG_CHIP_H : COL_BG_CHIP,
                      COL_RED, 8);
    SIZE rsz;
    HFONT old = (HFONT)SelectObject(dc, g_app.hFontUI);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, COL_RED);
    const wchar_t* rl = L"Voltar ao preset";
    GetTextExtentPoint32W(dc, rl, (int)wcslen(rl), &rsz);
    TextOutW(dc, reset.left + ((reset.right - reset.left) - rsz.cx) / 2,
             reset.top  + ((reset.bottom - reset.top) - rsz.cy) / 2,
             rl, (int)wcslen(rl));
    SelectObject(dc, old);
}

static void resize_action(int slot) {
    int idx = g_resizeCtx->cellIdx;
    switch (slot) {
        /* Expand */
        case 0: resize_apply(idx, 0, -RESIZE_DELTA, 0, 0); break;        /* up */
        case 1: resize_apply(idx, 0, 0, 0, RESIZE_DELTA); break;         /* down */
        case 2: resize_apply(idx, -RESIZE_DELTA, 0, 0, 0); break;        /* left */
        case 3: resize_apply(idx, 0, 0, RESIZE_DELTA, 0); break;         /* right */
        /* Shrink */
        case 4: resize_apply(idx, 0, RESIZE_DELTA, 0, 0); break;         /* shrink up = move top down */
        case 5: resize_apply(idx, 0, 0, 0, -RESIZE_DELTA); break;        /* shrink down */
        case 6: resize_apply(idx, RESIZE_DELTA, 0, 0, 0); break;         /* shrink left */
        case 7: resize_apply(idx, 0, 0, -RESIZE_DELTA, 0); break;        /* shrink right */
        case 8: resize_reset(idx); break;
    }
}

static LRESULT CALLBACK ResizeWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hWnd, &ps);
            RECT cli; GetClientRect(hWnd, &cli);
            HDC mem = CreateCompatibleDC(dc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, cli.right, cli.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);
            resize_paint(mem, cli);
            BitBlt(dc, 0, 0, cli.right, cli.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hWnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int h = resize_hit_test(x, y);
            if (h != g_resizeCtx->hoverIdx) {
                g_resizeCtx->hoverIdx = h;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            int h = resize_hit_test(x, y);
            if (h >= 0) resize_action(h);
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(wParam) == WA_INACTIVE) DestroyWindow(hWnd);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
            return 0;
        case WM_DESTROY:
            g_resizeCtx = NULL;
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void show_resize_popup(int cellIdx, int screenX, int screenY) {
    if (g_resizeCtx) { DestroyWindow(g_resizeCtx->hWnd); return; }

    static BOOL registered = FALSE;
    if (!registered) {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = ResizeWndProc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_HAND);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"DanteResizePopup";
        RegisterClassExW(&wc);
        registered = TRUE;
    }

    static ResizeCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.cellIdx = cellIdx;
    ctx.hoverIdx = -1;
    g_resizeCtx = &ctx;

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int x = screenX - RESIZE_W + 30;
    int y = screenY + 8;
    if (x < 8) x = 8;
    if (x + RESIZE_W > sw - 8) x = sw - RESIZE_W - 8;
    if (y + RESIZE_H > sh - 8) y = screenY - RESIZE_H - 8;

    ctx.hWnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        L"DanteResizePopup", L"Tamanho da célula",
        WS_POPUP | WS_BORDER,
        x, y, RESIZE_W, RESIZE_H,
        g_app.hWnd, NULL, GetModuleHandleW(NULL), NULL);
    ShowWindow(ctx.hWnd, SW_SHOW);
    SetForegroundWindow(ctx.hWnd);
}

static int hit_test_sidebar_mode(int x, int y, const RECT* rc) {
    for (int i = 0; i < MODE_COUNT; ++i) {
        RECT br; sidebar_mode_button_rect(i, rc, &br);
        if (x >= br.left && x < br.right && y >= br.top && y < br.bottom) return i;
    }
    return -1;
}
static int hit_test_sidebar_item(int x, int y, const RECT* rc) {
    int rowH = (g_app.sidebarMode == MODE_FILES) ? 28 : 46;
    /* Files mode has a small "current path" header above the list. */
    int extraHdr = (g_app.sidebarMode == MODE_FILES) ? 22 : 0;
    int top = rc->top + SIDEBAR_HDR_H + 76 + extraHdr;
    int bottom = rc->bottom - 48;
    if (x < rc->left + 12 || x > rc->right - 12) return -1;
    if (y < top || y >= bottom) return -1;
    int idx = (y - top) / rowH;
    int count = 0;
    if (g_app.sidebarMode == MODE_FAVORITES) count = g_app.favoriteCount;
    else if (g_app.sidebarMode == MODE_SNIPPETS) count = g_app.snippetCount;
    else if (g_app.sidebarMode == MODE_CREDS)    count = g_app.credCount;
    else if (g_app.sidebarMode == MODE_FILES)    count = g_app.filesCount;
    if (idx < 0 || idx >= count) return -1;
    return idx;
}

static int hit_test_sidebar_add(int x, int y, const RECT* rc) {
    return (x >= rc->left + 12 && x < rc->right - 12 &&
            y >= rc->bottom - 44 && y < rc->bottom - 12);
}

/* --------- Files mode: list/refresh/navigate the filesystem -------- */

static int wstr_cmp_ci(const void* a, const void* b) {
    return _wcsicmp((const wchar_t*)a, (const wchar_t*)b);
}

static void files_set_dir(const wchar_t* path);

static void files_refresh(void) {
    g_app.filesCount = 0;
    g_app.filesScroll = 0;

    if (!g_app.filesDir[0]) {
        /* First open — default to user home */
        SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, g_app.filesDir);
    }

    wchar_t pattern[MAX_PATH];
    _snwprintf_s(pattern, MAX_PATH, _TRUNCATE, L"%s\\*", g_app.filesDir);

    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    /* Slot 0 reserved for ".." parent navigation. */
    wcscpy_s(g_app.filesEntries[g_app.filesCount], MAX_PATH, L"..");
    g_app.filesIsDir[g_app.filesCount] = TRUE;
    g_app.filesCount++;

    do {
        if (wcscmp(fd.cFileName, L".")  == 0) continue;
        if (wcscmp(fd.cFileName, L"..") == 0) continue;
        if (g_app.filesCount >= 256) break;
        wcscpy_s(g_app.filesEntries[g_app.filesCount], MAX_PATH, fd.cFileName);
        g_app.filesIsDir[g_app.filesCount] =
            (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        g_app.filesCount++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    /* Sort entries [1..count] case-insensitive (slot 0 stays ".."). */
    if (g_app.filesCount > 2) {
        qsort(g_app.filesEntries[1],
              g_app.filesCount - 1,
              MAX_PATH * sizeof(wchar_t),
              wstr_cmp_ci);
        /* Re-derive isDir flags after sort: re-stat each entry. */
        for (int i = 1; i < g_app.filesCount; ++i) {
            wchar_t full[MAX_PATH];
            _snwprintf_s(full, MAX_PATH, _TRUNCATE, L"%s\\%s",
                         g_app.filesDir, g_app.filesEntries[i]);
            DWORD a = GetFileAttributesW(full);
            g_app.filesIsDir[i] = (a != INVALID_FILE_ATTRIBUTES) &&
                                  (a & FILE_ATTRIBUTE_DIRECTORY);
        }
    }
}

static void files_set_dir(const wchar_t* path) {
    str_copy_w(g_app.filesDir, MAX_PATH, path);
    files_refresh();
}

static void files_navigate_up(void) {
    wchar_t buf[MAX_PATH];
    str_copy_w(buf, MAX_PATH, g_app.filesDir);
    /* Strip trailing backslash */
    size_t n = wcslen(buf);
    while (n > 0 && (buf[n-1] == L'\\' || buf[n-1] == L'/')) buf[--n] = 0;
    wchar_t* slash = wcsrchr(buf, L'\\');
    if (!slash) slash = wcsrchr(buf, L'/');
    if (slash && slash != buf) { *slash = 0; files_set_dir(buf); }
    else if (slash) {
        /* root drive — keep the backslash */
        slash[1] = 0; files_set_dir(buf);
    }
}

static void on_sidebar_item_activated(int idx) {
    if (idx < 0) return;
    if (g_app.sidebarMode == MODE_FAVORITES && idx < g_app.favoriteCount) {
        wchar_t buf[1024];
        _snwprintf_s(buf, 1024, _TRUNCATE, L"cd \"%s\"\r", g_app.favorites[idx].path);
        inject_into_active(buf);
    } else if (g_app.sidebarMode == MODE_SNIPPETS && idx < g_app.snippetCount) {
        /* Snippets do NOT auto-press Enter — user reviews before running */
        inject_into_active(g_app.snippets[idx].cmd);
    } else if (g_app.sidebarMode == MODE_CREDS && idx < g_app.credCount) {
        Credential* c = &g_app.creds[idx];
        wchar_t buf[2048];
        _snwprintf_s(buf, 2048, _TRUNCATE,
            L"# === Credencial: %s (%s) ===\r"
            L"# user: %s\r"
            L"# host: %s\r"
            L"# === fim ===\r",
            c->name, c->kind, c->user, c->host);
        inject_into_active(buf);
    } else if (g_app.sidebarMode == MODE_FILES && idx < g_app.filesCount) {
        if (idx == 0) {                       /* ".." */
            files_navigate_up();
            InvalidateRect(g_app.hWnd, NULL, FALSE);
        } else if (g_app.filesIsDir[idx]) {
            wchar_t nd[MAX_PATH];
            _snwprintf_s(nd, MAX_PATH, _TRUNCATE, L"%s\\%s",
                         g_app.filesDir, g_app.filesEntries[idx]);
            files_set_dir(nd);
            InvalidateRect(g_app.hWnd, NULL, FALSE);
        } else {
            /* File → inject quoted path into the active terminal */
            wchar_t buf[MAX_PATH + 8];
            _snwprintf_s(buf, MAX_PATH + 8, _TRUNCATE,
                         L"\"%s\\%s\" ", g_app.filesDir, g_app.filesEntries[idx]);
            inject_into_active(buf);
        }
    }
}

static int hit_test_toolbar_action(int x, int y, const RECT* rc) {
    for (int i = 0; i < (int)TOOLBAR_ACTION_COUNT; ++i) {
        /* Skip hidden "Sair split" when not in split mode. */
        if (kToolbarActions[i].action == 6 && g_app.splitLayout == PRESET_SINGLE)
            continue;
        RECT br; toolbar_action_rect(i, rc, &br);
        if (x >= br.left && x < br.right && y >= br.top && y < br.bottom) return i;
    }
    return -1;
}

/* =========================================================================
 *                          MESSAGE LOOP
 * ========================================================================= */

static void send_key_text(const char* s) {
    int idx = active_tab_index();
    if (idx < 0 || idx >= g_app.tabCount) return;
    Tab* t = g_app.tabs[idx];
    if (!t || !t->session) return;
    session_write(t->session, s, (int)strlen(s));
}

static void on_keypress(WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    int idx = active_tab_index();
    if (idx < 0 || idx >= g_app.tabCount) return;
    Tab* t = g_app.tabs[idx];
    if (!t || !t->session) return;

    BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;

    if (ctrl && wParam >= 'A' && wParam <= 'Z') {
        char ctrlByte = (char)(wParam - 'A' + 1);
        session_write(t->session, &ctrlByte, 1);
        return;
    }

    /* Backspace family:
     *   - Backspace        → DEL (0x7F)  ⇒ PSReadLine BackwardDeleteChar
     *   - Ctrl+Backspace   → ETB (0x17)  ⇒ PSReadLine BackwardKillWord
     *   - Alt+Backspace    → ESC + DEL   ⇒ word boundary variant
     * 0x7F is what xterm/Windows Terminal/PSReadLine all expect for a
     * single-char erase; older 0x08 (BS) was triggering BackwardKillWord
     * in some PSReadLine binds and erased a whole word.                  */
    if (wParam == VK_BACK) {
        if (ctrl)      { char b = 0x17; session_write(t->session, &b, 1); }
        else if (alt)  { session_write(t->session, "\x1B\x7F", 2); }
        else           { char b = 0x7F; session_write(t->session, &b, 1); }
        return;
    }

    switch (wParam) {
        case VK_RETURN: send_key_text("\r"); break;
        case VK_TAB:    send_key_text("\t"); break;
        case VK_ESCAPE: send_key_text("\x1B"); break;
        case VK_UP:     send_key_text("\x1B[A"); break;
        case VK_DOWN:   send_key_text("\x1B[B"); break;
        case VK_RIGHT:  send_key_text("\x1B[C"); break;
        case VK_LEFT:   send_key_text("\x1B[D"); break;
        case VK_HOME:   send_key_text("\x1B[H"); break;
        case VK_END:    send_key_text("\x1B[F"); break;
        case VK_DELETE: send_key_text("\x1B[3~"); break;
        case VK_PRIOR:  send_key_text("\x1B[5~"); break;
        case VK_NEXT:   send_key_text("\x1B[6~"); break;
    }
}

static void on_char(WPARAM wParam) {
    int idx = active_tab_index();
    if (idx < 0) return;
    BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (ctrl) return; /* handled in keydown */

    wchar_t wc = (wchar_t)wParam;

    /* TranslateMessage produces a WM_CHAR for every WM_KEYDOWN with a
     * printable mapping, including CR (0x0D), TAB (0x09), BS (0x08) and
     * ESC (0x1B). on_keypress() already sent those — bail out here to
     * avoid the duplicate byte that produced "²", phantom prompts, and
     * Backspace eating two characters. Also drop any other C0 control
     * (Ctrl+letter is handled in on_keypress).                         */
    if (wc < 0x20 || wc == 0x7F) return;

    char utf[8] = {0};
    int n = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, utf, sizeof(utf), NULL, NULL);
    if (n > 0) {
        Tab* t = g_app.tabs[idx];
        if (t && t->session) session_write(t->session, utf, n);
    }
}

static void perform_toolbar_action(int idx) {
    if (idx < 0 || idx >= (int)TOOLBAR_ACTION_COUNT) return;
    const ToolbarAction* a = &kToolbarActions[idx];
    switch (a->action) {
        case 0: {
            wchar_t buf[128];
            _snwprintf_s(buf, 128, _TRUNCATE, L"%s\r", a->command);
            inject_into_active(buf);
            break;
        }
        case 1: send_key_text("\x15"); break;  /* Ctrl-U */
        case 2:
            voice_toggle();
            break;
        case 3:
            groq_explain_active_terminal();
            break;
        case 4:
            show_settings_dialog();
            break;
        case 5: {
            /* Shift = quick popup menu; default = visual gallery. */
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                POINT pt; GetCursorPos(&pt); ScreenToClient(g_app.hWnd, &pt);
                show_split_menu(g_app.hWnd, pt.x, pt.y);
            } else {
                show_layout_gallery();
            }
            break;
        }
        case 6:   /* Sair do split */
            set_split_layout(PRESET_SINGLE);
            break;
    }
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
            InitCommonControlsEx(&icc);
            g_app.hWnd = hWnd;
            DragAcceptFiles(hWnd, TRUE);   /* accept Explorer drag&drop */
            g_app.brBg       = CreateSolidBrush(COL_BG);
            g_app.brBgSide   = CreateSolidBrush(COL_BG_SIDE);
            g_app.brBgTabBar = CreateSolidBrush(COL_BG_TABBAR);
            g_app.brBgTerm   = CreateSolidBrush(COL_BG_TERM);
            g_app.brBgStatus = CreateSolidBrush(COL_BG_STATUS);
            g_app.brBgChip   = CreateSolidBrush(COL_BG_CHIP);
            g_app.brBgChipH  = CreateSolidBrush(COL_BG_CHIP_H);
            g_app.brAccent   = CreateSolidBrush(COL_ACCENT);
            g_app.penDiv     = CreatePen(PS_SOLID, 1, COL_DIV);
            g_app.penAccent  = CreatePen(PS_SOLID, 2, COL_ACCENT);

            g_app.hFontUI = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_app.hFontUIBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            g_app.hFontMono = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                          CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
            g_app.hFontMonoBold = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
            g_app.hFontEmoji = CreateFontW(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                           CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Emoji");
            g_app.hFontBig = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

            g_app.hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
                                            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                            0, 0, 0, 0,
                                            hWnd, (HMENU)IDC_STATIC, GetModuleHandleW(NULL), NULL);

            voice_init();

            /* 1 s tick — also drives the REC mm:ss label during voice
             * capture and the status-bar RAM update. */
            g_app.statsTimer = SetTimer(hWnd, 1, 1000, NULL);
            g_app.repaintTimer = SetTimer(hWnd, 2, 33, NULL);
            g_app.persistTimer = SetTimer(hWnd, 3, 1500, NULL);
            g_app.dragTabIdx = -1;
            g_app.tabBarHoverIdx = -1;
            g_app.sidebarHoverIdx = -1;
            g_app.sidebarItemHoverIdx = -1;
            for (int i = 0; i < MAX_SPLIT_CELLS; ++i) g_app.splitSlots[i] = -1;
            g_app.splitLayout = PRESET_SINGLE;
            g_app.splitActiveCell = 0;
            g_app.zoomedCell = -1;

            return 0;
        }

        case WM_SIZE: {
            if (g_app.hStatus) SendMessageW(g_app.hStatus, WM_SIZE, 0, 0);
            /* Debounce — terminal_grid_resize allocates new Cell arrays on
             * every grid change; doing it 60×/s during a window drag wastes
             * megabytes. Defer to a 150 ms one-shot timer.                */
            SetTimer(hWnd, 4, 150, NULL);
            return 0;
        }

        case WM_DPICHANGED: {
            /* Win10+ PerMonitorV2 — manifest already declares awareness, this
             * handler is responsible for honoring the suggested rect AND
             * refreshing the cached cellW/cellH so terminal text scales.   */
            UINT dpi = HIWORD(wParam);
            LOG_INFO(L"WM_DPICHANGED dpi=%u", dpi);
            const RECT* sug = (const RECT*)lParam;
            if (sug) {
                SetWindowPos(hWnd, NULL,
                             sug->left, sug->top,
                             sug->right - sug->left,
                             sug->bottom - sug->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            /* Force cell remeasure next paint by zeroing the cached metrics. */
            g_app.cellW = 0;
            g_app.cellH = 0;
            InvalidateRect(hWnd, NULL, TRUE);
            return 0;
        }

        case WM_ERASEBKGND: return 1;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT cli; GetClientRect(hWnd, &cli);
            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, cli.right, cli.bottom);
            HBITMAP oldBmp = (HBITMAP)SelectObject(mem, bmp);

            RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
            layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);

            fill_rect_color(mem, &cli, COL_BG);
            draw_sidebar(mem, &sideRc);
            draw_tabbar(mem, &tabRc);
            draw_terminal(mem, &termRc);
            draw_toolbar(mem, &toolbarRc);

            if (g_app.cheatsheetVisible) draw_cheatsheet(mem, &cli);
            if (voice_is_recording() || voice_is_uploading()) draw_voice_overlay(mem, &cli);

            BitBlt(hdc, 0, 0, cli.right, cli.bottom, mem, 0, 0, SRCCOPY);

            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);

            EndPaint(hWnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
            layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);

            if (g_app.cheatsheetVisible) { cheatsheet_toggle(); return 0; }

            /* Voice overlay click handling — Stop button stops recording. */
            if (voice_is_recording() || voice_is_uploading()) {
                if (hit_test_voice_stop(x, y)) { voice_stop_and_upload(); return 0; }
                return 0;
            }

            int mode = hit_test_sidebar_mode(x, y, &sideRc);
            if (mode >= 0) {
                g_app.sidebarMode = (SidebarMode)mode;
                if (g_app.sidebarMode == MODE_FILES && g_app.filesCount == 0) {
                    files_refresh();
                }
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }

            if (hit_test_sidebar_add(x, y, &sideRc)) { show_add_dialog(); return 0; }

            int sItem = hit_test_sidebar_item(x, y, &sideRc);
            if (sItem >= 0) { on_sidebar_item_activated(sItem); return 0; }

            int tab = hit_test_tab(x, y, &tabRc);
            if (tab >= 0) {
                if (hit_test_tab_close(x, y, &tabRc, tab)) { close_tab(tab); return 0; }
                g_app.activeTab = tab;
                g_app.dragTabIdx = tab;
                g_app.dragStartX = x;
                g_app.dragOffsetX = x - tab_chip_x(tab);
                SetCapture(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
                update_status();
                return 0;
            }
            /* Header buttons (split mode) — top priority. */
            {
                int uc = hit_test_unslot_btn(x, y);
                if (uc >= 0) { unslot_cell(uc); return 0; }
            }
            {
                int zc = hit_test_zoom_btn(x, y);
                if (zc >= 0) { toggle_zoom(zc); return 0; }
            }
            /* Resize button anchors a popup window. */
            {
                int sc = hit_test_size_btn(x, y);
                if (sc >= 0) {
                    POINT pt = { x, y };
                    ClientToScreen(hWnd, &pt);
                    show_resize_popup(sc, pt.x, pt.y);
                    return 0;
                }
            }

            if (hit_test_new_tab(x, y, &tabRc)) { open_new_tab(L"powershell"); return 0; }
            if (hit_test_stats_chip(x, y, &tabRc)) {
                POINT screenPt = { x, tabRc.bottom };
                ClientToScreen(hWnd, &screenPt);
                show_resource_monitor(screenPt.x, screenPt.y);
                return 0;
            }

            int act = hit_test_toolbar_action(x, y, &toolbarRc);
            if (act >= 0) { perform_toolbar_action(act); return 0; }

            /* Click inside terminal area + split → activate the cell */
            if (g_app.splitLayout != PRESET_SINGLE &&
                x >= termRc.left && x < termRc.right &&
                y >= termRc.top && y < termRc.bottom) {
                const SplitPreset* preset = &kPresets[g_app.splitLayout];
                for (int i = 0; i < preset->cellCount; ++i) {
                    const SplitCell* sc = resolve_cell(i);
                    if (!sc) continue;
                    RECT cellRc = split_cell_rect(&termRc, sc);
                    if (x >= cellRc.left && x < cellRc.right &&
                        y >= cellRc.top && y < cellRc.bottom) {
                        g_app.splitActiveCell = i;
                        InvalidateRect(hWnd, NULL, FALSE);
                        update_status();
                        break;
                    }
                }
            }

            SetFocus(hWnd);
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_app.dragTabIdx >= 0) {
                ReleaseCapture();
                int x = LOWORD(lParam), y = HIWORD(lParam);
                RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
                layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);

                /* Dropped over the terminal area in split mode → assign that
                 * tab to whichever slot the cursor is hovering.            */
                if (g_app.splitLayout != PRESET_SINGLE &&
                    x >= termRc.left && x < termRc.right &&
                    y >= termRc.top && y < termRc.bottom) {
                    const SplitPreset* preset = &kPresets[g_app.splitLayout];
                    for (int i = 0; i < preset->cellCount; ++i) {
                        const SplitCell* sc = resolve_cell(i);
                        if (!sc) continue;
                        RECT cellRc = split_cell_rect(&termRc, sc);
                        if (x >= cellRc.left && x < cellRc.right &&
                            y >= cellRc.top && y < cellRc.bottom) {
                            g_app.splitSlots[i] = g_app.dragTabIdx;
                            g_app.splitActiveCell = i;
                            schedule_persist();
                            break;
                        }
                    }
                }
                /* Standard tab-bar reorder. */
                int target = hit_test_tab(x, y, &tabRc);
                if (target >= 0 && target != g_app.dragTabIdx) {
                    Tab* moving = g_app.tabs[g_app.dragTabIdx];
                    if (target > g_app.dragTabIdx) {
                        for (int i = g_app.dragTabIdx; i < target; ++i) g_app.tabs[i] = g_app.tabs[i + 1];
                    } else {
                        for (int i = g_app.dragTabIdx; i > target; --i) g_app.tabs[i] = g_app.tabs[i - 1];
                    }
                    g_app.tabs[target] = moving;
                    g_app.activeTab = target;
                    schedule_persist();
                }
                g_app.dragTabIdx = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
            layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);
            int tab = hit_test_tab(x, y, &tabRc);
            if (tab >= 0) {
                wchar_t newName[128] = {0};
                str_copy_w(newName, 128, g_app.tabs[tab]->title);
                if (prompt_text(L"Renomear aba", L"Novo título:", newName, 128)) {
                    str_copy_w(g_app.tabs[tab]->title, 128, newName);
                    schedule_persist();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            }
            return 0;
        }

        case WM_RBUTTONUP: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
            layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);
            int tab = hit_test_tab(x, y, &tabRc);
            if (tab >= 0) show_tab_context_menu(hWnd, tab, x, y);
            return 0;
        }

        case WM_MOUSEMOVE: {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            RECT sideRc, tabRc, termRc, toolbarRc, statusRc;
            layout_compute(&sideRc, &tabRc, &termRc, &toolbarRc, &statusRc);
            int newHoverTab = hit_test_tab(x, y, &tabRc);
            int newHoverSide = hit_test_sidebar_mode(x, y, &sideRc);
            int newHoverItem = hit_test_sidebar_item(x, y, &sideRc);
            if (newHoverTab != g_app.tabBarHoverIdx ||
                newHoverSide != g_app.sidebarHoverIdx ||
                newHoverItem != g_app.sidebarItemHoverIdx) {
                g_app.tabBarHoverIdx = newHoverTab;
                g_app.sidebarHoverIdx = newHoverSide;
                g_app.sidebarItemHoverIdx = newHoverItem;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_KEYDOWN: {
            /* Global shortcuts before forwarding to PTY */
            BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (ctrl && wParam == VK_OEM_2) { cheatsheet_toggle(); return 0; }    /* Ctrl+/ */
            if (ctrl && wParam == VK_OEM_COMMA) { show_settings_dialog(); return 0; }  /* Ctrl+, */
            if (wParam == VK_F1) { show_about(); return 0; }
            if (ctrl && wParam == 'D' && !shift) { duplicate_tab(g_app.activeTab); return 0; }
            if (ctrl && shift && wParam == 'Z') { toggle_zoom(g_app.splitActiveCell); return 0; }
            /* Esc closes zoom if zoom is active (and cheatsheet isn't). */
            if (wParam == VK_ESCAPE && g_app.zoomedCell >= 0 && !g_app.cheatsheetVisible) {
                g_app.zoomedCell = -1;
                InvalidateRect(hWnd, NULL, FALSE);
                return 0;
            }
            if (g_app.cheatsheetVisible && wParam == VK_ESCAPE) { cheatsheet_toggle(); return 0; }
            if (ctrl && wParam == 'T' && !shift) { open_new_tab(L"powershell"); return 0; }
            if (ctrl && wParam == 'W' && !shift) {
                if (g_app.activeTab >= 0) close_tab(g_app.activeTab);
                return 0;
            }
            if (ctrl && wParam == VK_TAB) {
                if (g_app.tabCount > 0) {
                    int next = (g_app.activeTab + (shift ? -1 : 1) + g_app.tabCount) % g_app.tabCount;
                    g_app.activeTab = next;
                    InvalidateRect(hWnd, NULL, FALSE);
                    update_status();
                }
                return 0;
            }
            if (ctrl && wParam >= '1' && wParam <= '9') {
                int idx = (int)(wParam - '1');
                if (idx < g_app.tabCount) {
                    g_app.activeTab = idx;
                    InvalidateRect(hWnd, NULL, FALSE);
                    update_status();
                }
                return 0;
            }
            on_keypress(wParam, lParam);
            return 0;
        }
        case WM_SYSKEYDOWN: {
            /* Alt+key. We only intercept Alt+Backspace; everything else
             * (Alt+F4, Alt-menu accelerators) goes to DefWindowProc.  */
            if (wParam == VK_BACK) {
                on_keypress(wParam, lParam);
                return 0;
            }
            break;
        }
        case WM_CHAR:    on_char(wParam); return 0;
        case WM_SYSCHAR:
            /* Suppress the system bell when Alt+key has been handled. */
            if (wParam == VK_BACK) return 0;
            break;

        case WM_TIMER:
            if (wParam == 1) {
                resmon_sample();
                /* While the monitor popover is open, force it to refresh too. */
                if (g_monitorHwnd) InvalidateRect(g_monitorHwnd, NULL, FALSE);
                /* Also redraw the main window so the chip + headers update. */
                InvalidateRect(hWnd, NULL, FALSE);
                update_status();
                /* While recording, the REC mm:ss label must tick visibly.
                 * Without this the toolbar button stayed at "REC 00:00"
                 * because nothing else was invalidating during voice cap. */
                if (g_voice.recording) {
                    /* Enforce hard cap so a forgotten mic doesn't gravar
                     * indefinidamente — Whisper rejects very long takes. */
                    DWORD elapsed = (GetTickCount() - g_voice.startTick) / 1000;
                    if (elapsed >= VOICE_MAX_SECONDS) voice_stop_and_upload();
                    InvalidateRect(hWnd, NULL, FALSE);
                }
            } else if (wParam == 2) {
                if (g_app.pendingRepaint) { g_app.pendingRepaint = 0; InvalidateRect(hWnd, NULL, FALSE); }
            } else if (wParam == 3) {
                if (g_app.pendingPersist) { g_app.pendingPersist = 0; persist_save(); }
            } else if (wParam == 4) {
                /* Resize debounce expired — kill the one-shot and repaint
                 * so the grid is recomputed exactly once for the new size. */
                KillTimer(hWnd, 4);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;

        case WM_DANTE_UPDATE_RESULT: {
            wchar_t* remote = (wchar_t*)lParam;
            if (remote && wcscmp(remote, APP_VERSION_W) > 0) {
                wchar_t msg[512];
                _snwprintf_s(msg, 512, _TRUNCATE,
                    L"Há uma versão nova disponível!\n\n"
                    L"Sua versão: %s\n"
                    L"Disponível: %s\n\n"
                    L"Abrir página de download?",
                    APP_VERSION_W, remote);
                if (MessageBoxW(g_app.hWnd, msg, APP_NAME_W,
                                MB_ICONINFORMATION | MB_YESNO) == IDYES) {
                    ShellExecuteW(g_app.hWnd, L"open",
                        L"https://github.com/dantetesta/dante-cli/releases/latest",
                        NULL, NULL, SW_SHOWNORMAL);
                }
            }
            free(remote);
            return 0;
        }

        case WM_DANTE_VOICE_DONE: {
            g_voice.uploading = FALSE;
            wchar_t* text = (wchar_t*)lParam;
            if (text && text[0]) {
                size_t L = wcslen(text);
                wchar_t* withCr = (wchar_t*)malloc((L + 4) * sizeof(wchar_t));
                wcscpy_s(withCr, L + 4, text);
                wcscat_s(withCr, L + 4, L" ");
                inject_into_active(withCr);
                free(withCr);
            }
            free(text);
            InvalidateRect(g_app.hWnd, NULL, FALSE);
            return 0;
        }
        case WM_DANTE_VOICE_ERROR: {
            g_voice.uploading = FALSE;
            wchar_t* msg = (wchar_t*)lParam;
            MessageBoxW(g_app.hWnd, msg, APP_NAME_W, MB_ICONERROR);
            free(msg);
            InvalidateRect(g_app.hWnd, NULL, FALSE);
            return 0;
        }

        case WM_DANTE_GROQ_RESULT: {
            g_groqInFlight = FALSE;
            wchar_t* text = (wchar_t*)lParam;
            show_groq_result(L"Explicação (Groq)", text);
            return 0;
        }
        case WM_DANTE_GROQ_ERROR: {
            g_groqInFlight = FALSE;
            wchar_t* msg = (wchar_t*)lParam;
            MessageBoxW(g_app.hWnd, msg, APP_NAME_W, MB_ICONERROR);
            free(msg);
            return 0;
        }

        case WM_DANTE_OUTPUT: {
            char* heap = (char*)lParam;
            if (!heap) return 0;
            int tabId = *(int*)heap;
            int n     = *(int*)(heap + sizeof(int));
            const char* bytes = heap + sizeof(int) * 2;

            /* Find tab by stable id — Session* could be dangling if the tab
             * was closed between the post and the dispatch.                */
            Tab* target = NULL;
            for (int i = 0; i < g_app.tabCount; ++i) {
                if (g_app.tabs[i] && g_app.tabs[i]->id == tabId) {
                    target = g_app.tabs[i]; break;
                }
            }
            if (target) {
                parser_feed(target, bytes, n);
                g_app.pendingRepaint = 1;
            }
            free(heap);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT n = DragQueryFileW(hDrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < n; ++i) {
                wchar_t path[MAX_PATH] = {0};
                DragQueryFileW(hDrop, i, path, MAX_PATH);
                DWORD attr = GetFileAttributesW(path);
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
                    open_new_tab(L"powershell");
                    Tab* nt = g_app.tabs[g_app.tabCount - 1];
                    str_copy_w(nt->title, 128, wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path);
                    str_copy_w(nt->emoji, 8, L"\U0001F4C1");
                    wchar_t cmd[MAX_PATH + 16];
                    _snwprintf_s(cmd, MAX_PATH + 16, _TRUNCATE, L"cd \"%s\"\r", path);
                    Sleep(150);
                    inject_into_active(cmd);
                } else if (is_text_file(path)) {
                    /* Text-like file → open in the editor preview. */
                    show_editor_preview(path);
                } else {
                    /* Other file → inject quoted path into active terminal. */
                    wchar_t buf[MAX_PATH + 8];
                    _snwprintf_s(buf, MAX_PATH + 8, _TRUNCATE, L"\"%s\" ", path);
                    inject_into_active(buf);
                }
            }
            DragFinish(hDrop);
            SetForegroundWindow(hWnd);
            return 0;
        }

        case WM_QUERYENDSESSION:
            /* Logoff / shutdown — save now and allow the close to proceed. */
            persist_save();
            return TRUE;
        case WM_ENDSESSION:
            if (wParam) persist_save();
            return 0;

        case WM_DESTROY:
            persist_save();
            /* Tear down any orphan popovers so they don't outlive us as
             * top-level zombie windows.                                */
            if (g_monitorHwnd && IsWindow(g_monitorHwnd)) DestroyWindow(g_monitorHwnd);
            if (g_resizeCtx && g_resizeCtx->hWnd && IsWindow(g_resizeCtx->hWnd))
                DestroyWindow(g_resizeCtx->hWnd);
            if (g_galCtx && g_galCtx->hWnd && IsWindow(g_galCtx->hWnd))
                DestroyWindow(g_galCtx->hWnd);
            if (g_emojiCtx && g_emojiCtx->hWnd && IsWindow(g_emojiCtx->hWnd))
                DestroyWindow(g_emojiCtx->hWnd);
            /* Wipe the API key from memory before exit. */
            SecureZeroMemory(g_app.groqApiKey, sizeof(g_app.groqApiKey));
            for (int i = 0; i < g_app.tabCount; ++i) {
                if (g_app.tabs[i]) {
                    session_destroy(g_app.tabs[i]->session);
                    terminal_grid_free(&g_app.tabs[i]->grid);
                    free(g_app.tabs[i]);
                    g_app.tabs[i] = NULL;
                }
            }
            g_app.tabCount = 0;
            DeleteObject(g_app.brBg);
            DeleteObject(g_app.brBgSide);
            DeleteObject(g_app.brBgTabBar);
            DeleteObject(g_app.brBgTerm);
            DeleteObject(g_app.brBgStatus);
            DeleteObject(g_app.brBgChip);
            DeleteObject(g_app.brBgChipH);
            DeleteObject(g_app.brAccent);
            DeleteObject(g_app.penDiv);
            DeleteObject(g_app.penAccent);
            DeleteObject(g_app.hFontUI);
            DeleteObject(g_app.hFontUIBold);
            DeleteObject(g_app.hFontMono);
            DeleteObject(g_app.hFontMonoBold);
            DeleteObject(g_app.hFontEmoji);
            DeleteObject(g_app.hFontBig);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

/* =========================================================================
 *                              ENTRY POINT
 * ========================================================================= */

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nShow) {
    (void)hPrev; (void)cmd;

    if (!load_conpty_apis()) {
        MessageBoxW(NULL,
            L"ConPTY não está disponível.\n\nWindows 10 versão 1809 (build 17763) ou Windows 11 é obrigatório.\n\nAtualize o sistema e tente novamente.",
            APP_NAME_W, MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = APP_WINDOW_CLS;
    wc.hIcon         = LoadIconW(hInst, L"DanteIcon");
    wc.hIconSm       = LoadIconW(hInst, L"DanteIcon");
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int W = min_int(1280, sw - 100);
    int H = min_int(800,  sh - 100);

    HWND hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        APP_WINDOW_CLS,
        APP_NAME_W,
        WS_OVERLAPPEDWINDOW,
        (sw - W) / 2, (sh - H) / 2, W, H,
        NULL, NULL, hInst, NULL);
    if (!hWnd) return 2;

    LOG_INFO(L"Dante CLI %s starting", APP_VERSION_W);
    persist_load();
    if (g_app.tabCount == 0) open_new_tab(L"powershell");
    else g_app.activeTab = 0;

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);
    update_status();
    LOG_INFO(L"window shown, tabs=%d, scheme=%s", g_app.tabCount,
             kSchemes[g_schemeIdx].id);
    check_for_updates_async();

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
