// common.h - спільні визначення сервера та клієнтів (лабораторна: filemapping, варіант 7)
#pragma once
#include <windows.h>

// Імена іменованих об'єктів ядра, за якими клієнти підключаються до сервера
#define FM_NAME        L"myFileMapping"        // об'єкт відображення файлу
#define FM_MUTEX       L"myFileMappingMutex"   // м'ютекс: взаємовиключний доступ до буфера
#define FM_EVENT_DATA  L"myFileMappingData"    // подія "у буфері нові дані" (клієнт -> сервер)
#define FM_EVENT_READY L"myFileMappingReady"   // подія "буфер вільний" (сервер -> клієнти)

#define FM_TEXT_CHARS  480

// Структура даних усередині проекції
typedef struct _SHARED_DATA
{
    DWORD   seq;                    // лічильник записів (0 - ще ніхто нічого не писав)
    DWORD   clientNo;               // номер клієнта-відправника
    wchar_t text[FM_TEXT_CHARS];    // текст повідомлення (UTF-16, завершується нулем)
} SHARED_DATA;