#pragma once

#include <winscard.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;
extern bool g_ProcessTerminating;

// Provider modules stay loaded: a provider may retain callbacks or worker threads.
// Loading is deferred until an API call, outside DllMain and the loader lock.
struct LocalProvider {
    HMODULE module = NULL;
    decltype(&SCardEstablishContext) establish = NULL;
    decltype(&SCardReleaseContext) release = NULL;
    decltype(&SCardListReadersA) listA = NULL;
    decltype(&SCardListReadersW) listW = NULL;
    decltype(&SCardConnectA) connectA = NULL;
    decltype(&SCardConnectW) connectW = NULL;
    decltype(&SCardDisconnect) disconnect = NULL;
    decltype(&SCardTransmit) transmit = NULL;

    bool load(const std::wstring& path) {
        // Require an absolute DLL path, never resolve from the current directory.
        const bool drive = path.size() > 3 && path[1] == L':' &&
            (path[2] == L'\\' || path[2] == L'/');
        const bool unc = path.size() > 2 && path[0] == L'\\' && path[1] == L'\\';
        if (!drive && !unc) return false;
        HMODULE loaded = LoadLibraryExW(path.c_str(), NULL,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!loaded) return false;
        if (loaded == reinterpret_cast<HMODULE>(&__ImageBase)) {
            FreeLibrary(loaded);
            return false;
        }
#define LOAD_API(field, name) field = reinterpret_cast<decltype(field)>(GetProcAddress(loaded, name))
        LOAD_API(establish, "SCardEstablishContext");
        LOAD_API(release, "SCardReleaseContext");
        LOAD_API(listA, "SCardListReadersA");
        LOAD_API(listW, "SCardListReadersW");
        LOAD_API(connectA, "SCardConnectA");
        LOAD_API(connectW, "SCardConnectW");
        LOAD_API(disconnect, "SCardDisconnect");
        LOAD_API(transmit, "SCardTransmit");
#undef LOAD_API
        if (!establish || !release || !listA || !listW || !connectA || !connectW ||
            !disconnect || !transmit) {
            FreeLibrary(loaded);
            return false;
        }
        module = loaded;
        return true;
    }

    LONG list(SCARDCONTEXT context, LPCSTR groups, LPSTR names, LPDWORD length) {
        return listA(context, groups, names, length);
    }
    LONG list(SCARDCONTEXT context, LPCWSTR groups, LPWSTR names, LPDWORD length) {
        return listW(context, groups, names, length);
    }
    LONG connect(SCARDCONTEXT context, LPCSTR name, DWORD share, DWORD protocols,
                 LPSCARDHANDLE card, LPDWORD protocol) {
        return connectA(context, name, share, protocols, card, protocol);
    }
    LONG connect(SCARDCONTEXT context, LPCWSTR name, DWORD share, DWORD protocols,
                 LPSCARDHANDLE card, LPDWORD protocol) {
        return connectW(context, name, share, protocols, card, protocol);
    }
};

struct LocalContext {
    std::mutex io;
    std::shared_ptr<LocalProvider> provider;
    SCARDCONTEXT handle = 0;
    bool active = false;
    // Native cleanup can involve IPC. At process exit the OS reclaims these
    // resources; calling a provider under the loader lock can deadlock.
    ~LocalContext() { if (active && !g_ProcessTerminating) provider->release(handle); }

    // Called with io held. Never request provider-owned AUTOALLOCATE memory.
    template<class C> std::vector<std::basic_string<C>> readers(const C* groups) {
        std::vector<std::basic_string<C>> result;
        if (!active) return result;
        DWORD length = 0;
        LONG rc = provider->list(handle, groups, static_cast<C*>(NULL), &length);
        if (rc != SCARD_S_SUCCESS || length < 2 || length > 1048576) return result;
        for (int attempt = 0; attempt != 3; ++attempt) {
            std::vector<C> buffer(length, 0);
            DWORD capacity = length;
            rc = provider->list(handle, groups, buffer.data(), &length);
            if (rc == SCARD_E_INSUFFICIENT_BUFFER && length > capacity && length <= 1048576)
                continue;
            if (rc != SCARD_S_SUCCESS || length > capacity || length < 2) return result;
            std::size_t start = 0;
            while (start < length && buffer[start]) {
                std::size_t end = start;
                while (end < length && buffer[end]) ++end;
                if (end == length) return {};
                result.emplace_back(buffer.data() + start, end - start);
                start = end + 1;
            }
            return result;
        }
        return result;
    }
};
