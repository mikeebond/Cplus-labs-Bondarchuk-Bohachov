// Лабораторна: Багатозадачність та багатопоточність (Win32)
// Збірка (Developer Command Prompt for VS):
//   cl /EHsc /utf-8 lab_threads.cpp user32.lib gdi32.lib
//
// Керування:
//   ЛКМ         - потік виводить СПАДНИЙ ряд (20..1) у позицію курсора
//   ПКМ         - потік виводить ЗРОСТАЮЧИЙ ряд (1..20) у позицію курсора
//   СКМ         - контекстне меню вибору ліміту потоків (0..20)
//   Меню "Limit"- те саме через рядок меню
//   P           - пауза/продовження всіх потоків (ResetEvent / SetEvent)
//   C           - імітація аварійного завершення одного потоку
//                 (потік гине, не звільнивши м'ютекс -> WAIT_ABANDONED)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cwchar>

constexpr int   MAX_THREADS = 20;     // Верхня межа ліміту
constexpr int   SERIES_LEN = 20;      // Довжина ряду
constexpr DWORD STEP_DELAY = 150;     // мс між числами
constexpr int   ID_LIMIT_BASE = 1000; // ID пунктів меню: 1000 + N
constexpr UINT_PTR ID_TIMER = 1;

constexpr DWORD EXIT_OK = 0;
constexpr DWORD EXIT_STOP = 1;        // Завершення програми
constexpr DWORD EXIT_LIMIT = 2;       // Ліміт зменшили під час роботи
constexpr DWORD EXIT_CRASH = 0xDEAD;  // Імітація аварії

struct Slot {
    bool        used;
    HANDLE      h;
    DWORD       id;
    volatile LONG abort;      // Просимо потік завершитись (ліміт зменшено)
    volatile LONG crash;      // Просимо потік "впасти"
    int         x, y;
    bool        ascending;
};

static Slot    g_slots[MAX_THREADS];
static HANDLE g_hMutex;   // Взаємне виключення: доступ до вікна/GDI
static HANDLE g_hRun;     // Manual-reset event: сигнал = "працювати", скинутий = пауза
static HANDLE g_hStop;    // Manual-reset event: сигнал = "усім завершуватись"
static HWND   g_hwnd;
static HMENU  g_hLimitMenu;
static int    g_limit = 5;
static bool   g_paused = false;

// ---------------------------------------------------------------- Потік
DWORD WINAPI Worker(LPVOID param)
{
    Slot* s = static_cast<Slot*>(param);
    DWORD tid = GetCurrentThreadId();
    int   x = s->x;

    printf("[thread %5lu] START (%s series, pos %d,%d)\n",
        tid, s->ascending ? "ascending" : "descending", s->x, s->y);

    for (int i = 0; i < SERIES_LEN; ++i) {
        int value = s->ascending ? (i + 1) : (SERIES_LEN - i);

        // Чекаємо дозволу працювати (не пауза) АБО сигнал зупинки.
        // При одночасному сигналі повертається менший індекс -> stop має пріоритет.
        HANDLE ev[2] = { g_hStop, g_hRun };
        DWORD w = WaitForMultipleObjects(2, ev, FALSE, INFINITE);
        if (w == WAIT_OBJECT_0) {
            printf("[thread %5lu] EXIT (stop signal)\n", tid);
            ExitThread(EXIT_STOP);
        }
        if (s->abort) {
            printf("[thread %5lu] EXIT (thread limit reduced)\n", tid);
            ExitThread(EXIT_LIMIT);
        }

        // ---- Критична секція: спільний ресурс = вікно (GDI) ----
        w = WaitForSingleObject(g_hMutex, INFINITE);
        if (w == WAIT_ABANDONED)
            printf("[thread %5lu] WARNING: Mutex abandoned by crashed thread, restoring ownership\n", tid);

        printf("[thread %5lu] ACCESS resource, value %d\n", tid, value);

        HDC dc = GetDC(g_hwnd);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, s->ascending ? RGB(0, 90, 200) : RGB(200, 30, 30));
        wchar_t buf[16];
        int len = swprintf_s(buf, L"%d", value);
        TextOutW(dc, x, s->y, buf, len);
        ReleaseDC(g_hwnd, dc);
        x += 28;

        if (s->crash) {
            // Аварія ВСЕРЕДИНІ критичної секції: м'ютекс НЕ звільняємо.
            printf("[thread %5lu] CRASH (mutex not released!)\n", tid);
            ExitThread(EXIT_CRASH);
        }
        ReleaseMutex(g_hMutex);
        // ---- Кінець критичної секції ----

        // "Сон", який можна перервати сигналом зупинки
        if (WaitForSingleObject(g_hStop, STEP_DELAY) == WAIT_OBJECT_0) {
            printf("[thread %5lu] EXIT (stop signal)\n", tid);
            ExitThread(EXIT_STOP);
        }
    }

    printf("[thread %5lu] EXIT (finished counting)\n", tid);
    ExitThread(EXIT_OK);
    return 0; // Недосяжно
}

