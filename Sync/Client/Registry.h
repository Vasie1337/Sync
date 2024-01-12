#pragma once

bool CreateValueInRegistry(const wchar_t* keyPath, const wchar_t* valueName, std::uint64_t value)
{
    HKEY hKey;
    LONG createResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);

    if (createResult != ERROR_SUCCESS) {
        std::cerr << "Error creating registry key: " << createResult << std::endl;
        return false;
    }

    LONG setResult = RegSetValueEx(hKey, valueName, 0, REG_QWORD, reinterpret_cast<const BYTE*>(&value), sizeof(std::uint64_t));

    RegCloseKey(hKey);

    if (setResult != ERROR_SUCCESS) {
        std::cerr << "Error setting registry value: " << setResult << std::endl;
        return false;
    }

    return true;
}

bool Create32BitValueInRegistry(const wchar_t* keyPath, const wchar_t* valueName, std::uint32_t value)
{
    HKEY hKey;
    LONG createResult = RegCreateKeyEx(HKEY_LOCAL_MACHINE, keyPath, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);

    if (createResult != ERROR_SUCCESS) {
        std::cerr << "Error creating registry key: " << createResult << std::endl;
        return false;
    }

    LONG setResult = RegSetValueEx(hKey, valueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(std::uint32_t));

    RegCloseKey(hKey);

    if (setResult != ERROR_SUCCESS) {
        std::cerr << "Error setting registry value: " << setResult << std::endl;
        return false;
    }

    return true;
}