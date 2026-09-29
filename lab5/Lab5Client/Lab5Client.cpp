// ============================================================
//  Лабораторна робота: Клієнт-серверна архітектура. Mailslot
//  КЛІЄНТ (варіант 9). Один файл -> три програми:
//
//    cl /utf-8 /EHsc /DCLIENT_ID=1 client.cpp user32.lib gdi32.lib /Fe:client1.exe
//    cl /utf-8 /EHsc /DCLIENT_ID=2 client.cpp user32.lib gdi32.lib /Fe:client2.exe
//    cl /utf-8 /EHsc /DCLIENT_ID=3 client.cpp user32.lib gdi32.lib /Fe:client3.exe
//
//  Або зібрати один client.exe і запускати з аргументом: client.exe 2
//
//  Клієнт 1: SM_CLEANBOOT, час системи, SM_CXMIN, HORZRES
//  Клієнт 2: SM_CYMENUSIZE, SM_CXMINTRACK, VERTRES
//  Клієнт 3 (із зірочкою): площа прямокутника за введеними a, b
// ============================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <vector>
#include <cwchar>
#include <cstdlib>

#ifndef CLIENT_ID
#define CLIENT_ID 1
#endif

#define ID_SEND      32771   // меню: Надіслати
#define ID_REFRESH   32772   // меню: Оновити дані
#define IDC_EDIT     100
#define IDC_EDIT_A   101
#define IDC_EDIT_B   102
#define IDC_BTN_CALC 103
#define IDC_STATUS   104
#define IDC_LBL_A    105
#define IDC_LBL_B    106

static const wchar_t* ServerName = L"\\\\.\\mailslot\\lab_mailslot";

static int  g_clientId = CLIENT_ID;
static HWND hwndEdit, hwndStatus, hwndA, hwndB, hwndBtn, hwndLblA, hwndLblB;

static const int TOP_H = 34;   // панель введення (лише клієнт 3)
static const int STATUS_H = 22;

static void SetStatus(const wchar_t* s) { SetWindowTextW(hwndStatus, s); }

// Прочитати число з поля (допускає кому як роздільник)
static bool ReadDouble(HWND h, double& out)
{
    wchar_t buf[64];
    GetWindowTextW(h, buf, 64);
    for (wchar_t* p = buf; *p; ++p) if (*p == L',') *p = L'.';
    wchar_t* end = NULL;
    out = wcstod(buf, &end);
    if (end == buf) return false;
    while (*end == L' ') ++end;
    return *end == L'\0';
}

// Формування повідомлення за замовчуванням
static void BuildDefaultMessage(HWND hWnd)
{
    wchar_t mess[1024] = L"";

    if (g_clientId == 1)
    {
        HDC hdc = GetDC(hWnd);
        SYSTEMTIME st;
        GetLocalTime(&st);
        int cleanboot = GetSystemMetrics(SM_CLEANBOOT);
        int cxmin = GetSystemMetrics(SM_CXMIN);
        int horzres = GetDeviceCaps(hdc, HORZRES);
        ReleaseDC(hWnd, hdc);

        swprintf_s(mess,
            L"Дані Клієнта #1:\r\n"
            L" - час системи = %02d:%02d:%02d\r\n"
            L" - режим завантаження (SM_CLEANBOOT) = %d\r\n"
            L" - стан антивірусу (SM_CXMIN) = %d\r\n"
            L" - ширина екрану (HORZRES) = %d",
            st.wHour, st.wMinute, st.wSecond, cleanboot, cxmin, horzres);
    }
    else if (g_clientId == 2)
    {
        HDC hdc = GetDC(hWnd);
        int cybutton = GetSystemMetrics(SM_CYMENUSIZE);
        int cxmintrack = GetSystemMetrics(SM_CXMINTRACK);
        int vertres = GetDeviceCaps(hdc, VERTRES);
        ReleaseDC(hWnd, hdc);

        swprintf_s(mess,
            L"Дані Клієнта #2:\r\n"
            L" - висота кнопок у меню (SM_CYMENUSIZE) = %d\r\n"
            L" - ширина кнопки \"Пуск\" (SM_CXMINTRACK) = %d\r\n"
            L" - кількість рядків екрану (VERTRES) = %d",
            cybutton, cxmintrack, vertres);
    }
    else
    {
        double a, b;
        if (!ReadDouble(hwndA, a) || !ReadDouble(hwndB, b))
        {
            SetStatus(L"Введіть коректні числа a і b.");
            return;
        }
        double S = a * b;
        double P = 2.0 * (a + b);
        swprintf_s(mess,
            L"Дані Клієнта #3:\r\n"
            L" - сторона a = %.2f\r\n"
            L" - сторона b = %.2f\r\n"
            L" - площа прямокутника S = %.2f\r\n"
            L" - периметр прямокутника P = %.2f",
            a, b, S, P);
    }
    SetWindowTextW(hwndEdit, mess);
    SetStatus(L"Повідомлення сформовано.");
}

