// PipeServer.cpp - віконний сервер на іменованих каналах (Win32, Unicode)
// Меню: File -> Exit | ManagePipe -> Create pipe / Close pipe | Help -> About
#include <windows.h>
#include <wchar.h>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

#define PIPE_NAME   L"\\\\.\\pipe\\lab7pipe"
#define BUF_BYTES   4096
#define WM_APP_TEXT (WM_APP + 1)   // lParam = wchar_t*, створений через new[]

enum
{
    IDM_EXIT = 100,
    IDM_PIPE_CREATE,
    IDM_PIPE_CLOSE,
    IDM_ABOUT
};

static HWND   g_hEdit = NULL;
static HANDLE g_hPipe = INVALID_HANDLE_VALUE;
static HANDLE g_hThread = NULL;
static HANDLE g_hStop = NULL;   // подія зупинки потоку читання

// Додає текст у кінець поля виводу
static void AppendText(const wchar_t* s)
{
    int len = GetWindowTextLengthW(g_hEdit);
    SendMessageW(g_hEdit, EM_SETSEL, len, len);
    SendMessageW(g_hEdit, EM_REPLACESEL, FALSE, (LPARAM)s);
}

// Безпечна передача тексту з робочого потоку у потік інтерфейсу
static void PostText(HWND hWnd, const wchar_t* s)
{
    size_t n = wcslen(s) + 1;
    wchar_t* copy = new wchar_t[n];
    wmemcpy(copy, s, n);
    PostMessageW(hWnd, WM_APP_TEXT, 0, (LPARAM)copy);
}

// Очікування завершення асинхронної операції або сигналу зупинки.
// Повертає: 1 - операція завершена, 0 - запит на зупинку, -1 - помилка
static int WaitOverlapped(OVERLAPPED& ov, DWORD& bytes)
{
    HANDLE ev[2] = { g_hStop, ov.hEvent };
    if (WaitForMultipleObjects(2, ev, FALSE, INFINITE) == WAIT_OBJECT_0)
        return 0;
    if (!GetOverlappedResult(g_hPipe, &ov, &bytes, FALSE))
    {
        if (GetLastError() == ERROR_MORE_DATA)
            return 1;
        return -1;
    }
    return 1;
}

// Потік: очікує клієнта, читає повідомлення, відключає клієнта, і так по колу
static DWORD WINAPI ReaderThread(LPVOID lpParam)
{
    HWND hWnd = (HWND)lpParam;
    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    wchar_t buf[BUF_BYTES / sizeof(wchar_t)];

    while (WaitForSingleObject(g_hStop, 0) != WAIT_OBJECT_0)
    {
        // 1. Очікування підключення клієнта
        ResetEvent(ov.hEvent);
        DWORD bytes = 0;
        if (!ConnectNamedPipe(g_hPipe, &ov))
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
            {
                int w = WaitOverlapped(ov, bytes);
                if (w == 0) break;
                if (w < 0) { DisconnectNamedPipe(g_hPipe); continue; }
            }
            else if (err != ERROR_PIPE_CONNECTED)
            {
                break;
            }
        }
        PostText(hWnd, L"Встановлено з'єднання з клієнтом\r\n");

        // 2. Читання повідомлення
        ResetEvent(ov.hEvent);
        bytes = 0;
        int w = 1;
        if (ReadFile(g_hPipe, buf, BUF_BYTES - sizeof(wchar_t), &bytes, &ov))
        {
            GetOverlappedResult(g_hPipe, &ov, &bytes, FALSE);
        }
        else
        {
            DWORD err = GetLastError();
            if (err == ERROR_IO_PENDING)
                w = WaitOverlapped(ov, bytes);
            else if (err != ERROR_MORE_DATA)
                w = -1;
        }
        if (w == 0) break;

        if (w > 0)
        {
            buf[bytes / sizeof(wchar_t)] = L'\0';
            PostText(hWnd, buf);
            PostText(hWnd, L"\r\n");
        }

        // 3. Відключаємо клієнта, канал готовий до наступного
        DisconnectNamedPipe(g_hPipe);
    }

    CloseHandle(ov.hEvent);
    return 0;
}

