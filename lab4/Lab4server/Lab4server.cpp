// Server (Win32 GUI): приймає дані від клієнтів і показує їх у вікні
// Меню: File | Help | Config | Clients
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#define PORT 27015
#define WM_APP_TEXT (WM_APP + 1)

enum { ID_FILE_CLEAR = 101, ID_FILE_EXIT, ID_HELP_ABOUT, ID_CFG_INFO, ID_CLI_COUNT };

HWND g_hwnd = nullptr, g_edit = nullptr;
SOCKET g_listen = INVALID_SOCKET;
std::atomic<int> g_clients{ 0 }, g_total{ 0 };

static std::wstring FromUtf8(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static void AppendText(const std::wstring& s)
{
    int len = GetWindowTextLengthW(g_edit);
    SendMessageW(g_edit, EM_SETSEL, len, len);
    SendMessageW(g_edit, EM_REPLACESEL, FALSE, (LPARAM)s.c_str());
}

// Потік обслуговування одного клієнта. Повідомлення завершується нульовим байтом.
static void ClientThread(SOCKET c)
{
    g_clients++; g_total++;
    std::string buf;
    char tmp[2048];
    int n;
    while ((n = recv(c, tmp, sizeof(tmp), 0)) > 0) {
        buf.append(tmp, n);
        size_t p;
        while ((p = buf.find('\0')) != std::string::npos) {
            std::string msg = buf.substr(0, p);
            buf.erase(0, p + 1);
            PostMessageW(g_hwnd, WM_APP_TEXT, 0,
                (LPARAM) new std::wstring(FromUtf8(msg) + L"\r\n\r\n"));
        }
    }
    closesocket(c);
    g_clients--;
}

static void AcceptThread()
{
    for (;;) {
        sockaddr_in ca; int len = sizeof(ca);
        SOCKET c = accept(g_listen, (sockaddr*)&ca, &len);
        if (c == INVALID_SOCKET) break;
        std::thread(ClientThread, c).detach();
    }
}

static bool StartServer()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);          // створення сокета
    if (g_listen == INVALID_SOCKET) return false;

    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(PORT);
    if (bind(g_listen, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) return false;
    if (listen(g_listen, SOMAXCONN) == SOCKET_ERROR) return false;

    std::thread(AcceptThread).detach();
    return true;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_hwnd = hwnd;
        g_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hwnd, (HMENU)1, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        if (!StartServer())
            MessageBoxW(hwnd, L"Не вдалося запустити сервер (порт зайнятий?)", L"Server", MB_ICONERROR);
        return 0;

    case WM_SIZE:
        MoveWindow(g_edit, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        return 0;

    case WM_APP_TEXT: {
        std::wstring* s = (std::wstring*)lp;
        AppendText(*s);
        delete s;
        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_FILE_CLEAR: SetWindowTextW(g_edit, L""); break;
        case ID_FILE_EXIT:  DestroyWindow(hwnd); break;
        case ID_HELP_ABOUT:
            MessageBoxW(hwnd, L"Лабораторна робота: клієнт-сервер, сокети (TCP)", L"Help", MB_OK);
            break;
        case ID_CFG_INFO: {
            wchar_t t[128];
            swprintf_s(t, L"Протокол: TCP\nПорт: %d\nСтан: очікування підключень", PORT);
            MessageBoxW(hwnd, t, L"Config", MB_OK);
            break;
        }
        case ID_CLI_COUNT: {
            wchar_t t[128];
            swprintf_s(t, L"Зараз підключено: %d\nВсього підключень: %d", g_clients.load(), g_total.load());
            MessageBoxW(hwnd, t, L"Clients", MB_OK);
            break;
        }
        }
        return 0;

    case WM_DESTROY:
        if (g_listen != INVALID_SOCKET) closesocket(g_listen);
        WSACleanup();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int show)
{
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"ServerWnd";
    RegisterClassW(&wc);

    HMENU bar = CreateMenu(), f = CreatePopupMenu(), h = CreatePopupMenu(),
        cfg = CreatePopupMenu(), cli = CreatePopupMenu();
    AppendMenuW(f, MF_STRING, ID_FILE_CLEAR, L"Очистити");
    AppendMenuW(f, MF_STRING, ID_FILE_EXIT, L"Вихід");
    AppendMenuW(h, MF_STRING, ID_HELP_ABOUT, L"Про програму");
    AppendMenuW(cfg, MF_STRING, ID_CFG_INFO, L"Параметри сервера");
    AppendMenuW(cli, MF_STRING, ID_CLI_COUNT, L"Кількість підключень");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)f, L"File");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)h, L"Help");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)cfg, L"Config");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)cli, L"Clients");

    HWND w = CreateWindowW(L"ServerWnd", L"Server", WS_OVERLAPPEDWINDOW,
        50, 50, 520, 420, nullptr, bar, hInst, nullptr);
    ShowWindow(w, show);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}