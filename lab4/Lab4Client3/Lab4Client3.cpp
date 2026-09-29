// Client 3 (Win32 GUI): sends arbitrary text to the server
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

enum { ID_FILE_CLEAR = 101, ID_FILE_EXIT, ID_HELP_ABOUT, ID_SRV_SEND, ID_BTN_SEND };
HWND g_in = nullptr, g_out = nullptr, g_btn = nullptr;

static void DoSend(HWND hwnd)
{
    int len = GetWindowTextLengthW(g_in);
    if (len == 0) { MessageBoxW(hwnd, L"Enter text to send", L"client3", MB_ICONINFORMATION); return; }
    std::wstring text(len + 1, 0);
    GetWindowTextW(g_in, &text[0], len + 1);
    text.resize(len);

    std::wstring err;
    if (SendToServer(L"Data from Client 3:\r\n" + text, err)) {
        int n = GetWindowTextLengthW(g_out);
        SendMessageW(g_out, EM_SETSEL, n, n);
        std::wstring line = L"Data sent to server\r\n" + text + L"\r\n\r\n";
        SendMessageW(g_out, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
        SetWindowTextW(g_in, L"");
    }
    else {
        MessageBoxW(hwnd, err.c_str(), L"Error", MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        HINSTANCE hi = GetModuleHandleW(nullptr);
        HGDIOBJ font = GetStockObject(DEFAULT_GUI_FONT);
        g_in = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)2, hi, nullptr);
        g_btn = CreateWindowW(L"BUTTON", L"Send", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            0, 0, 0, 0, hwnd, (HMENU)ID_BTN_SEND, hi, nullptr);
        g_out = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)3, hi, nullptr);
        SendMessageW(g_in, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_btn, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(g_out, WM_SETFONT, (WPARAM)font, TRUE);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(g_in, 5, 5, w - 115, 24, TRUE);
        MoveWindow(g_btn, w - 105, 5, 100, 24, TRUE);
        MoveWindow(g_out, 0, 35, w, h - 35, TRUE);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_FILE_CLEAR: SetWindowTextW(g_out, L""); break;
        case ID_FILE_EXIT:  DestroyWindow(hwnd); break;
        case ID_HELP_ABOUT: MessageBoxW(hwnd, L"Client 3: sends arbitrary text to the server", L"Help", MB_OK); break;
        case ID_SRV_SEND:
        case ID_BTN_SEND:   DoSend(hwnd); break;
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
    wc.lpszClassName = L"Client3Wnd";
    RegisterClassW(&wc);

    HMENU bar = CreateMenu(), f = CreatePopupMenu(), h = CreatePopupMenu(), sv = CreatePopupMenu();
    AppendMenuW(f, MF_STRING, ID_FILE_CLEAR, L"Clear");
    AppendMenuW(f, MF_STRING, ID_FILE_EXIT, L"Exit");
    AppendMenuW(h, MF_STRING, ID_HELP_ABOUT, L"About");
    AppendMenuW(sv, MF_STRING, ID_SRV_SEND, L"Send text");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)f, L"File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)h, L"Help");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)sv, L"Server");

    HWND w = CreateWindowW(L"Client3Wnd", L"client3", WS_OVERLAPPEDWINDOW,
        50, 500, 520, 240, nullptr, bar, hInst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}