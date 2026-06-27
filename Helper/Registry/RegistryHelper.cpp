#include "RegistryHelper.h"

// ─── Helpers internes ────────────────────────────────────────────────────────

// Ouvre ou crée la clé, et ferme automatiquement via RAII.
struct RegKeyGuard {
    HKEY key = nullptr;
    ~RegKeyGuard() { if (key) RegCloseKey(key); }
};

// ─── DWORD ───────────────────────────────────────────────────────────────────

DWORD GetOrCreateDword(HKEY       hRootKey,
    const std::wstring& subKey,
    const std::wstring& valueName,
    DWORD      defaultValue)
{
    RegKeyGuard guard;

    // Ouvre la clé (la crée si elle n'existe pas)
    LONG res = RegCreateKeyExW(
        hRootKey, subKey.c_str(),
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE, nullptr,
        &guard.key, nullptr);

    if (res != ERROR_SUCCESS)
        throw std::runtime_error("RegCreateKeyEx failed: " + std::to_string(res));

    // Tente de lire la valeur
    DWORD value = 0;
    DWORD dataSize = sizeof(DWORD);
    DWORD type = 0;

    res = RegQueryValueExW(
        guard.key, valueName.c_str(),
        nullptr, &type,
        reinterpret_cast<LPBYTE>(&value), &dataSize);

    if (res == ERROR_SUCCESS && type == REG_DWORD)
        return value;   // ✅ valeur existante

    // Valeur absente → on l'écrit avec la valeur par défaut
    res = RegSetValueExW(
        guard.key, valueName.c_str(),
        0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&defaultValue), sizeof(DWORD));

    if (res != ERROR_SUCCESS)
        throw std::runtime_error("RegSetValueEx failed: " + std::to_string(res));

    return defaultValue;
}

// ─── Chaîne REG_SZ ───────────────────────────────────────────────────────────

std::wstring GetOrCreateString(HKEY       hRootKey,
    const std::wstring& subKey,
    const std::wstring& valueName,
    const std::wstring& defaultValue)
{
    RegKeyGuard guard;

    LONG res = RegCreateKeyExW(
        hRootKey, subKey.c_str(),
        0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_READ | KEY_WRITE, nullptr,
        &guard.key, nullptr);

    if (res != ERROR_SUCCESS)
        throw std::runtime_error("RegCreateKeyEx failed: " + std::to_string(res));

    // Premier appel : obtenir la taille nécessaire
    DWORD dataSize = 0;
    DWORD type = 0;

    res = RegQueryValueExW(guard.key, valueName.c_str(),
        nullptr, &type, nullptr, &dataSize);

    if (res == ERROR_SUCCESS && type == REG_SZ && dataSize > 0) {
        // Deuxième appel : lire les données
        std::wstring value(dataSize / sizeof(wchar_t), L'\0');
        res = RegQueryValueExW(guard.key, valueName.c_str(),
            nullptr, nullptr,
            reinterpret_cast<LPBYTE>(value.data()), &dataSize);
        if (res == ERROR_SUCCESS) {
            // Supprimer le null-terminator inclus dans dataSize
            if (!value.empty() && value.back() == L'\0')
                value.pop_back();
            return value;   // ✅ valeur existante
        }
    }

    // Valeur absente → écriture avec la valeur par défaut
    DWORD writeSize = static_cast<DWORD>(
        (defaultValue.size() + 1) * sizeof(wchar_t)); // +1 pour \0

    res = RegSetValueExW(
        guard.key, valueName.c_str(),
        0, REG_SZ,
        reinterpret_cast<const BYTE*>(defaultValue.c_str()), writeSize);

    if (res != ERROR_SUCCESS)
        throw std::runtime_error("RegSetValueEx failed: " + std::to_string(res));

    return defaultValue;
}