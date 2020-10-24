#pragma once

void PrintImageImportW(HMODULE hmodCaller);
BOOL ReplaceIATEntryInOneMod(PCSTR pszCalleeModName, PROC pfnCurrent, PROC pfnNew, HMODULE hmodCaller);