#include <winsock2.h>
#include <windows.h>
#include <winscard.h>
#include <cassert>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

template<class T> T function(HMODULE module, const char* name) {
    auto address = GetProcAddress(module, name);
    assert(address);
    return reinterpret_cast<T>(address);
}
std::wstring absolute(const wchar_t* path) {
    wchar_t buffer[32768];
    const DWORD n = GetFullPathNameW(path, 32768, buffer, NULL);
    assert(n && n < 32768);
    return std::wstring(buffer, n);
}
void textFile(const std::wstring& path, const std::wstring& text) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    const std::wstring data = L"\xFEFF" + text;
    DWORD written = 0;
    assert(WriteFile(file, data.data(), static_cast<DWORD>(data.size() * sizeof(wchar_t)), &written, NULL));
    assert(written == data.size() * sizeof(wchar_t));
    CloseHandle(file);
}

int wmain(int argc, wchar_t** argv) {
    assert(argc == 4 || argc == 5);
    const std::wstring scenario(argv[3]);
    const bool hub = scenario == L"hub";
    wchar_t temp[32768];
    const DWORD scratchLength = GetEnvironmentVariableW(L"BONCAS_TEST_SCRATCH", temp, 32768);
    std::wstring root;
    if (scratchLength && scratchLength < 32768) root.assign(temp, scratchLength);
    else { assert(GetTempPathW(32768, temp)); root = temp; }
    const std::wstring directory = root + L"\\BonCasLocal-" + std::to_wstring(GetCurrentProcessId());
    assert(CreateDirectoryW(directory.c_str(), NULL));
    const std::wstring dllPath = directory + L"\\BonCasClient.dll";
    const std::wstring readerPath = directory + L"\\\u30ea\u30fc\u30c0\u30fc.dll";
    const std::wstring iniPath = directory + L"\\BonCasClient.ini";
    assert(CopyFileW(absolute(argv[1]).c_str(), dllPath.c_str(), FALSE));
    assert(CopyFileW(absolute(argv[2]).c_str(), readerPath.c_str(), FALSE));
    HMODULE fixture = LoadLibraryW(readerPath.c_str());
    assert(fixture);
    auto mode = function<LONG(WINAPI*)(LONG)>(fixture, "FixtureMode");
    auto count = function<LONG(WINAPI*)(LONG)>(fixture, "FixtureCount");
    mode(scenario == L"establish-failed" ? 1 : scenario == L"empty" ? 2 :
        scenario == L"list-failed" ? 3 : scenario == L"cycle" ? 4 :
        scenario == L"exit-active" ? 6 : hub ? 5 : 0);
    std::wstring configured = readerPath;
    if (scenario == L"missing") configured += L".missing";
    if (scenario == L"relative") configured = L"reader.dll";
    if (scenario == L"self") configured = dllPath;
    if (scenario == L"invalid") {
        wchar_t system[MAX_PATH];
        assert(GetSystemDirectoryW(system, MAX_PATH));
        configured = std::wstring(system) + L"\\kernel32.dll";
    }
    if (hub) {
        assert(argc == 5);
        configured = directory + L"\\WinSCard.dll";
        assert(CopyFileW(absolute(argv[4]).c_str(), configured.c_str(), FALSE));
        textFile(directory + L"\\SCardHub.ini", L"[option]\r\nreader_dlls=" + readerPath + L"\r\n");
    }
    textFile(iniPath, L"[Local]\r\nEnable=" + std::wstring(scenario == L"disabled" ? L"0" : L"1") +
        L"\r\nPath=" + configured + L"\r\n");
    HMODULE dll = LoadLibraryW(dllPath.c_str());
    assert(dll);
    auto establish = function<decltype(&SCardEstablishContext)>(dll, "CasLinkEstablishContext");
    auto release = function<decltype(&SCardReleaseContext)>(dll, "CasLinkReleaseContext");
    auto listA = function<decltype(&SCardListReadersA)>(dll, "CasLinkListReadersA");
    auto listW = function<decltype(&SCardListReadersW)>(dll, "CasLinkListReadersW");
    auto freeMemory = function<decltype(&SCardFreeMemory)>(dll, "CasLinkFreeMemory");
    auto connectA = function<decltype(&SCardConnectA)>(dll, "CasLinkConnectA");
    auto connectW = function<decltype(&SCardConnectW)>(dll, "CasLinkConnectW");
    auto transmit = function<decltype(&SCardTransmit)>(dll, "CasLinkTransmit");
    auto disconnect = function<decltype(&SCardDisconnect)>(dll, "CasLinkDisconnect");
    if (scenario == L"cycle") {
        auto parent = function<LONG(WINAPI*)(decltype(establish))>(fixture, "FixtureParent");
        parent(establish);
    }
    SCARDCONTEXT context = 0, other = 0;
    assert(establish(SCARD_SCOPE_USER, NULL, NULL, &context) == SCARD_S_SUCCESS);
    if (scenario == L"exit-active") {
        assert(count(0) == 1);
        std::wcout << L"Leaving a native context active for the process termination test\n";
        return 0; // The provider would exit with code 99 if its release were called.
    }
    DWORD length = 0;
    assert(listW(context, NULL, NULL, &length) == SCARD_S_SUCCESS);
    std::vector<wchar_t> names(length);
    assert(listW(context, NULL, names.data(), &length) == SCARD_S_SUCCESS);
    assert(std::wstring(names.data()) == L"127.0.0.1:6900");
    bool found = false;
    unsigned duplicates = 0;
    for (const wchar_t* entry = names.data(); *entry; entry += wcslen(entry) + 1) {
        found |= std::wstring(entry) == L"Mock local";
        duplicates += std::wstring(entry) == L"127.0.0.1:6900";
    }
    assert(duplicates == 1);
    const bool usable = scenario == L"success" || scenario == L"disconnect" || hub;
    assert(found == usable);
    if (usable) {
        // The W variant preserves Unicode reader names; A is independently queried.
        if (!hub) {
            bool unicode = false;
            for (const wchar_t* entry = names.data(); *entry; entry += wcslen(entry) + 1)
                unicode |= std::wstring(entry) == L"\u30ea\u30fc\u30c0\u30fc";
            assert(unicode);
        }
        char* ansi = NULL;
        DWORD automatic = SCARD_AUTOALLOCATE;
        assert(listA(context, NULL, reinterpret_cast<char*>(&ansi), &automatic) == SCARD_S_SUCCESS);
        bool ansiFound = false;
        for (const char* entry = ansi; *entry; entry += strlen(entry) + 1)
            ansiFound |= std::string(entry) == "Mock local";
        assert(ansiFound);
        assert(freeMemory(context, ansi) == SCARD_S_SUCCESS);
        wchar_t output[2] = {L'X', L'Y'};
        DWORD capacity = 1;
        assert(listW(context, NULL, output, &capacity) == SCARD_E_INSUFFICIENT_BUFFER);
        assert(output[0] == L'X' && output[1] == L'Y' && capacity == length);
        assert(establish(SCARD_SCOPE_USER, NULL, NULL, &other) == SCARD_S_SUCCESS && other != context);
        SCARDHANDLE first = 0, second = 0;
        DWORD protocol = 0;
        const DWORD requested = hub ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0;
        const DWORD share = hub ? SCARD_SHARE_SHARED : SCARD_SHARE_EXCLUSIVE;
        assert(connectA(context, "Mock local", share, requested, &first, &protocol) == SCARD_S_SUCCESS);
        assert(protocol == requested);
        assert(connectW(other, L"Mock local", share, requested, &second, &protocol) == SCARD_S_SUCCESS);
        assert(first != second && protocol == requested);
        BYTE command[512] = {}, response[3] = {0xAA, 0xBB, 0xCC};
        SCARD_IO_REQUEST pci = {requested, sizeof(pci)};
        SCARD_IO_REQUEST received = {0, sizeof(received)};
        capacity = 1;
        assert(transmit(first, &pci, command, sizeof(command), &received, response, &capacity) == SCARD_E_INSUFFICIENT_BUFFER);
        assert(capacity == 2 && response[0] == 0xAA && response[1] == 0xBB && response[2] == 0xCC);
        capacity = sizeof(response);
        assert(transmit(first, &pci, command, sizeof(command), &received, response, &capacity) == SCARD_S_SUCCESS);
        assert(capacity == 2 && response[0] == 0x90 && response[1] == 0 && response[2] == 0xCC);
        assert(received.dwProtocol == requested);
        if (!hub) {
            assert(disconnect(first, SCARD_LEAVE_CARD) == SCARD_E_INVALID_VALUE);
            assert(count(1) == 2); // Failed disconnect leaves the wrapper usable.
        }
        assert(disconnect(first, SCARD_RESET_CARD) == SCARD_S_SUCCESS);
        assert(disconnect(first, SCARD_RESET_CARD) == SCARD_E_INVALID_HANDLE);
        if (!hub) {
            command[0] = 0xFA;
            std::thread call([&] {
                DWORD size = sizeof(response);
                assert(transmit(second, &pci, command, sizeof(command), NULL, response, &size) == SCARD_S_SUCCESS);
            });
            const ULONGLONG deadline = GetTickCount64() + 4000;
            while (!count(2) && GetTickCount64() < deadline) Sleep(1);
            assert(count(2));
            if (scenario == L"disconnect") assert(disconnect(second, SCARD_RESET_CARD) == SCARD_S_SUCCESS);
            else assert(release(other) == SCARD_S_SUCCESS);
            call.join();
            if (scenario == L"disconnect") assert(release(other) == SCARD_S_SUCCESS);
        } else assert(release(other) == SCARD_S_SUCCESS);
        assert(disconnect(second, SCARD_RESET_CARD) == SCARD_E_INVALID_HANDLE);
    }
    assert(release(context) == SCARD_S_SUCCESS);
    if (!hub) assert(count(0) == 0 && count(1) == 0);
    assert(FreeLibrary(dll));
    FreeLibrary(fixture);
    DeleteFileW(iniPath.c_str());
    DeleteFileW(dllPath.c_str());
    // The provider can deliberately pin itself (SCardHub does). Its file may
    // stay mapped until process exit; cleanup is best effort for that file.
    DeleteFileW(readerPath.c_str());
    if (hub) {
        DeleteFileW((directory + L"\\SCardHub.ini").c_str());
        DeleteFileW(configured.c_str());
    }
    RemoveDirectoryW(directory.c_str());
    std::wcout << L"Local provider test passed: " << scenario << L"\n";
}
