#include "pch.h"

#pragma comment(lib, "Dbghelp.lib")

BOOL LoadSymbols()
{
    ::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_INCLUDE_32BIT_MODULES | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    return ::SymInitialize(::GetCurrentProcess(), "http://msdl.microsoft.com/download/symbols", TRUE);
}

void UnloadSymbols()
{
    ::SymCleanup(::GetCurrentProcess());
}

std::wstring GetCallStack(const wchar_t* prefix)
{
    std::wstring result(prefix);
    result.append(L"\n");
    DWORD disp;

    static BOOL fSymLoaded = FALSE;
    if (!fSymLoaded)
    {
        fSymLoaded = LoadSymbols();
        if (!fSymLoaded)
            return std::wstring(L"Symbols was not loaded");
    }

    PVOID addrs[5] = { 0 };
    WORD frames = CaptureStackBackTrace(2, 5, addrs, NULL);
    for (USHORT i = 0; i < frames; i++)
    {
        ULONG64 buffer[sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(TCHAR)] = { 0 };
        SYMBOL_INFOW* pSymbol = (SYMBOL_INFOW*)buffer;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
        pSymbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 dwDisplacement = 0;
        if (::SymFromAddrW(::GetCurrentProcess(), (DWORD64)addrs[i], &dwDisplacement, pSymbol))
        {
            result.append(pSymbol->Name, static_cast<size_t>(pSymbol->NameLen));
            IMAGEHLP_LINEW64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINEW64);
            if (::SymGetLineFromAddrW64(::GetCurrentProcess(), (DWORD64)addrs[i], &disp, &line))
            {
                wchar_t szLine[1024] = {};
                _snwprintf_s(szLine, sizeof(szLine) / sizeof(wchar_t), sizeof(szLine) / sizeof(wchar_t) - 1, L" in %s line: %lu", line.FileName, line.LineNumber);
                result.append(szLine);
            }

            result.append(L"\n");
        }
    }
    return result;
}