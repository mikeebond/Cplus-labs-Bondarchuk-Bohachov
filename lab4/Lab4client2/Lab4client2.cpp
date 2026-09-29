// Client 2 (Win32 GUI): menu button height (SM_CYBUTTON), Start button width (SM_CXMINTRACK), screen rows (VERTRES)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <cstdio>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define PORT 27015

std::wstring g_host = L"127.0.0.1";

static std::string ToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}


static bool SendToServer(const std::wstring& text, std::wstring& err)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { err = L"WSAStartup error"; return false; }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        err = L"socket error: " + std::to_wstring(WSAGetLastError());
        WSACleanup(); return false;
    }
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    if (InetPtonW(AF_INET, g_host.c_str(), &a.sin_addr) != 1) {
        err = L"Invalid server address"; closesocket(s); WSACleanup(); return false;
    }
    if (connect(s, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) {
        err = L"Cannot connect to server (code " + std::to_wstring(WSAGetLastError()) +
            L"). Is the Server running?";
        closesocket(s); WSACleanup(); return false;
    }
    std::string u = ToUtf8(text);
    bool ok = send(s, u.c_str(), (int)u.size() + 1, 0) != SOCKET_ERROR;
    if (!ok) err = L"send error: " + std::to_wstring(WSAGetLastError());
    shutdown(s, SD_SEND);
    closesocket(s);
    WSACleanup();
    return ok;
}

// SM_CYBUTTON is not defined in winuser.h; SM_CYSIZE is the closest equivalent
#ifndef SM_CYBUTTON
#define SM_CYBUTTON SM_CYSIZE
#endif

enum { ID_FILE_CLEAR = 101, ID_FILE_EXIT, ID_HELP_ABOUT, ID_SRV_SEND };
HWND g_edit = nullptr;

static std::wstring BuildBody()
{
    HDC hdc = GetDC(nullptr);
    int vert = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(nullptr, hdc);

    wchar_t b[512];
    swprintf_s(b,
        L"Menu button height (SM_CYBUTTON): %d\r\n"
        L"Start button width (SM_CXMINTRACK): %d px\r\n"
        L"Screen rows (VERTRES): %d",
        GetSystemMetrics(SM_CYBUTTON), GetSystemMetrics(SM_CXMINTRACK), vert);
    return b;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        return 0;
    case WM_SIZE:
        MoveWindow(g_edit, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_FILE_CLEAR: SetWindowTextW(g_edit, L""); break;
        case ID_FILE_EXIT:  DestroyWindow(hwnd); break;
        case ID_HELP_ABOUT: MessageBoxW(hwnd, L"Client 2: menu button height, Start button width, screen rows", L"Help", MB_OK); break;
        case ID_SRV_SEND: {
            std::wstring body = BuildBody(), err;
            if (SendToServer(L"Data from Client 2:\r\n" + body, err))
                SetWindowTextW(g_edit, (L"Data sent to server\r\n" + body).c_str());
            else
                MessageBoxW(hwnd, err.c_str(), L"Error", MB_ICONERROR);
            break;
        }
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmd, int show)
{
    if (cmd && cmd[0]) g_host = cmd;

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"Client2Wnd";
    RegisterClassW(&wc);

    HMENU bar = CreateMenu(), f = CreatePopupMenu(), h = CreatePopupMenu(), sv = CreatePopupMenu();
    AppendMenuW(f, MF_STRING, ID_FILE_CLEAR, L"Clear");
    AppendMenuW(f, MF_STRING, ID_FILE_EXIT, L"Exit");
    AppendMenuW(h, MF_STRING, ID_HELP_ABOUT, L"About");
    AppendMenuW(sv, MF_STRING, ID_SRV_SEND, L"Send data");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)f, L"File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)h, L"Help");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)sv, L"Server");

    HWND w = CreateWindowW(L"Client2Wnd", L"client2", WS_OVERLAPPEDWINDOW,
        600, 320, 420, 200, nullptr, bar, hInst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}