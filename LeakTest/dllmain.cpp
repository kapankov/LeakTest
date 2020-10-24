// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

static DWORD s_dwTlsIndex;

typedef HANDLE
(WINAPI* type_CreateFileA)(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);
typedef HANDLE
(WINAPI* type_CreateFileW)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);

typedef BOOL
(WINAPI* type_CloseHandle)(HANDLE hObject);

type_CreateFileA True_CreateFileA = nullptr;
type_CreateFileW True_CreateFileW = nullptr;
type_CloseHandle True_CloseHandle = nullptr;

static HANDLE g_hFile = nullptr;
HANDLE WINAPI Mine_CreateFileA(
    LPCSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    ::OutputDebugString(_TEXT("CreateFileA called!"));
    return g_hFile = True_CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

HANDLE WINAPI Mine_CreateFileW(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    ::OutputDebugString(_TEXT("CreateFileW called!"));
    return g_hFile = True_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI Mine_CloseHandle(HANDLE hObject)
{
    ::OutputDebugString(_TEXT("CloseHandle called!"));
    if (g_hFile == hObject)
    {
        TCHAR szSign[MINCHAR] = _TEXT("<Hacked by Interceptor>");
        DWORD dwWritten = 0;
        WriteFile(hObject, szSign, lstrlen(szSign) * sizeof(TCHAR), &dwWritten, nullptr);
    }
    return True_CloseHandle(hObject);
}

BOOL ThreadAttach(HMODULE hDll)
{
    ::OutputDebugString(_TEXT("LeakTest DLL_THREAD_ATTACH"));
    return TlsSetValue(s_dwTlsIndex, (PVOID)0);
}

BOOL ProcessAttach(HMODULE hDll)
{
    ::OutputDebugString(_TEXT("LeakTest DLL_PROCESS_ATTACH"));
    if ((s_dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
        return FALSE;

    HMODULE hUserDll = ::GetModuleHandle(_TEXT("Kernel32.dll"));
    True_CreateFileA = (type_CreateFileA)::GetProcAddress(hUserDll, "CreateFileA");
    True_CreateFileW = (type_CreateFileW)::GetProcAddress(hUserDll, "CreateFileW");
    True_CloseHandle = (type_CloseHandle)::GetProcAddress(hUserDll, "CloseHandle");

    HMODULE hmodCaller = ::GetModuleHandle(nullptr);
    if (!ReplaceIATEntryInOneMod("Kernel32.dll", (PROC)True_CreateFileA, (PROC)Mine_CreateFileA, hmodCaller))
        ::OutputDebugString(_TEXT("CreateFileA replacement failed"));
    if (!ReplaceIATEntryInOneMod("Kernel32.dll", (PROC)True_CreateFileW, (PROC)Mine_CreateFileW, hmodCaller))
        ::OutputDebugString(_TEXT("CreateFileW replacement failed"));
    if (!ReplaceIATEntryInOneMod("Kernel32.dll", (PROC)True_CloseHandle, (PROC)Mine_CloseHandle, hmodCaller))
        ::OutputDebugString(_TEXT("CloseHandle replacement failed"));
    return ThreadAttach(hDll);
}

BOOL ThreadDetach(HMODULE hDll)
{
    ::OutputDebugString(_TEXT("LeakTest DLL_THREAD_DETACH"));
    return TlsSetValue(s_dwTlsIndex, (PVOID)0);
}

BOOL ProcessDetach(HMODULE hDll)
{
    ::OutputDebugString(_TEXT("LeakTest DLL_PROCESS_DETACH"));
    ThreadDetach(hDll);
    TlsFree(s_dwTlsIndex);
    return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        return ProcessAttach(hModule);
    case DLL_THREAD_ATTACH:
        return ThreadAttach(hModule);
    case DLL_THREAD_DETACH:
        return ThreadDetach(hModule);
    case DLL_PROCESS_DETACH:
        return ProcessDetach(hModule);
    }
    return TRUE;
}

