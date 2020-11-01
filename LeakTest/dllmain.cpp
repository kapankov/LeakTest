// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

static DWORD s_dwTlsIndex;

class CntGuard
{
    DWORD m_value;
public:
    CntGuard() : m_value(0)
    {
        m_value = (DWORD)(DWORD_PTR)TlsGetValue(s_dwTlsIndex);
        if (m_value == 0)
            TlsSetValue(s_dwTlsIndex, (PVOID)(DWORD_PTR)1);
    }
    ~CntGuard()
    {
        if (m_value == 0)
            TlsSetValue(s_dwTlsIndex, (PVOID)(DWORD_PTR)0);
    }
    BOOL IsNotZero()
    {
        return m_value>0;
    }
};

using AllocMap = std::map<void*, std::wstring>;
static safe_ptr<AllocMap> g_AllocMap;
using HeapAllocMap = std::map<HANDLE, AllocMap>;
static safe_ptr<HeapAllocMap> g_HeapAllocMap;

typedef void* (__cdecl* type_malloc)(size_t);
typedef void* (__cdecl* type_calloc)(size_t _Count, size_t _Size);
typedef void* (__cdecl* type_realloc)(void*, size_t);
typedef void (__cdecl* type_free)(void*);
typedef BOOL (WINAPI* type_HeapDestroy)(HANDLE hHeap);
typedef LPVOID (WINAPI* type_HeapAlloc)(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
typedef LPVOID (WINAPI* type_HeapReAlloc)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes);
typedef BOOL (WINAPI* type_HeapFree)(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);

type_malloc True_malloc = nullptr;
type_calloc True_calloc = nullptr;
type_realloc True_realloc = nullptr;
type_free True_free = nullptr;
type_HeapDestroy True_HeapDestroy = nullptr;
type_HeapAlloc True_HeapAlloc = nullptr;
type_HeapReAlloc True_HeapReAlloc = nullptr;
type_HeapFree True_HeapFree = nullptr;

void* __cdecl Mine_malloc(size_t _Size)
{
    ::OutputDebugString(_TEXT("malloc called!"));
    void* result = True_malloc(_Size);
    if (result)
        (*g_AllocMap)[result] = GetCallStack(L"malloc");
    return result;
}

void* __cdecl Mine_calloc(size_t _Count, size_t _Size)
{
    ::OutputDebugString(_TEXT("calloc called!"));
    void* result = True_calloc(_Count, _Size);
    if (result)
        (*g_AllocMap)[result] = GetCallStack(L"calloc");
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

BOOL WINAPI Mine_HeapDestroy(HANDLE hHeap)
{
    CntGuard guard;
    if (guard.IsNotZero())
        return True_HeapDestroy(hHeap);
    OutputDebugString(L"************************************************\n");
    ::OutputDebugString(_TEXT("HeapDestroy called!"));
    BOOL result = True_HeapDestroy(hHeap);
    if (result)
    {
        HeapAllocMap::iterator it = g_HeapAllocMap->find(hHeap);
        if (it != g_HeapAllocMap->end())
        {
            wchar_t szOutDebug[4096] = {};
            for (AllocMap::iterator vit = it->second.begin(); vit != it->second.end(); ++vit)
            {
                _snwprintf_s(szOutDebug, sizeof(szOutDebug) / sizeof(wchar_t), sizeof(szOutDebug) / sizeof(wchar_t) - 1, L"Pointer: %I64X\n%s\n", reinterpret_cast<DWORD_PTR>(vit->first), vit->second.c_str());
                OutputDebugString(szOutDebug);
            }
        }
        g_HeapAllocMap->erase(hHeap);
    }
    OutputDebugString(L"************************************************\n");
    return result;
}

LPVOID WINAPI Mine_HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes)
{
    CntGuard guard;
    if (guard.IsNotZero()) // Отсекаем все "внутренние" вызовы HeapAlloc
        return True_HeapAlloc(hHeap, dwFlags, dwBytes);
    ::OutputDebugString(_TEXT("HeapAlloc called!"));
    LPVOID result = True_HeapAlloc(hHeap, dwFlags, dwBytes);
    if (result)
        (*g_HeapAllocMap)[hHeap][result] = GetCallStack(L"HeapAlloc");
    return result;
}

LPVOID WINAPI Mine_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
    CntGuard guard;
    if (guard.IsNotZero())
        return True_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
    ::OutputDebugString(_TEXT("HeapReAlloc called!"));
    LPVOID result = True_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
    if (result)
    {
        g_AllocMap->erase(lpMem);
        (*g_HeapAllocMap)[hHeap][result] = GetCallStack(L"HeapReAlloc");
    }
    return result;
}

