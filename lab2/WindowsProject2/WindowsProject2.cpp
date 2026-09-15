#include "framework.h"
#include "WindowsProject2.h"

#define MAX_LOADSTRING 100

// Глобал змінні
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

// Змінні для збереження поточних розмірів клієнтської області вікна!
int clientWidth = 500;
int clientHeight = 300;

// Константний масив, що задає бітове зображення (Рисунок: 16x16 пікселів)
// Кожні 2 байти (16 біт) представляють один рядок пікселів. 1 - білий, 0 - чорний
const BYTE bmpImageBits[32] = {
    0x03, 0xC0, // 00000011 11000000
    0x0F, 0xF0, // 00001111 11110000
    0x1F, 0xF8, // 00011111 11111000
    0x3F, 0xFC, // 00111111 11111100
    0x33, 0xCC, // 00110011 11001100 (очі)
    0x33, 0xCC, // 00110011 11001100
    0x3F, 0xFC, // 00111111 11111100
    0x3F, 0xFC, // 00111111 11111100
    0x3C, 0x3C, // 00111100 00111100 (рот)
    0x3E, 0x7C, // 00111110 01111100
    0x3F, 0xFC, // 00111111 11111100
    0x1F, 0xF8, // 00011111 11111000
    0x0F, 0xF0, // 00001111 11110000
    0x03, 0xC0, // 00000011 11000000
    0x00, 0x00, // 00000000 00000000
    0x00, 0x00  // 00000000 00000000
};

// Попередні оголошення функцій
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    wcscpy_s(szWindowClass, MAX_LOADSTRING, L"BondarchukBohachov_Lab2");

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {return FALSE;}

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSPROJECT2));
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW; // Перемальовування при зміні розмірів
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr; // Вимикаємо меню для чистоти вікна
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, L"Лабораторна 2 - Варіант 7", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, clientWidth, clientHeight, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd)
    {return FALSE;}
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        // Отримуємо нові розміри вікна при розтягуванні/стисканні
        clientWidth = LOWORD(lParam);
        clientHeight = HIWORD(lParam);

        // Сповіщаємо систему, що вікно треба перемалювати
        InvalidateRect(hWnd, NULL, TRUE);
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // 1. Створюємо бітове зображення з нашого константного масиву
        // Параметри: ширина (16), висота (16), ксть кольорових площин (1), біт на піксель (1 - монохром), масив даних
        HBITMAP hBitmap = CreateBitmap(16, 16, 1, 1, bmpImageBits);
        // 2. Створюю сумісний контекст пристрою (в пам'яті), щоб завантажити туди малюнок
        HDC hdcMem = CreateCompatibleDC(hdc);
        // 3. Робимо наше зображення поточним для контексту пам'яті
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
        // 4. Виводимо зображення на екран з масштабуванням.
        // Функція StretchBlt розтягує малюнок 16x16 до поточних розмірів вікна (clientWidth, clientHeight)
        StretchBlt(hdc, 0, 0, clientWidth, clientHeight, hdcMem, 0, 0, 16, 16, SRCCOPY);
        // 5. Очищення ресурсів GDI, щоб не було витоку пам'яті
        SelectObject(hdcMem, hOldBitmap);
        DeleteDC(hdcMem);
        DeleteObject(hBitmap);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}