// ============================================================
//  Лабораторна робота: Клієнт-серверна архітектура. Mailslot
//  СЕРВЕР (Win32 API, Unicode)
//
//  Збірка (Developer Command Prompt for VS):
//    cl /utf-8 /EHsc server.cpp user32.lib gdi32.lib /Fe:server.exe
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

#define ID_MAILSLOT_CREATE 32771
#define ID_MAILSLOT_CLOSE  32772
#define IDT_TIMER          1
#define IDC_EDIT           100

static const wchar_t* MailslotName = L"\\\\.\\mailslot\\lab_mailslot";

static HANDLE hMailslot = INVALID_HANDLE_VALUE;
static HWND   hwndEdit = NULL;

// Додати текст у кінець текстового поля
static void AppendText(const wchar_t* text)
{
    int len = GetWindowTextLengthW(hwndEdit);
    SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)text);
}

static void CloseServer()
{
    if (hMailslot != INVALID_HANDLE_VALUE)
    {
        KillTimer(GetParent(hwndEdit), IDT_TIMER);
        CloseHandle(hMailslot);
        hMailslot = INVALID_HANDLE_VALUE;
    }
}

// Прочитати всі повідомлення, що є у скриньці
static void ReadMailslot()
{
    if (hMailslot == INVALID_HANDLE_VALUE) return;

    DWORD cbNext = 0, cMessage = 0;
    while (GetMailslotInfo(hMailslot, NULL, &cbNext, &cMessage, NULL)
        && cMessage != 0 && cbNext != MAILSLOT_NO_MESSAGE)
    {
        std::vector<wchar_t> buf(cbNext / sizeof(wchar_t) + 2, 0);
        DWORD cbRead = 0;
        if (!ReadFile(hMailslot, buf.data(), cbNext, &cbRead, NULL))
        {
            wchar_t err[128];
            swprintf_s(err, L"\r\nПомилка читання. Error: %lu\r\n", GetLastError());
            AppendText(err);
            break;
        }
        buf[cbRead / sizeof(wchar_t)] = L'\0';

        AppendText(L"\r\n--- Нове повідомлення ---\r\n");
        AppendText(buf.data());
        AppendText(L"\r\n");
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT, GetModuleHandleW(NULL), NULL);
        SendMessageW(hwndEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        SetWindowTextW(hwndEdit, L"Поштовий сервер. Оберіть: Поштова скриня -> Створити.\r\n");
        return 0;

    case WM_SIZE:
        MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_MAILSLOT_CREATE:
        {
            if (hMailslot != INVALID_HANDLE_VALUE)
            {
                AppendText(L"\r\nСервер уже запущено.\r\n");
                break;
            }
            SetWindowTextW(hwndEdit, L"Поштова скриня\r\n");

            // Тайм-аут читання 0: ReadFile не блокує інтерфейс
            hMailslot = CreateMailslotW(MailslotName, 0, 0, NULL);
            if (hMailslot == INVALID_HANDLE_VALUE)
            {
                wchar_t err[128];
                swprintf_s(err, L"\r\nПоштовий сервер. Error: %lu\r\n", GetLastError());
                AppendText(err);
                break;
            }
            AppendText(L"\r\nСервер запущено\r\n");
            SetTimer(hWnd, IDT_TIMER, 1000, NULL);   // період 1 с
            break;
        }
        case ID_MAILSLOT_CLOSE:
            if (hMailslot == INVALID_HANDLE_VALUE)
            {
                AppendText(L"\r\nСервер не запущено.\r\n");
                break;
            }
            CloseServer();
            AppendText(L"\r\nПоштовий сервер закрито\r\n");
            break;
        }
        return 0;

    case WM_TIMER:
        if (wParam == IDT_TIMER)
            ReadMailslot();
        return 0;

    case WM_DESTROY:
        CloseServer();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow)
{
    const wchar_t* cls = L"MailslotServerClass";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = cls;
    RegisterClassW(&wc);

    // Головне меню
    HMENU hMenu = CreateMenu();
    HMENU hPopup = CreatePopupMenu();
    AppendMenuW(hPopup, MF_STRING, ID_MAILSLOT_CREATE, L"Створити");
    AppendMenuW(hPopup, MF_STRING, ID_MAILSLOT_CLOSE, L"Закрити");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hPopup, L"Поштова скриня");

    HWND hWnd = CreateWindowW(cls, L"Поштовий сервер",
        WS_OVERLAPPEDWINDOW, 50, 50, 520, 480, NULL, hMenu, hInst, NULL);
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