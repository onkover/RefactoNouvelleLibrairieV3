#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

// Lit une valeur DWORD ; la crée avec defaultValue si absente.
DWORD GetOrCreateDword(HKEY hRootKey,
    const std::wstring& subKey,
    const std::wstring& valueName,
    DWORD defaultValue);

// Lit une valeur REG_SZ ; la crée avec defaultValue si absente.
std::wstring GetOrCreateString(HKEY hRootKey,
    const std::wstring& subKey,
    const std::wstring& valueName,
    const std::wstring& defaultValue);