BOOL WINAPI Mine_HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    CntGuard guard;
    if (guard.IsNotZero())
        return True_HeapFree(hHeap, dwFlags, lpMem);
    ::OutputDebugString(_TEXT("HeapFree called!"));
    BOOL result = True_HeapFree(hHeap, dwFlags, lpMem);
    if (result)
        (*g_HeapAllocMap)[hHeap].erase(lpMem);
    return result;
}

// ****************************************************************************
void Analize()
{
    wchar_t szOutDebug[4096] = {};
    OutputDebugString(L"************************************************\n");
    // Allocations
    size_t cntAlloc = 0;
    for (AllocMap::iterator it = g_AllocMap->begin(); it != g_AllocMap->end(); ++it)
    {
        _snwprintf_s(szOutDebug, sizeof(szOutDebug) / sizeof(wchar_t), sizeof(szOutDebug) / sizeof(wchar_t) - 1, L"Pointer: %I64X\n%s\n", reinterpret_cast<DWORD_PTR>(it->first), it->second.c_str());
        OutputDebugString(szOutDebug);
        cntAlloc++;
    }
    for (HeapAllocMap::iterator it = g_HeapAllocMap->begin(); it != g_HeapAllocMap->end(); ++it)
    {
        for (AllocMap::iterator vit = it->second.begin(); vit != it->second.end(); ++vit)
        {
            _snwprintf_s(szOutDebug, sizeof(szOutDebug) / sizeof(wchar_t), sizeof(szOutDebug) / sizeof(wchar_t) - 1, L"Pointer: %I64X\n%s\n", reinterpret_cast<DWORD_PTR>(vit->first), vit->second.c_str());
            OutputDebugString(szOutDebug);
            cntAlloc++;
        }
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
    True_calloc = (type_calloc)::GetProcAddress(hModule, "calloc");
    True_realloc = (type_realloc)::GetProcAddress(hModule, "realloc");
    True_free = (type_free)::GetProcAddress(hModule, "free");

    HMODULE hmodCaller = ::GetModuleHandle(nullptr);
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_malloc, (PROC)Mine_malloc, hmodCaller))
        ::OutputDebugString(_TEXT("malloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_calloc, (PROC)Mine_calloc, hmodCaller))
        ::OutputDebugString(_TEXT("calloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_realloc, (PROC)Mine_realloc, hmodCaller))
        ::OutputDebugString(_TEXT("realloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("api-ms-win-crt-heap-l1-1-0.dll", (PROC)True_free, (PROC)Mine_free, hmodCaller))
        ::OutputDebugString(_TEXT("free replacement failed"));

    hModule = ::GetModuleHandle(_TEXT("KERNEL32.dll"));
 //   True_HeapCreate = (type_HeapCreate)::GetProcAddress(hModule, "HeapCreate");
    True_HeapDestroy = (type_HeapDestroy)::GetProcAddress(hModule, "HeapDestroy");
    True_HeapAlloc = (type_HeapAlloc)::GetProcAddress(hModule, "HeapAlloc");
    True_HeapReAlloc = (type_HeapReAlloc)::GetProcAddress(hModule, "HeapReAlloc");
    True_HeapFree = (type_HeapFree)::GetProcAddress(hModule, "HeapFree");
/*    if (!ReplaceIATEntryInOneMod("KERNEL32.dll", (PROC)True_HeapCreate, (PROC)Mine_HeapCreate, hmodCaller))
        ::OutputDebugString(_TEXT("HeapCreate replacement failed"));*/
    if (!ReplaceIATEntryInOneMod("KERNEL32.dll", (PROC)True_HeapDestroy, (PROC)Mine_HeapDestroy, hmodCaller))
        ::OutputDebugString(_TEXT("HeapDestroy replacement failed"));
    if (!ReplaceIATEntryInOneMod("KERNEL32.dll", (PROC)True_HeapAlloc, (PROC)Mine_HeapAlloc, hmodCaller))
        ::OutputDebugString(_TEXT("HeapAlloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("KERNEL32.dll", (PROC)True_HeapReAlloc, (PROC)Mine_HeapReAlloc, hmodCaller))
        ::OutputDebugString(_TEXT("HeapReAlloc replacement failed"));
    if (!ReplaceIATEntryInOneMod("KERNEL32.dll", (PROC)True_HeapFree, (PROC)Mine_HeapFree, hmodCaller))
        ::OutputDebugString(_TEXT("HeapFree replacement failed"));


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