// Відправлення вмісту текстового поля на сервер
static void SendToServer()
{
    int len = GetWindowTextLengthW(hwndEdit);
    std::vector<wchar_t> buf(len + 1, 0);
    GetWindowTextW(hwndEdit, buf.data(), len + 1);

    // Підключення до поштового сервера
    HANDLE h = CreateFileW(ServerName, GENERIC_WRITE, FILE_SHARE_READ,
        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
    {
        wchar_t err[128];
        swprintf_s(err, L"Помилка поштового серверу: %lu (сервер не запущено?)", GetLastError());
        SetStatus(err);
        return;
    }

    // Запис на поштовий сервер (разом з завершальним нулем)
    DWORD cbWritten = 0;
    BOOL ok = WriteFile(h, buf.data(), (DWORD)((len + 1) * sizeof(wchar_t)), &cbWritten, NULL);
    DWORD e = GetLastError();
    CloseHandle(h);

    if (ok)
    {
        wchar_t s[128];
        swprintf_s(s, L"Надіслано байтів: %lu", cbWritten);
        SetStatus(s);
    }
    else
    {
        wchar_t s[128];
        swprintf_s(s, L"Помилка запису: %lu", e);
        SetStatus(s);
    }
}

static void Layout(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    int w = rc.right, h = rc.bottom;
    int top = (g_clientId == 3) ? TOP_H : 0;

    if (g_clientId == 3)
    {
        MoveWindow(hwndLblA, 8, 8, 20, 20, TRUE);
        MoveWindow(hwndA, 30, 5, 80, 24, TRUE);
        MoveWindow(hwndLblB, 125, 8, 20, 20, TRUE);
        MoveWindow(hwndB, 147, 5, 80, 24, TRUE);
        MoveWindow(hwndBtn, 240, 4, 110, 26, TRUE);
    }
    MoveWindow(hwndEdit, 0, top, w, h - top - STATUS_H, TRUE);
    MoveWindow(hwndStatus, 4, h - STATUS_H + 2, w - 8, STATUS_H - 2, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        HINSTANCE hi = GetModuleHandleW(NULL);
        HGDIOBJ font = GetStockObject(DEFAULT_GUI_FONT);

        hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT, hi, NULL);
        hwndStatus = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATUS, hi, NULL);

        SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)font, TRUE);
        SendMessageW(hwndStatus, WM_SETFONT, (WPARAM)font, TRUE);

        if (g_clientId == 3)
        {
            hwndLblA = CreateWindowW(L"STATIC", L"a:", WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)IDC_LBL_A, hi, NULL);
            hwndA = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"12.5",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_A, hi, NULL);
            hwndLblB = CreateWindowW(L"STATIC", L"b:", WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hWnd, (HMENU)IDC_LBL_B, hi, NULL);
            hwndB = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"8.4",
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_B, hi, NULL);
            hwndBtn = CreateWindowW(L"BUTTON", L"Обчислити",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_CALC, hi, NULL);
            SendMessageW(hwndLblA, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(hwndLblB, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(hwndA, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(hwndB, WM_SETFONT, (WPARAM)font, TRUE);
            SendMessageW(hwndBtn, WM_SETFONT, (WPARAM)font, TRUE);
        }
        BuildDefaultMessage(hWnd);
        return 0;
    }

    case WM_SIZE:
        Layout(hWnd);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_SEND:      SendToServer();              break;
        case ID_REFRESH:   BuildDefaultMessage(hWnd);   break;
        case IDC_BTN_CALC: BuildDefaultMessage(hWnd);   break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int nCmdShow)
{
    // Необов'язково: номер клієнта з командного рядка (client.exe 2)
    if (lpCmdLine && lpCmdLine[0] >= L'1' && lpCmdLine[0] <= L'3')
        g_clientId = lpCmdLine[0] - L'0';

    const wchar_t* cls = L"MailslotClientClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    // Меню
    HMENU hMenu = CreateMenu();
    AppendMenuW(hMenu, MF_STRING, ID_SEND, L"Надіслати");
    AppendMenuW(hMenu, MF_STRING, ID_REFRESH, L"Оновити дані");

    wchar_t title[64];
    swprintf_s(title, L"Клієнт №%d", g_clientId);

    HWND hWnd = CreateWindowW(cls, title, WS_OVERLAPPEDWINDOW,
        600 + (g_clientId - 1) * 30, 50 + (g_clientId - 1) * 200,
        440, 260, NULL, hMenu, hInst, NULL);
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0))
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}