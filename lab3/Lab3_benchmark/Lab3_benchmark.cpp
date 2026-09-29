// Бенчмарк: контрольна сума великого масиву при 1, 2, 4, 8, 16 потоках
// Збірка: cl /EHsc /O2 /utf-8 bench_checksum.cpp
// Обов'язково запускайте Release (/O2), не Debug.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <vector>

constexpr size_t N_ELEMS = 64ull * 1024 * 1024; // 64M * 4 Б = 256 МБ
constexpr int    PASSES = 4;                   // кілька проходів, щоб задача була CPU-bound
constexpr int    RUNS = 3;                   // повторів для усереднення

static uint32_t* g_data;
static HANDLE    g_start;   // подія: одночасний старт усіх потоків
static HANDLE    g_mutex;   // м'ютекс: захист загальної суми
static uint64_t  g_total;

struct Job { size_t begin, end; };

DWORD WINAPI SumWorker(LPVOID p)
{
    Job* j = static_cast<Job*>(p);
    WaitForSingleObject(g_start, INFINITE);      // чекаємо сигналу старту

    uint64_t local = 0;
    for (int pass = 0; pass < PASSES; ++pass)
        for (size_t i = j->begin; i < j->end; ++i) {
            uint32_t v = g_data[i];
            local += (uint64_t)((v * 2654435761u) ^ (v >> 13));
        }

    WaitForSingleObject(g_mutex, INFINITE);      // короткa критична секція
    g_total += local;
    ReleaseMutex(g_mutex);
    return 0;
}

static uint64_t ft2u64(const FILETIME& f) { return ((uint64_t)f.dwHighDateTime << 32) | f.dwLowDateTime; }

struct Result { double ms; double cpu; uint64_t sum; };

static Result RunOnce(int threads, int cpus)
{
    g_total = 0;
    ResetEvent(g_start);

    std::vector<Job>    jobs(threads);
    std::vector<HANDLE> hs(threads);
    size_t chunk = N_ELEMS / threads;
    for (int t = 0; t < threads; ++t) {
        jobs[t].begin = t * chunk;
        jobs[t].end = (t == threads - 1) ? N_ELEMS : (t + 1) * chunk;
        hs[t] = CreateThread(nullptr, 0, SumWorker, &jobs[t], 0, nullptr);
    }

    FILETIME c, e, k0, u0, k1, u1;
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k0, &u0);
    LARGE_INTEGER fq, t0, t1;
    QueryPerformanceFrequency(&fq);

    QueryPerformanceCounter(&t0);
    SetEvent(g_start);                                        // старт
    WaitForMultipleObjects(threads, hs.data(), TRUE, INFINITE); // чекаємо всіх
    QueryPerformanceCounter(&t1);
    GetProcessTimes(GetCurrentProcess(), &c, &e, &k1, &u1);

    for (HANDLE h : hs) CloseHandle(h);

    double wall_ms = (t1.QuadPart - t0.QuadPart) * 1000.0 / fq.QuadPart;
    double cpu_ms = ((ft2u64(k1) - ft2u64(k0)) + (ft2u64(u1) - ft2u64(u0))) / 10000.0; // 100 нс -> мс
    double cpu_load = cpu_ms / (wall_ms * cpus) * 100.0;
    return { wall_ms, cpu_load, g_total };
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SYSTEM_INFO si; GetSystemInfo(&si);
    int cpus = (int)si.dwNumberOfProcessors;
    printf("Логічних процесорів: %d, масив: %zu МБ, проходів: %d, повторів: %d\n\n",
        cpus, N_ELEMS * sizeof(uint32_t) >> 20, PASSES, RUNS);

    g_data = (uint32_t*)VirtualAlloc(nullptr, N_ELEMS * sizeof(uint32_t),
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    for (size_t i = 0; i < N_ELEMS; ++i) g_data[i] = (uint32_t)(i * 1103515245u + 12345u);

    g_start = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_mutex = CreateMutexW(nullptr, FALSE, nullptr);

    const int counts[] = { 1, 2, 4, 8, 16 };
    printf("%-8s | %-14s | %-16s | %s\n", "Потоків", "Час, мс (сер.)", "Сер. CPU, %", "Контрольна сума");
    printf("---------+----------------+------------------+---------------------\n");

    uint64_t ref = 0; bool ok = true;
    for (int n : counts) {
        double ms = 0, cpu = 0; uint64_t sum = 0;
        for (int r = 0; r < RUNS; ++r) {
            Result x = RunOnce(n, cpus);
            ms += x.ms; cpu += x.cpu; sum = x.sum;
        }
        if (ref == 0) ref = sum; else if (sum != ref) ok = false;
        printf("%-8d | %-14.1f | %-16.1f | %llu\n", n, ms / RUNS, cpu / RUNS, (unsigned long long)sum);
    }
    printf("\nПеревірка коректності (суми збігаються): %s\n", ok ? "OK" : "ПОМИЛКА");

    CloseHandle(g_start); CloseHandle(g_mutex);
    VirtualFree(g_data, 0, MEM_RELEASE);
    return 0;
}