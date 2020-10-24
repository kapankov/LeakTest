#pragma once

BOOL LoadSymbols();
void UnloadSymbols();
std::wstring GetCallStack(const wchar_t* prefix = L"");