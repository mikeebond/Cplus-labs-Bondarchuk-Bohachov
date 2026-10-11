// server.c - додаток-сервер: створює filemapping, приймає дані від двох клієнтів.
// Завдання "з зірочкою": окремий потік чекає на подію (event) і автоматично
// виводить на екран нові дані, щойно клієнт записав їх у проекцію.
//
// Збирання (MSVC):  cl /utf-8 /DUNICODE /D_UNICODE server.c user32.lib
// Збирання (MinGW): gcc -municode -mwindows -finput-charset=UTF-8 server.c -o server.exe
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "common.h"

#define IDM_START   32771   // ID_FILEMAPPING_START
#define IDM_STOP    32772   // ID_FILEMAPPING_STOP
#define IDM_READ    32773   // ID_FILEMAPPING_READ (примусове читання)
#define IDM_EXIT    32774
#define ID_EDIT     100
#define WM_FM_CHANGED (WM_APP + 1)   // потік-спостерігач -> вікно: "дані змінилися"

static HWND   hwndEdit;
static HANDLE hMapFile, hMutex, hEventData, hEventReady, hEventStop, hThread;
static BOOL   running = FALSE;

// ---------- допоміжні функції ----------

static void AppendLog(const wchar_t* s)
{
    int len = GetWindowTextLengthW(hwndEdit);
    SendMessageW(hwndEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(hwndEdit, EM_REPLACESEL, FALSE, (LPARAM)s);
}

static void AppendLogf(const wchar_t* fmt, ...)
{
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    AppendLog(buf);
}

// Читає проекцію під захистом м'ютекса й виводить вміст.
// byEvent == TRUE  - виклик від потоку-спостерігача (автоматичне виявлення змін)
// byEvent == FALSE - примусове читання через меню
static void ReadMapping(HWND hWnd, BOOL byEvent)
{
    SHARED_DATA local;
    SYSTEMTIME st;
    LPVOID pBuf;
    DWORD w;

    if (!running)
    {
        if (!byEvent)
            MessageBoxW(hWnd, L"Сервер FileMapping не запущено", L"Map", MB_OK);
        return;
    }

    // взаємовиключний доступ: поки читаємо, клієнти не пишуть
    w = WaitForSingleObject(hMutex, 3000);
    if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED)
    {
        AppendLog(L"Не вдалося захопити м'ютекс\r\n");
        return;
    }

    pBuf = MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (pBuf == NULL)
    {
        ReleaseMutex(hMutex);
        MessageBoxW(hWnd, L"Подання проектованого файлу неможливе", L"Map", MB_OK);
        return;
    }
    CopyMemory(&local, pBuf, sizeof(local));   // швидко копіюємо й одразу відпускаємо
    UnmapViewOfFile(pBuf);
    ReleaseMutex(hMutex);

    local.text[FM_TEXT_CHARS - 1] = L'\0';
    GetLocalTime(&st);

    if (local.seq == 0)
    {
        AppendLogf(L"[%02d:%02d:%02d] Проекція порожня - клієнти ще нічого не надсилали\r\n",
            st.wHour, st.wMinute, st.wSecond);
    }
    else
    {
        AppendLogf(L"[%02d:%02d:%02d] %s (запис №%lu, клієнт %lu)\r\n%s\r\n\r\n",
            st.wHour, st.wMinute, st.wSecond,
            byEvent ? L"Автоматично виявлено зміни" : L"Примусове читання",
            local.seq, local.clientNo, local.text);
    }

    // повідомляємо клієнтам, що буфер прочитано й його можна перезаписувати
    if (byEvent)
        SetEvent(hEventReady);
}

// Потік-спостерігач: чекає на подію "нові дані" або на подію зупинки.
static DWORD WINAPI WatcherThread(LPVOID param)
{
    HWND hWnd = (HWND)param;
    HANDLE h[2] = { hEventStop, hEventData };
    for (;;)
    {
        DWORD r = WaitForMultipleObjects(2, h, FALSE, INFINITE);
        if (r == WAIT_OBJECT_0 + 1)
            PostMessageW(hWnd, WM_FM_CHANGED, 0, 0);   // читання робить потік інтерфейсу
        else
            break;                                      // зупинка (або помилка)
    }
    return 0;
}

static void StartServer(HWND hWnd)
{
    if (running) return;

    // створення об'єкта відображення в пам'яті (без фізичного файлу)
    hMapFile = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        0, sizeof(SHARED_DATA), FM_NAME);
    if (hMapFile == NULL)
    {
        MessageBoxW(hWnd, L"Не можливо створити відображений у пам'яті об'єкт", L"Map", MB_OK);
        return;
    }
    hMutex = CreateMutexW(NULL, FALSE, FM_MUTEX);
    hEventData = CreateEventW(NULL, FALSE, FALSE, FM_EVENT_DATA);   // авто-скидання
    hEventReady = CreateEventW(NULL, FALSE, TRUE, FM_EVENT_READY);  // спочатку "вільно"
    hEventStop = CreateEventW(NULL, TRUE, FALSE, NULL);            // ручне скидання
    hThread = CreateThread(NULL, 0, WatcherThread, hWnd, 0, NULL);

    running = TRUE;
    SetWindowTextW(hwndEdit, L"Сервер FileMapping запущено\r\n");
}

static void StopServer(void)
{
    if (!running) return;

    SetEvent(hEventStop);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    CloseHandle(hEventStop);
    CloseHandle(hEventReady);
    CloseHandle(hEventData);
    CloseHandle(hMutex);
    CloseHandle(hMapFile);     // закриття filemapping
    running = FALSE;
}

// ---------- вікно ----------

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Сервер FileMapping зупинено\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, 0, 0, 0, hWnd, (HMENU)ID_EDIT, GetModuleHandleW(NULL), NULL);
        return 0;
    }
    case WM_SIZE:
        MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_START:
            StartServer(hWnd);
            break;
        case IDM_STOP:
            StopServer();
            SetWindowTextW(hwndEdit, L"Сервер FileMapping зупинено\r\n");
            break;
        case IDM_READ:
            ReadMapping(hWnd, FALSE);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        }
        return 0;

    case WM_FM_CHANGED:               // сигнал від потоку-спостерігача
        ReadMapping(hWnd, TRUE);
        return 0;

    case WM_DESTROY:
        StopServer();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nShow)
{
    WNDCLASSW wc = { 0 };
    HMENU hBar, hSub;
    MSG m;
    HWND hWnd;

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FmServerWnd";
    RegisterClassW(&wc);

    hBar = CreateMenu();
    hSub = CreatePopupMenu();
    AppendMenuW(hSub, MF_STRING, IDM_START, L"Запустити сервер");
    AppendMenuW(hSub, MF_STRING, IDM_STOP, L"Зупинити сервер");
    AppendMenuW(hSub, MF_STRING, IDM_READ, L"Прочитати проекцію (примусово)");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hSub, L"FileMapping");
    AppendMenuW(hBar, MF_STRING, IDM_EXIT, L"Вихід");

    hWnd = CreateWindowW(L"FmServerWnd", L"Сервер FileMapping", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 420, NULL, hBar, hInst, NULL);
    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    while (GetMessageW(&m, NULL, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}