// Injector.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <tchar.h>

void PrintUsage(void)
{
    std::cout << "Usage:\n"
        "    Injector.exe target.exe\n"
        "Options:\n"
        "    target.exe : application to run.\n";
}

BOOL WINAPI InjectLib(LPPROCESS_INFORMATION pi, LPCTSTR pszDllPath)
{
    BOOL result = FALSE;
    LPTSTR pszDllPathRemote = nullptr;
    HANDLE hThread = nullptr;
    __try {
        int iBufLen = (lstrlen(pszDllPath) + 1) * sizeof(TCHAR);
        // �������� ������ � ����������� ��������
        pszDllPathRemote = (LPTSTR)
            VirtualAllocEx(pi->hProcess, nullptr, iBufLen, MEM_COMMIT, PAGE_READWRITE);
        if (pszDllPathRemote == nullptr) __leave;
        // ��������� ���� ��� ����� Dll
        if (!WriteProcessMemory(pi->hProcess, pszDllPathRemote,
            (PVOID)pszDllPath, iBufLen, nullptr)) __leave;
        // ������� ����� LoadLibrary
        PTHREAD_START_ROUTINE pfnThreadRtn = (PTHREAD_START_ROUTINE)
            GetProcAddress(GetModuleHandle(_TEXT("Kernel32")), 
#ifdef _UNICODE
                "LoadLibraryW"
#else
                "LoadLibraryA"
#endif
            );
        if (pfnThreadRtn == nullptr) __leave;
        // �������� ��������� ����� � ������� LoadLibrary(pszDllPath)
        hThread = CreateRemoteThread(pi->hProcess, nullptr, 0,
            pfnThreadRtn, pszDllPathRemote, 0, nullptr);
        if (hThread == nullptr) __leave;
        // ��������, ���� ����� �� ����������
        WaitForSingleObject(hThread, INFINITE);
        // ����������!
        result = TRUE;
    }
    __finally {
        // �������� �� �����
        // ���������� ������
        if (pszDllPathRemote)
            VirtualFreeEx(pi->hProcess, pszDllPathRemote, 0, MEM_RELEASE);
        if (hThread)
            CloseHandle(hThread);
    }
    return result;
}


int main(int argc, char* argv[])
{
    TCHAR szDllName[MAX_PATH] = _TEXT("LeakTest.dll");
    TCHAR szDllPath[MAX_PATH] = _TEXT("");
    TCHAR szCommandLine[MAX_PATH] = _TEXT("");
    size_t NumOfCharConverted = 0;

    if (argc > 1)
    {
#ifdef _UNICODE
        mbstowcs_s(&NumOfCharConverted, szCommandLine, sizeof(szCommandLine) / sizeof(WCHAR), argv[1], strlen(argv[1]));
#else
        lstrcpy(szCommandLine, argv[1]);
#endif
        //TODO: ���������, ��� ���� �� szCommandLine ����������

        // �������� ���� � dll
        if (GetModuleFileName(nullptr, szDllPath, sizeof(szDllPath) / sizeof(TCHAR)) > 0)
        {
            int iLen = lstrlen(szDllPath);
            while (szDllPath[--iLen] != _TEXT('\\')) { szDllPath[iLen] = 0; }
            lstrcat(szDllPath, szDllName);
        }
        //TODO: ��������� szDllPath

        // ���������
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
            std::cout << argv[1] << " successfully started.\n";
            // ������� ��������
            if (InjectLib(&pi, szDllPath))
            {
                std::cout << szDllPath << " injection successfull.\n";
            }
            // ��������� �����
            ::ResumeThread(pi.hThread);
            //TODO: ����� ��������� � ���-������ ������� ��������
            // EjectLib(dwProcessId, szLibFile)
        }
        else
        {
            DWORD dwError = GetLastError();
            std::cout << argv[1] << " failed: " << dwError << std::endl;
        }
    }
    else
    {
        PrintUsage();
        return 9001;
    }
    return 0;
}
