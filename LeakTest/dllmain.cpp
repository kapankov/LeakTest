// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

static DWORD s_dwTlsIndex;
using AllocMap = std::map<void*, std::wstring>;
static safe_ptr <AllocMap> g_AllocMap;


typedef void* (__cdecl* type_malloc)(size_t);
typedef void* (__cdecl* type_realloc)(void* _Block, size_t _Size);
typedef void (__cdecl* type_free)(void* _Block);

type_malloc True_malloc = nullptr;
type_realloc True_realloc = nullptr;
type_free True_free = nullptr;

void* __cdecl Mine_malloc(size_t _Size)
{
    ::OutputDebugString(_TEXT("malloc called!"));
    void* result = True_malloc(_Size);
    if (result)
        (*g_AllocMap)[result] = GetCallStack(L"malloc");
    return result;
}

void* __cdecl Mine_realloc(void* _Block, size_t _Size)
{
    ::OutputDebugString(_TEXT("realloc called!"));
    void* result = True_realloc(_Block, _Size);
    if (result)
    {
        g_AllocMap->erase(_Block);
        (*g_AllocMap)[result] = GetCallStack(L"realloc");

    }
    return result;
}

void __cdecl Mine_free(void* _Block)
{
    ::OutputDebugString(_TEXT("free called!"));
    True_free(_Block);
    g_AllocMap->erase(_Block);
}

// ****************************************************************************
void Analize()
{
    wchar_t szOutDebug[4096] = {};
    // Allocations
    size_t cntAlloc = 0;
    for (AllocMap::iterator it = g_AllocMap->begin(); it != g_AllocMap->end(); ++it)
    {
        _snwprintf_s(szOutDebug, sizeof(szOutDebug) / sizeof(wchar_t), sizeof(szOutDebug) / sizeof(wchar_t) - 1, L"Pointer: %I64X\n%s\n", reinterpret_cast<DWORD_PTR>(it->first), it->second.c_str());
        OutputDebugString(szOutDebug);
        cntAlloc++;
    }
    UnloadSymbols();
    OutputDebugString(L"************************************************\n");
    _snwprintf_s(szOutDebug, sizeof(szOutDebug) / sizeof(wchar_t), sizeof(szOutDebug) / sizeof(wchar_t) - 1, L"Memory leaks: %zu\n", cntAlloc);
    OutputDebugString(szOutDebug);
    OutputDebugString(L"************************************************\n");

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

    PrintImageImportW(::GetModuleHandle(nullptr));

    HMODULE hModule = ::GetModuleHandle(_TEXT("api-ms-win-crt-heap-l1-1-0.dll"));
    True_malloc = (type_malloc)::GetProcAddress(hModule, "malloc");
    True_realloc = (type_realloc)::GetProcAddress(hModule, "realloc");
    True_free = (type_free)::GetProcAddress(hModule, "free");

    HMODULE hmodCaller = ::GetModuleHandle(nullptr);
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_malloc, (PROC)Mine_malloc, hmodCaller))
        ::OutputDebugString(_TEXT("malloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_realloc, (PROC)Mine_realloc, hmodCaller))
        ::OutputDebugString(_TEXT("realloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_free, (PROC)Mine_free, hmodCaller))
        ::OutputDebugString(_TEXT("free replacement failed"));
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
    Analize();
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

