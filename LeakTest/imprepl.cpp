#include "pch.h"

#pragma comment(lib, "Dbghelp.lib")

void PrintImageImportW(HMODULE hmodCaller)
{
    ULONG ulSize = 0;
    // �������� ����� ������ �������
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc =
        (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(
            hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
    if (pImportDesc == nullptr)
        return; // � ���� ������ ��� ������� �������
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
    // �������� ����� ������ �������
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc =
        (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(
            hmodCaller, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);
    if (pImportDesc == nullptr)
        return FALSE; // � ���� ������ ��� ������� �������

    // ������� ����������  ������� �������  �� ��������
    // �� ������� Dll ����������� ������
    for (; pImportDesc->Name; pImportDesc++)
    {
        PSTR pszModName = (PSTR)((PBYTE)hmodCaller + pImportDesc->Name);
        if (::lstrcmpiA(pszModName, pszCalleeModName) == 0)
            break;
    }

    if (pImportDesc->Name == 0)
        return FALSE; // ���� ������  �� ����������� ������� ������� �� ������ Dll
    // �������� ������� ������� ������� (IAT) ��� ������� Dll
    PIMAGE_THUNK_DATA pThunk = (PIMAGE_THUNK_DATA)((PBYTE)hmodCaller + pImportDesc->FirstThunk);
    // �������� ����� �������� ������� ������� ����� �������
    for (; pThunk->u1.Function; pThunk++)
    {
        // �������� ����� ������ �������
        PROC* ppfn = (PROC*)&pThunk->u1.Function;
        // ���� ��� �� ����� �������
        if (*ppfn == pfnCurrent)
        {
            // ������ ��������, �������� ����� � ������� �������
            DWORD dwDummy = 0;
            // �������� ������ � ��� ��������
            ::VirtualProtect(ppfn, sizeof(ppfn), PAGE_EXECUTE_READWRITE, &dwDummy);
            // ������ ����� �� ����
            ::WriteProcessMemory(GetCurrentProcess(), ppfn, &pfnNew, sizeof(pfnNew), nullptr);
            // ����������� ���������
            ::VirtualProtect(ppfn, sizeof(ppfn), dwDummy, &dwDummy);
            // ������!
            return TRUE;
        }
    }
    return FALSE;
}