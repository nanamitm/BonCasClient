#include <windows.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

std::wstring absolute(const wchar_t* path) {
    wchar_t buffer[32768];
    const DWORD n = GetFullPathNameW(path, 32768, buffer, NULL);
    assert(n && n < 32768);
    return std::wstring(buffer, n);
}
HMODULE importOwner(HMODULE module) {
    const auto base = reinterpret_cast<const BYTE*>(module);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const DWORD imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    assert(imports);
    auto descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports);
    for (; descriptor->Name; ++descriptor) {
        if (_stricmp(reinterpret_cast<const char*>(base + descriptor->Name), "winscard.dll")) continue;
        const auto thunk = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + descriptor->FirstThunk);
        HMODULE owner = NULL;
        assert(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(thunk->u1.Function), &owner));
        return owner;
    }
    assert(!"WinSCard import missing");
    return NULL;
}
int wmain(int argc, wchar_t** argv) {
    assert(argc == 4);
    assert(!GetModuleHandleW(L"winscard.dll"));
    wchar_t scratch[32768];
    const DWORD length = GetEnvironmentVariableW(L"BONCAS_TEST_SCRATCH", scratch, 32768);
    assert(length && length < 32768);
    const std::wstring directory = std::wstring(scratch) + L"\\Reload-" + std::to_wstring(GetCurrentProcessId());
    assert(CreateDirectoryW(directory.c_str(), NULL));
    const std::wstring wrapper = directory + L"\\WinSCard.dll";
    const std::wstring consumer = directory + L"\\Consumer.dll";
    const std::wstring ini = directory + L"\\WinSCard.ini";
    assert(CopyFileW(absolute(argv[1]).c_str(), wrapper.c_str(), FALSE));
    assert(CopyFileW(absolute(argv[2]).c_str(), consumer.c_str(), FALSE));
    assert(WritePrivateProfileStringW(L"Server", L"IP", L"192.0.2.56", ini.c_str()));
    assert(WritePrivateProfileStringW(L"Server", L"Port", L"36999", ini.c_str()));
    assert(WritePrivateProfileStringW(L"Local", L"Enable", argv[3], ini.c_str()));
    HMODULE explicitReference = LoadLibraryW(wrapper.c_str());
    assert(explicitReference);
    for (int cycle = 0; cycle != 3; ++cycle) {
        HMODULE module = LoadLibraryExW(consumer.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        assert(module);
        const HMODULE owner = importOwner(module);
        if (owner != explicitReference) {
            wchar_t path[32768];
            GetModuleFileNameW(owner, path, 32768);
            std::wcerr << L"Import bypassed BonCasClient after reload: " << path << L"\n";
        }
        assert(owner == explicitReference);
        auto probe = reinterpret_cast<LONG(WINAPI*)()>(GetProcAddress(module, "ProbeReaders"));
#ifdef _M_IX86
        if (!probe) probe = reinterpret_cast<LONG(WINAPI*)()>(GetProcAddress(module, "_ProbeReaders@0"));
#endif
        assert(probe && probe() == 0);
        if (!cycle) assert(FreeLibrary(explicitReference));
        assert(FreeLibrary(module));
    }
    std::wcout << L"Import binding and network readers survived reload (Local.Enable=" << argv[3] << L")\n";
    // Pinned modules stay mapped until process exit; the runner cleans up.
}
