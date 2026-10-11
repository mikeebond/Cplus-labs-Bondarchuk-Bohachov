// client.c - додаток-клієнт. Один вихідний файл для обох клієнтів (варіант 7):
//   клієнт №1:  cl /utf-8 /DUNICODE /D_UNICODE /DCLIENT_NO=1 client.c user32.lib gdi32.lib /Feclient1.exe
//   клієнт №2:  cl /utf-8 /DUNICODE /D_UNICODE /DCLIENT_NO=2 client.c user32.lib gdi32.lib /Feclient2.exe
// MinGW: gcc -municode -mwindows -finput-charset=UTF-8 -DCLIENT_NO=1 client.c -o client1.exe -lgdi32
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include "common.h"

#ifndef CLIENT_NO
#define CLIENT_NO 1
#endif

#define IDM_SEND 32771
#define ID_EDIT  100

static HWND hwndEdit;

// Формує дефолтне повідомлення із системних метрик (варіант 7)
static void BuildDefaultMessage(HWND hWnd, wchar_t* out, size_t n)
{
    HDC hdc = GetDC(hWnd);
#if CLIENT_NO == 1
    _snwprintf_s(out, n, _TRUNCATE,
        L"Дані від Клієнта №1:\r\n"
        L"Стан завантаження системи (SM_CLEANBOOT) = %d\r\n"
        L"Стан антивірусу (SM_CXMIN) = %d\r\n"
        L"Ширина екрану (HORZRES) = %d",
        GetSystemMetrics(SM_CLEANBOOT),
        GetSystemMetrics(SM_CXMIN),
        GetDeviceCaps(hdc, HORZRES));
#else
    _snwprintf_s(out, n, _TRUNCATE,
        L"Дані від Клієнта №2:\r\n"
        L"Висота кнопок у меню (SM_CYMENUSIZE) = %d\r\n"
        L"Ширина кнопки «Пуск» (SM_CXMINTRACK) = %d\r\n"
        L"Кількість рядків екрану (VERTRES) = %d",
        GetSystemMetrics(SM_CYMENUSIZE),
        GetSystemMetrics(SM_CXMINTRACK),
        GetDeviceCaps(hdc, VERTRES));
#endif
    ReleaseDC(hWnd, hdc);
}

// Відправка тексту з поля вводу на сервер через filemapping
static void SendToServer(HWND hWnd)
{
    wchar_t text[FM_TEXT_CHARS];
    HANDLE hMapFile, hMutex, hEventData, hEventReady;
    SHARED_DATA* p;
    DWORD w;

    // підготовка повідомлення
    GetWindowTextW(hwndEdit, text, FM_TEXT_CHARS);

    // підготовка проекції
    hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FM_NAME);
    if (hMapFile == NULL)
    {
        MessageBoxW(hWnd, L"Не можливо відкрити відображений у пам'яті об'єкт.\nСервер запущено?",
            L"Map", MB_OK);
        return;
    }
    hMutex = OpenMutexW(SYNCHRONIZE, FALSE, FM_MUTEX);
    hEventData = OpenEventW(EVENT_MODIFY_STATE, FALSE, FM_EVENT_DATA);
    hEventReady = OpenEventW(SYNCHRONIZE, FALSE, FM_EVENT_READY);
    if (!hMutex || !hEventData || !hEventReady)
    {
        MessageBoxW(hWnd, L"Не вдалося відкрити об'єкти синхронізації", L"Map", MB_OK);
        goto cleanup;
    }

    // чекаємо, поки сервер прочитає попереднє повідомлення (щоб нічого не загубити)
    if (WaitForSingleObject(hEventReady, 3000) != WAIT_OBJECT_0)
    {
        MessageBoxW(hWnd, L"Сервер ще не прочитав попереднє повідомлення", L"Map", MB_OK);
        goto cleanup;
    }

    // взаємовиключний доступ до буфера
    w = WaitForSingleObject(hMutex, 3000);
    if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED)
    {
        SetEvent(hEventReady);     // повертаємо "токен", бо нічого не записали
        MessageBoxW(hWnd, L"Не вдалося захопити м'ютекс", L"Map", MB_OK);
        goto cleanup;
    }

    p = (SHARED_DATA*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_DATA));
    if (p == NULL)
    {
        ReleaseMutex(hMutex);
        SetEvent(hEventReady);
        MessageBoxW(hWnd, L"Подання проектованого файлу неможливе", L"Map", MB_OK);
        goto cleanup;
    }

    // безпосередня проекція (запис у спільну пам'ять)
    p->clientNo = CLIENT_NO;
    wcsncpy_s(p->text, FM_TEXT_CHARS, text, _TRUNCATE);
    p->seq++;
    UnmapViewOfFile(p);
    ReleaseMutex(hMutex);

    SetEvent(hEventData);          // будимо сервер: "у буфері нові дані"

cleanup:
    if (hEventReady) CloseHandle(hEventReady);
    if (hEventData)  CloseHandle(hEventData);
    if (hMutex)      CloseHandle(hMutex);
    CloseHandle(hMapFile);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        wchar_t buf[FM_TEXT_CHARS];
        hwndEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0, hWnd, (HMENU)ID_EDIT, GetModuleHandleW(NULL), NULL);
        BuildDefaultMessage(hWnd, buf, FM_TEXT_CHARS);
        SetWindowTextW(hwndEdit, buf);
        return 0;
    }
    case WM_SIZE:
        MoveWindow(hwndEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), TRUE);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_SEND)
            SendToServer(hWnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmd, int nShow)
{
    WNDCLASSW wc = { 0 };
    HMENU hBar;
    HWND hWnd;
    MSG m;

    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"FmClientWnd";
    RegisterClassW(&wc);

    hBar = CreateMenu();
    AppendMenuW(hBar, MF_STRING, IDM_SEND, L"Відправити на сервер");

    hWnd = CreateWindowW(L"FmClientWnd",
#if CLIENT_NO == 1
        L"Клієнт №1",
#else
        L"Клієнт №2",
#endif
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 420, 240, NULL, hBar, hInst, NULL);
    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    while (GetMessageW(&m, NULL, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return (int)m.wParam;
}