static void CreatePipe(HWND hWnd)
{
    if (g_hPipe != INVALID_HANDLE_VALUE)
    {
        MessageBoxW(hWnd, L"Канал уже створено", L"NamedPipe", MB_OK);
        return;
    }

    g_hStop = CreateEventW(NULL, TRUE, FALSE, NULL);

    g_hPipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        BUF_BYTES, BUF_BYTES, 5000, NULL);

    if (g_hPipe == INVALID_HANDLE_VALUE)
    {
        MessageBoxW(hWnd, L"Неможливо створити канал", L"NamedPipeError", MB_OK);
        CloseHandle(g_hStop);
        g_hStop = NULL;
        return;
    }

    AppendText(L"Канал створено\r\nОчікуємо на підключення клієнтів...\r\n");

    g_hThread = CreateThread(NULL, 0, ReaderThread, hWnd, 0, NULL);
    if (!g_hThread)
    {
        MessageBoxW(hWnd, L"Не вдалося створити потік читання", L"NamedPipeError", MB_OK);
        CloseHandle(g_hPipe);
        g_hPipe = INVALID_HANDLE_VALUE;
        CloseHandle(g_hStop);
        g_hStop = NULL;
    }
}

static void ClosePipe()
{
    if (g_hPipe == INVALID_HANDLE_VALUE)
        return;

    SetEvent(g_hStop);
    CancelIoEx(g_hPipe, NULL);

    if (g_hThread)
    {
        WaitForSingleObject(g_hThread, INFINITE);
        CloseHandle(g_hThread);
        g_hThread = NULL;
    }

    DisconnectNamedPipe(g_hPipe);
    CloseHandle(g_hPipe);
    g_hPipe = INVALID_HANDLE_VALUE;

    CloseHandle(g_hStop);
    g_hStop = NULL;

    AppendText(L"Канал закрито\r\n");
}

static void BuildMenu(HWND hWnd)
{
    HMENU hBar = CreateMenu();
    HMENU hFile = CreatePopupMenu();
    HMENU hManage = CreatePopupMenu();
    HMENU hHelp = CreatePopupMenu();

    AppendMenuW(hFile, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenuW(hManage, MF_STRING, IDM_PIPE_CREATE, L"Create pipe");
    AppendMenuW(hManage, MF_STRING, IDM_PIPE_CLOSE, L"Close pipe");
    AppendMenuW(hHelp, MF_STRING, IDM_ABOUT, L"About");

    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hFile, L"File");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hManage, L"ManagePipe");
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
            10, 10, 400, 300, hWnd, NULL,
            ((LPCREATESTRUCTW)lParam)->hInstance, NULL);
        SendMessageW(g_hEdit, EM_SETLIMITTEXT, 1000000, 0);
        break;

    case WM_SIZE:
        MoveWindow(g_hEdit, 10, 10, LOWORD(lParam) - 20, HIWORD(lParam) - 20, TRUE);
        break;

    case WM_APP_TEXT:
    {
        wchar_t* text = (wchar_t*)lParam;
        AppendText(text);
        delete[] text;
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_PIPE_CREATE: CreatePipe(hWnd); break;
        case IDM_PIPE_CLOSE:  ClosePipe();      break;
        case IDM_ABOUT:
            MessageBoxW(hWnd, L"PipeServer\nЛабораторна робота: іменовані канали",
                L"About", MB_OK);
            break;
        case IDM_EXIT: DestroyWindow(hWnd); break;
        }
        break;

    case WM_DESTROY:
        ClosePipe();
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
    wc.lpszClassName = L"PipeServerWnd";
    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowW(L"PipeServerWnd", L"PipeServer", WS_OVERLAPPEDWINDOW,
        320, 150, 430, 375, NULL, NULL, hInstance, NULL);
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