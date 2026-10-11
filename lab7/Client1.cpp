// PipeClient1.cpp - віконний клієнт 1 (варіант 2)
//  - висота рядка заголовка вікна (SM_CYCAPTION)
//  - стан клавіатури (GetKeyboardType)
//  - висота екрану (GetDeviceCaps VERTRES)
// Меню: File -> Exit | Pipe -> Send data | Help -> About
#include <windows.h>
#include <wchar.h>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#define PIPE_NAME  L"\\\\.\\pipe\\lab7pipe"
#define CLIENT_NO  1

enum
{
    IDM_EXIT = 100,
    IDM_PIPE_SEND,
    IDM_ABOUT
};

static HWND g_hEdit = NULL;

// Збирає дані клієнта у текстовий рядок
static void BuildData(wchar_t* out, size_t n)
{
    int caption = GetSystemMetrics(SM_CYCAPTION);
    int kbType = GetKeyboardType(0);   // 0 - клавіатури немає

    HDC hdc = GetDC(NULL);
    int vres = GetDeviceCaps(hdc, VERTRES);
    ReleaseDC(NULL, hdc);

    swprintf_s(out, n,
        L"Дані Клієнта #%d:\r\n"
        L" - Висота рядка заголовка вікна: %d px\r\n"
        L" - Клавіатура: %s (тип %d)\r\n"
        L" - Висота екрану: %d px\r\n",
        CLIENT_NO, caption,
        kbType != 0 ? L"підключена" : L"відсутня", kbType,
        vres);
}

// Відкриває канал і надсилає повідомлення серверу
static void SendData(HWND hWnd)
{
    wchar_t text[1024];
    BuildData(text, _countof(text));
    SetWindowTextW(g_hEdit, text);

    HANDLE hPipe = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 5; i++)
    {
        hPipe = CreateFileW(PIPE_NAME, GENERIC_WRITE, 0, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hPipe != INVALID_HANDLE_VALUE)
            break;
        if (GetLastError() != ERROR_PIPE_BUSY)
            break;
        WaitNamedPipeW(PIPE_NAME, 2000);   // канал зайнятий - чекаємо
    }

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        wchar_t err[128];
        swprintf_s(err, L"Не вдалося підключитися до каналу (помилка %lu).\n"
            L"Перевірте, що сервер запущено і канал створено.",
            GetLastError());
        MessageBoxW(hWnd, err, L"PipeClientError", MB_OK);
        return;
    }

    DWORD written = 0;
    if (!WriteFile(hPipe, text, (DWORD)(wcslen(text) * sizeof(wchar_t)), &written, NULL))
    {
        wchar_t err[64];
        swprintf_s(err, L"Помилка відправки: %lu", GetLastError());
        MessageBoxW(hWnd, err, L"PipeClientError", MB_OK);
    }
    else
    {
        int len = GetWindowTextLengthW(g_hEdit);
        SendMessageW(g_hEdit, EM_SETSEL, len, len);
        SendMessageW(g_hEdit, EM_REPLACESEL, FALSE, (LPARAM)L"\r\nДані надіслано серверу.");
    }
    CloseHandle(hPipe);
}

static void BuildMenu(HWND hWnd)
{
    HMENU hBar = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    HMENU hPipe = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();

    AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenuW(hPipe, MF_STRING, IDM_PIPE_SEND, L"Send data");
    AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile, L"File");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hPipe, L"Pipe");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"Help");

    SetMenu(hWnd, hBar);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        BuildMenu(hWnd);
        g_hEdit = CreateWindowExW(
            0, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
            ES_AUTOVSCROLL | ES_READONLY,
            10, 10, 360, 200, hWnd, NULL,
            ((LPCREATESTRUCTW)lParam)->hInstance, NULL);
        break;

    case WM_SIZE:
        MoveWindow(g_hEdit, 10, 10, LOWORD(lParam) - 20, HIWORD(lParam) - 20, TRUE);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_PIPE_SEND: SendData(hWnd); break;
        case IDM_ABOUT:
            MessageBoxW(hWnd, L"PipeClient1\nЛабораторна робота: іменовані канали",
                L"About", MB_OK);
            break;
        case IDM_EXIT: DestroyWindow(hWnd); break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"PipeClient1Wnd";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowW(L"PipeClient1Wnd", L"PipeClient1", WS_OVERLAPPEDWINDOW,
        760, 380, 400, 280, NULL, NULL, hInstance, NULL);
    if (!hWnd)
        return 1;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}