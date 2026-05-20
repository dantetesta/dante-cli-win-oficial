/*
 * Dante CLI — Bootstrap launcher (Win32 native, no Qt).
 *
 * Quando o usuário clica no atalho instalado, este executável abre uma
 * janela explicando o status do projeto e oferecendo:
 *   - "Abrir terminal padrão" (cmd.exe) com aviso
 *   - "Ver código no GitHub" / abrir README local
 *   - "Compilar versão completa" (abre INSTALL.md)
 *
 * Esta é a versão portátil cross-compilada com mingw-w64 que garante que o
 * instalador entregue um .exe FUNCIONAL desde a primeira instalação. Quando
 * o build Qt 6 + MSVC for executado, o instalador substitui este stub pelo
 * binário completo.
 */

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>

#define APP_NAME           L"Dante CLI"
#define APP_VERSION        L"1.0.19-stub"
#define APP_BUILD_DATE     L"2026"
#define APP_URL            L"https://github.com/dantetesta/dante-cli"
#define APP_DOCS_URL       L"https://github.com/dantetesta/dante-cli/blob/main/INSTALL.md"

#define ID_BTN_TERMINAL    1001
#define ID_BTN_DOCS        1002
#define ID_BTN_GITHUB      1003
#define ID_BTN_CLOSE       1004

static HFONT g_titleFont = NULL;
static HFONT g_bodyFont  = NULL;
static HFONT g_smallFont = NULL;
static HFONT g_btnFont   = NULL;
static HBRUSH g_bgBrush  = NULL;
static HICON  g_appIcon  = NULL;

static const COLORREF kBgColor      = RGB(0x0A, 0x12, 0x08);
static const COLORREF kAccentGreen  = RGB(0x3D, 0xD6, 0x68);
static const COLORREF kTextLight    = RGB(0xE6, 0xF7, 0xE8);
static const COLORREF kTextDim      = RGB(0x80, 0xA0, 0x88);

static void
launch_shell(HWND parent) {
    wchar_t cmdline[MAX_PATH * 2];
    GetEnvironmentVariableW(L"COMSPEC", cmdline, MAX_PATH);
    if (cmdline[0] == 0) lstrcpyW(cmdline, L"cmd.exe");

    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE,
                        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
        MessageBoxW(parent,
            L"Não foi possível abrir o cmd.exe.",
            APP_NAME, MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static void
open_url(HWND parent, LPCWSTR url) {
    HINSTANCE r = ShellExecuteW(parent, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) {
        MessageBoxW(parent, L"Não foi possível abrir o navegador padrão.",
                    APP_NAME, MB_ICONERROR);
    }
}

static void
open_local_doc(HWND parent) {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wchar_t* slash = wcsrchr(exePath, L'\\');
    if (slash) *slash = 0;

    wchar_t docPath[MAX_PATH];
    swprintf(docPath, MAX_PATH, L"%s\\INSTALL.md", exePath);

    HINSTANCE r = ShellExecuteW(parent, L"open", docPath, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32) {
        /* Fallback to remote */
        open_url(parent, APP_DOCS_URL);
    }
}

static HFONT
make_font(int height, int weight, LPCWSTR name) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, name);
}

