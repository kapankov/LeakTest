// Injector.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>

void PrintUsage(void)
{
    std::cout << "Usage:\n"
        "    Injector.exe -d:file.dll -e:target.exe\n"
        "Options:\n"
        "    -d:file.dll   : start the process with file.dll.\n"
        "    -e:target.exe : application to run.\n";
}

BOOL WINAPI InjectLib(LPPROCESS_INFORMATION pi, LPCTSTR pszDllPath)
{
    BOOL result = FALSE;
    LPTSTR pszDllPathRemote = nullptr;
    HANDLE hThread = nullptr;
    __try {
        int iBufLen = (lstrlen(pszDllPath) + 1) * sizeof(TCHAR);
        // выделить память в запускаемом процессе
        pszDllPathRemote = (LPTSTR)
            VirtualAllocEx(pi->hProcess, nullptr, iBufLen, MEM_COMMIT, PAGE_READWRITE);
        if (pszDllPathRemote == nullptr) __leave;
        // скопируем туда имя нашей Dll
        if (!WriteProcessMemory(pi->hProcess, pszDllPathRemote,
            (PVOID)pszDllPath, iBufLen, nullptr)) __leave;
        // получим адрес LoadLibrary
        PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE)
            GetProcAddress(GetModuleHandle(_TEXT("Kernel32")), 
#ifdef _UNICODE
                "LoadLibraryW"
#else
                "LoadLibraryA"
#endif
            );
        if (pfnThreadRtn == nullptr) __leave;
        // создадим удаленный поток с вызовом LoadLibrary(pszDllPath)
        hThread = CreateRemoteThread(pi->hProcess, nullptr, 0,
            pfnThreadRtn, pszDllPathRemote, 0, nullptr);
        if (hThread == nullptr) __leave;
        // подождем, пока поток не завершится
        WaitForSingleObject(hThread, INFINITE);
        // поздравляю!
        result = TRUE;
    }
    __finally {
        // почистим за собой
        // выделенную память
        if (pszDllPathRemote)
            VirtualFreeEx(pi->hProcess, pszDllPathRemote, 0, MEM_RELEASE);
        if (hThread)
            CloseHandle(hThread);
    }
    return result;
}


int main(int argc, char* argv[])
{
    LPCSTR pszDll = nullptr;
    LPCSTR pszExe = nullptr;
    TCHAR szDllPath[MAX_PATH] = _TEXT("");
    TCHAR szCommandLine[MAX_PATH] = _TEXT("");
    size_t NumOfCharConverted = 0;
    for (int arg = 1; arg < argc && argv[arg][0] == '-'; arg++)
    {
        LPSTR argn = argv[arg] + 1;
        LPSTR argp = argn;
        while (*argp && *argp != ':' && *argp != '=')
            argp++;
        if (*argp == ':' || *argp == '=')
            *argp++ = '\0';

        switch (argn[0]) {
        case 'd':   // Set DLL path
        case 'D':
            pszDll = argp;
#ifdef _UNICODE
            mbstowcs_s(&NumOfCharConverted, szDllPath, sizeof(szDllPath) / sizeof(WCHAR), argp, strlen(argp));
#else
            lstrcpy(szDllName, argp);
#endif
            break;
        case 'e':   // Set Exe path
        case 'E':
            pszExe = argp;
#ifdef _UNICODE
            mbstowcs_s(&NumOfCharConverted, szCommandLine, sizeof(szCommandLine) / sizeof(WCHAR), argp, strlen(argp));
#else
            lstrcpy(szCommandLine, argp);
#endif

            break;
        }
    }

    if (!*szDllPath || !*szCommandLine)
    {
        PrintUsage();
        return 9001;
    }

    //TODO: проверить szDllPath + szCommandLine

    // запустить
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    si.cb = sizeof(STARTUPINFO);
    BOOL ret = ::CreateProcess(
        nullptr,
        szCommandLine,
        nullptr,
        nullptr,
        TRUE,
        CREATE_DEFAULT_ERROR_MODE | CREATE_SUSPENDED,
        nullptr,
        nullptr,
        &si,
        &pi
    );
    if (ret)
    {
        std::cout << pszExe << " successfully started.\n";
        // сделать прививку
        if (InjectLib(&pi, szDllPath))
        {
            std::cout << pszDll << " injection successfull.\n";
        }
        // запустить поток
        ::ResumeThread(pi.hThread);
        //TODO: можно подождать и что-нибудь сделать полезное
        // EjectLib(dwProcessId, szLibFile)
    }
    else
    {
        DWORD dwError = GetLastError();
        std::cout << pszExe << " failed: " << dwError << std::endl;
        return 9002;
    }

    return 0;
}
