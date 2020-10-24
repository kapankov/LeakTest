#include "pch.h"

#pragma comment(lib, "Dbghelp.lib")

void PrintImageImportW(HMODULE hmodCaller)
{
    ULONG ulSize = 0;
    // получить адрес секции импорта
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc =
        (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(
            hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
    if (pImportDesc == nullptr)
        return; // в этом модуле нет раздела импорта
    OutputDebugString(L"Image import:");
    for (; pImportDesc->Name; pImportDesc++)
    {
        PSTR pszModName = (PSTR)((PBYTE)hmodCaller + pImportDesc->Name);
        int wchars_num = MultiByteToWideChar(CP_UTF8, 0, pszModName, -1, NULL, 0);
        wchar_t* pwcString = (wchar_t*)malloc((wchars_num + 1) * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, pszModName, -1, pwcString, wchars_num);
        OutputDebugString(pwcString);
        free(pwcString);
    }
}

BOOL ReplaceIATEntryInOneMod(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew, HMODULE hmodCaller)
{
    ULONG ulSize = 0;
    // получить адрес секции импорта
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc =
        (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(
            hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
    if (pImportDesc == nullptr)
        return FALSE; // в этом модуле нет раздела импорта

    // находим дескриптор  раздела импорта  со ссылками
    // на функции Dll вызываемого модуля
    for (; pImportDesc->Name; pImportDesc++)
    {
        PSTR pszModName = (PSTR)((PBYTE)hmodCaller + pImportDesc->Name);
        if (::lstrcmpiA(pszModName, pszCalleeModName) == 0)
            break;
    }

    if (pImportDesc->Name == 0)
        return FALSE; // этот модуль  не импортирует никаких функций из данной Dll
    // получаем таблицу адресов импорта (IAT) для функций Dll
    PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hmodCaller + pImportDesc->FirstThunk);
    // заменяем адрес исходной функции адресом своей функции
    for (; pThunk->u1.Function; pThunk++)
    {
        // получаем адрес адреса функции
        PROC* ppfn = (PROC*)&pThunk->u1.Function;
        // если это та самая функция
        if (*ppfn == pfnCurrent)
        {
            // адреса сходятся, изменяем адрес в разделе импорта
            DWORD dwDummy = 0;
            // Разершим запись в эту страницу
            ::VirtualProtect(ppfn, sizeof(ppfn), PAGE_EXECUTE_READWRITE, &dwDummy);
            // Сменим адрес на свой
            ::WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), nullptr);
            // Восстановим аттрибуты
            ::VirtualProtect(ppfn, sizeof(ppfn), dwDummy, &dwDummy);
            // готово!
            return TRUE;
        }
    }
    return FALSE;
}