static LRESULT CALLBACK
wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_bgBrush    = CreateSolidBrush(kBgColor);
        g_titleFont  = make_font(-36, FW_BOLD,    L"Segoe UI");
        g_bodyFont   = make_font(-16, FW_REGULAR, L"Segoe UI");
        g_smallFont  = make_font(-13, FW_REGULAR, L"Consolas");
        g_btnFont    = make_font(-15, FW_SEMIBOLD,L"Segoe UI");
        g_appIcon    = LoadIconW(GetModuleHandleW(NULL), L"DanteIcon");
        if (g_appIcon) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_appIcon);
            SendMessageW(hwnd, WM_SETICON, ICON_BIG,   (LPARAM)g_appIcon);
        }

        HWND btn1 = CreateWindowExW(0, L"BUTTON", L"   ▶  Abrir terminal padrão (cmd)",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            48, 380, 320, 44, hwnd, (HMENU)ID_BTN_TERMINAL,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(btn1, WM_SETFONT, (WPARAM)g_btnFont, TRUE);

        HWND btn2 = CreateWindowExW(0, L"BUTTON", L"   📖  Guia de instalação completa",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            48, 432, 320, 44, hwnd, (HMENU)ID_BTN_DOCS,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(btn2, WM_SETFONT, (WPARAM)g_btnFont, TRUE);

        HWND btn3 = CreateWindowExW(0, L"BUTTON", L"   ⌥  Ver código no GitHub",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            48, 484, 320, 44, hwnd, (HMENU)ID_BTN_GITHUB,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(btn3, WM_SETFONT, (WPARAM)g_btnFont, TRUE);

        HWND btn4 = CreateWindowExW(0, L"BUTTON", L"Fechar",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            390, 484, 120, 44, hwnd, (HMENU)ID_BTN_CLOSE,
            GetModuleHandleW(NULL), NULL);
        SendMessageW(btn4, WM_SETFONT, (WPARAM)g_btnFont, TRUE);
        return 0;
    }

    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wParam;
        SetBkColor(dc, kBgColor);
        SetTextColor(dc, kTextLight);
        return (LRESULT)g_bgBrush;
    }

    case WM_ERASEBKGND: {
        HDC dc = (HDC)wParam;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_bgBrush);
        return 1;
    }

    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        if (!dis) break;

        int id = (int)wParam;
        BOOL hover = (dis->itemState & ODS_HOTLIGHT) || (dis->itemState & ODS_FOCUS);
        BOOL pressed = (dis->itemState & ODS_SELECTED);

        HBRUSH bgb;
        COLORREF border;
        COLORREF text = kTextLight;

        if (id == ID_BTN_CLOSE) {
            bgb = CreateSolidBrush(pressed ? RGB(0x18, 0x22, 0x18) : RGB(0x12, 0x1A, 0x12));
            border = kTextDim;
            text = kTextDim;
        } else {
            bgb = CreateSolidBrush(pressed ? RGB(0x18, 0x36, 0x1E) :
                                    hover ? RGB(0x12, 0x28, 0x16) : RGB(0x0C, 0x1C, 0x10));
            border = kAccentGreen;
        }
        FillRect(dis->hDC, &dis->rcItem, bgb);
        DeleteObject(bgb);

        HPEN pen = CreatePen(PS_SOLID, 2, border);
        HGDIOBJ oldPen = SelectObject(dis->hDC, pen);
        HGDIOBJ oldBr  = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
        RoundRect(dis->hDC,
                  dis->rcItem.left + 1, dis->rcItem.top + 1,
                  dis->rcItem.right - 1, dis->rcItem.bottom - 1, 10, 10);
        SelectObject(dis->hDC, oldPen);
        SelectObject(dis->hDC, oldBr);
        DeleteObject(pen);

        wchar_t caption[128] = {0};
        GetWindowTextW(dis->hwndItem, caption, 128);
        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, text);
        SelectObject(dis->hDC, g_btnFont);
        DrawTextW(dis->hDC, caption, -1, &dis->rcItem,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT);
        return TRUE;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_bgBrush);

        /* Subtle grid pattern */
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(0x10, 0x20, 0x14));
        HGDIOBJ oldPen = SelectObject(dc, gridPen);
        for (int y = 0; y < rc.bottom; y += 28) {
            MoveToEx(dc, 0, y, NULL);
            LineTo(dc, rc.right, y);
        }
        for (int x = 0; x < rc.right; x += 28) {
            MoveToEx(dc, x, 0, NULL);
            LineTo(dc, x, rc.bottom);
        }
        SelectObject(dc, oldPen);
        DeleteObject(gridPen);

        /* Icon on the left */
        if (g_appIcon) {
            DrawIconEx(dc, 48, 48, g_appIcon, 128, 128, 0, NULL, DI_NORMAL);
        }

        SetBkMode(dc, TRANSPARENT);

        /* Title */
        SelectObject(dc, g_titleFont);
        SetTextColor(dc, kTextLight);
        RECT tr = { 200, 56, rc.right - 32, 110 };
        DrawTextW(dc, L"Dante CLI", -1, &tr, DT_SINGLELINE | DT_LEFT);

        /* Version + subtitle */
        SelectObject(dc, g_smallFont);
        SetTextColor(dc, kAccentGreen);
        RECT vr = { 200, 110, rc.right - 32, 132 };
        DrawTextW(dc, L"v" APP_VERSION L"   ·   Terminal nativo Windows", -1, &vr,
                  DT_SINGLELINE | DT_LEFT);

        /* Body */
        SelectObject(dc, g_bodyFont);
        SetTextColor(dc, kTextLight);
        RECT br = { 48, 200, rc.right - 48, 360 };
        const wchar_t* body =
            L"O instalador foi executado com sucesso. Este executável é o "
            L"bootstrap nativo Win32 que abre quando você clica no atalho.\n\n"
            L"A versão Qt 6 completa (com ConPTY, splits, sidebar, integração "
            L"AI e tudo mais descrito no roadmap) requer build Windows com "
            L"Visual Studio 2022 + Qt 6.5+. Veja o guia de instalação para "
            L"compilar e substituir este stub pelo binário completo.\n\n"
            L"Enquanto isso, você pode abrir o terminal padrão do Windows pelo "
            L"botão abaixo.";
        DrawTextW(dc, body, -1, &br, DT_LEFT | DT_WORDBREAK);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case ID_BTN_TERMINAL: launch_shell(hwnd); return 0;
        case ID_BTN_DOCS:     open_local_doc(hwnd); return 0;
        case ID_BTN_GITHUB:   open_url(hwnd, APP_URL); return 0;
        case ID_BTN_CLOSE:    DestroyWindow(hwnd); return 0;
        }
        break;
    }

    case WM_DESTROY:
        if (g_titleFont) DeleteObject(g_titleFont);
        if (g_bodyFont)  DeleteObject(g_bodyFont);
        if (g_smallFont) DeleteObject(g_smallFont);
        if (g_btnFont)   DeleteObject(g_btnFont);
        if (g_bgBrush)   DeleteObject(g_bgBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI
wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine;

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"DanteCLIBootstrap";
    wc.hIcon         = LoadIconW(hInstance, L"DanteIcon");
    wc.hIconSm       = LoadIconW(hInstance, L"DanteIcon");
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int w  = 560;
    int h  = 580;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"DanteCLIBootstrap",
        APP_NAME,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (sw - w) / 2, (sh - h) / 2,
        w, h,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