// ---------------------------------------------------------------- Керування (тільки GUI-потік)
static int ActiveCount()
{
    int n = 0;
    for (auto& s : g_slots) if (s.used) ++n;
    return n;
}

static void StartWorker(int x, int y, bool ascending)
{
    if (ActiveCount() >= g_limit) {
        MessageBeep(MB_ICONEXCLAMATION);
        printf("[main] Rejected: thread limit reached (%d)\n", g_limit);
        return;
    }
    for (auto& s : g_slots) {
        if (s.used) continue;
        s.abort = 0; s.crash = 0;
        s.x = x; s.y = y; s.ascending = ascending;
        s.h = CreateThread(nullptr, 0, Worker, &s, 0, &s.id);
        if (!s.h) { printf("[main] CreateThread error %lu\n", GetLastError()); return; }
        s.used = true;
        return;
    }
}

// Збирає завершені потоки: WaitForMultipleObjects з нульовим таймаутом
static void ReapFinished()
{
    for (;;) {
        HANDLE hs[MAX_THREADS];
        Slot* map[MAX_THREADS];
        int n = 0;
        for (auto& s : g_slots)
            if (s.used) { hs[n] = s.h; map[n] = &s; ++n; }
        if (n == 0) return;

        DWORD r = WaitForMultipleObjects(n, hs, FALSE, 0);
        if (r >= WAIT_OBJECT_0 && r < WAIT_OBJECT_0 + (DWORD)n) {
            Slot* s = map[r - WAIT_OBJECT_0];
            DWORD code = 0;
            GetExitCodeThread(s->h, &code);
            const char* why = code == EXIT_OK ? "OK" :
                code == EXIT_LIMIT ? "limit reduced" :
                code == EXIT_STOP ? "stop signal" :
                code == EXIT_CRASH ? "CRASH" : "unknown";
            printf("[main] thread %lu exited: code %lu (%s)\n", s->id, code, why);
            CloseHandle(s->h);
            s->used = false;
        }
        else {
            return; // Нікого завершеного більше немає
        }
    }
}

static void SetLimit(int n)
{
    if (n < 0) n = 0;
    if (n > MAX_THREADS) n = MAX_THREADS;
    g_limit = n;
    CheckMenuRadioItem(g_hLimitMenu, ID_LIMIT_BASE, ID_LIMIT_BASE + MAX_THREADS,
        ID_LIMIT_BASE + n, MF_BYCOMMAND);
    printf("[main] New thread limit: %d\n", n);

    // Динамічне керування: якщо активних більше за ліміт - просимо зайві завершитись
    int excess = ActiveCount() - n;
    for (int i = MAX_THREADS - 1; i >= 0 && excess > 0; --i) {
        if (g_slots[i].used && !g_slots[i].abort) {
            InterlockedExchange(&g_slots[i].abort, 1);
            --excess;
        }
    }
}

static void UpdateTitle()
{
    wchar_t t[160];
    swprintf_s(t, L"Threads: %d / %d%s   [LMB: desc, RMB: asc, MMB: limit, P: pause, C: crash]",
        ActiveCount(), g_limit, g_paused ? L"  (PAUSED)" : L"");
    SetWindowTextW(g_hwnd, t);
}

static void ShutdownAll()
{
    KillTimer(g_hwnd, ID_TIMER);
    SetEvent(g_hStop);
    SetEvent(g_hRun); // Розбудити потоки, що стоять на паузі

    HANDLE hs[MAX_THREADS];
    int n = 0;
    for (auto& s : g_slots) if (s.used) hs[n++] = s.h;
    if (n > 0) {
        printf("[main] Waiting for %d threads to finish...\n", n);
        DWORD r = WaitForMultipleObjects(n, hs, TRUE, 5000);
        if (r == WAIT_TIMEOUT) printf("[main] Timeout waiting for threads!\n");
    }
    ReapFinished();
}

// ---------------------------------------------------------------- Вікно
static HMENU BuildLimitMenu()
{
    HMENU m = CreatePopupMenu();
    for (int i = 0; i <= MAX_THREADS; ++i) {
        wchar_t txt[16];
        swprintf_s(txt, L"%d", i);
        AppendMenuW(m, MF_STRING, ID_LIMIT_BASE + i, txt);
    }
    return m;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_LBUTTONDOWN:
        StartWorker(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), false);
        return 0;
    case WM_RBUTTONDOWN:
        StartWorker(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), true);
        return 0;
    case WM_MBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ClientToScreen(hwnd, &p);
        TrackPopupMenu(g_hLimitMenu, TPM_LEFTALIGN | TPM_TOPALIGN, p.x, p.y, 0, hwnd, nullptr);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= ID_LIMIT_BASE && id <= ID_LIMIT_BASE + MAX_THREADS)
            SetLimit(id - ID_LIMIT_BASE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == 'P') {
            g_paused = !g_paused;
            if (g_paused) ResetEvent(g_hRun); else SetEvent(g_hRun);
            printf("[main] %s\n", g_paused ? "PAUSE (ResetEvent)" : "RESUME (SetEvent)");
        }
        else if (wp == 'C') {
            for (auto& s : g_slots)
                if (s.used && !s.crash) {
                    InterlockedExchange(&s.crash, 1);
                    printf("[main] Simulating crash for thread %lu\n", s.id);
                    break;
                }
        }
        return 0;
    case WM_TIMER:
        ReapFinished();
        UpdateTitle();
        return 0;
    case WM_CLOSE:
        ShutdownAll();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow)
{
    AllocConsole();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    setvbuf(stdout, nullptr, _IONBF, 0);

    g_hMutex = CreateMutexW(nullptr, FALSE, nullptr);
    g_hRun = CreateEventW(nullptr, TRUE, TRUE, nullptr); // Manual-reset, початково сигнальний
    g_hStop = CreateEventW(nullptr, TRUE, FALSE, nullptr); // Manual-reset, початково скинутий

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"LabThreadsWnd";
    RegisterClassW(&wc);

    g_hLimitMenu = BuildLimitMenu();
    HMENU bar = CreateMenu();
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)g_hLimitMenu, L"Thread Limit");

    g_hwnd = CreateWindowW(L"LabThreadsWnd", L"Threads", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1000, 650,
        nullptr, bar, hInst, nullptr);
    SetLimit(g_limit);
    SetTimer(g_hwnd, ID_TIMER, 200, nullptr);
    ShowWindow(g_hwnd, nShow);
    UpdateWindow(g_hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    CloseHandle(g_hMutex);
    CloseHandle(g_hRun);
    CloseHandle(g_hStop);
    return 0;
}