#include <winsock2.h>
#include <windows.h>
#include <winscard.h>
#include <map>
#include <set>
#include <mutex>
#include <string>
#include <atomic>

namespace {
std::mutex lock;
std::set<SCARDCONTEXT> contexts;
std::map<SCARDHANDLE, SCARDCONTEXT> cards;
ULONG_PTR nextContext = 10, nextCard = 100;
std::atomic<LONG> busy(0);
LONG mode = 0;
decltype(&SCardEstablishContext) parent = NULL;
}
#define API extern "C" __declspec(dllexport) LONG WINAPI
API FixtureMode(LONG value) { mode = value; return 0; }
API FixtureParent(decltype(&SCardEstablishContext) callback) { parent = callback; return 0; }
API FixtureCount(LONG kind) {
    std::lock_guard<std::mutex> guard(lock);
    return kind == 0 ? static_cast<LONG>(contexts.size()) :
        kind == 1 ? static_cast<LONG>(cards.size()) : busy.load();
}
API MockEstablish(DWORD scope, LPCVOID, LPCVOID, LPSCARDCONTEXT result) {
    if (mode == 1) return SCARD_E_NO_SERVICE;
    if (mode == 4 && parent) return parent(scope, NULL, NULL, result);
    if (scope != SCARD_SCOPE_USER && scope != SCARD_SCOPE_SYSTEM) return SCARD_E_INVALID_VALUE;
    std::lock_guard<std::mutex> guard(lock);
    *result = nextContext++;
    contexts.insert(*result);
    return SCARD_S_SUCCESS;
}
API MockRelease(SCARDCONTEXT context) {
    std::lock_guard<std::mutex> guard(lock);
    if (!contexts.erase(context)) return SCARD_E_INVALID_HANDLE;
    for (auto it = cards.begin(); it != cards.end();) {
        if (it->second == context) it = cards.erase(it);
        else ++it;
    }
    return SCARD_S_SUCCESS;
}
std::string readerNames(char*) {
    const char names[] = "Mock local\0\x83\x8A\x81\x5B\x83\x5F\x81\x5B\0";
    return std::string(names, sizeof(names));
}
std::wstring readerNames(wchar_t*) {
    const wchar_t names[] = L"Mock local\0\u30ea\u30fc\u30c0\u30fc\0";
    return std::wstring(names, sizeof(names) / sizeof(wchar_t));
}
template<class C> LONG list(SCARDCONTEXT context, C* output, LPDWORD length) {
    std::lock_guard<std::mutex> guard(lock);
    if (!contexts.count(context)) return SCARD_E_INVALID_HANDLE;
    if (mode == 2) return SCARD_E_NO_READERS_AVAILABLE;
    if (mode == 3) return SCARD_F_COMM_ERROR;
    std::basic_string<C> data = readerNames(static_cast<C*>(NULL));
    // Include a network name to exercise precedence and deduplication.
    const char duplicate[] = "127.0.0.1:6900";
    std::basic_string<C> dup(duplicate, duplicate + sizeof(duplicate));
    data.insert(data.size() - 1, dup);
    const DWORD capacity = *length;
    *length = static_cast<DWORD>(data.size());
    if (!output) return SCARD_S_SUCCESS;
    if (capacity < *length) return SCARD_E_INSUFFICIENT_BUFFER;
    CopyMemory(output, data.data(), data.size() * sizeof(C));
    return SCARD_S_SUCCESS;
}
API MockListA(SCARDCONTEXT context, LPCSTR, LPSTR output, LPDWORD length) {
    return list(context, output, length);
}
API MockListW(SCARDCONTEXT context, LPCWSTR, LPWSTR output, LPDWORD length) {
    return list(context, output, length);
}
template<class C> LONG connect(SCARDCONTEXT context, const C* name, DWORD share, DWORD protocols,
                              LPSCARDHANDLE result, LPDWORD protocol) {
    std::lock_guard<std::mutex> guard(lock);
    if (!contexts.count(context)) return SCARD_E_INVALID_HANDLE;
    if (!name || (name[0] != 'M' && name[0] != 0x30EA && name[0] != static_cast<C>(0x83)))
        return SCARD_E_UNKNOWN_READER;
    if (share != static_cast<DWORD>(mode == 5 ? SCARD_SHARE_SHARED : SCARD_SHARE_EXCLUSIVE) ||
        protocols != static_cast<DWORD>(mode == 5 ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0))
        return SCARD_E_PROTO_MISMATCH;
    *result = nextCard++;
    cards[*result] = context;
    *protocol = mode == 5 ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0;
    return SCARD_S_SUCCESS;
}
API MockConnectA(SCARDCONTEXT context, LPCSTR name, DWORD share, DWORD protocols,
                 LPSCARDHANDLE result, LPDWORD protocol) {
    return connect(context, name, share, protocols, result, protocol);
}
API MockConnectW(SCARDCONTEXT context, LPCWSTR name, DWORD share, DWORD protocols,
                 LPSCARDHANDLE result, LPDWORD protocol) {
    return connect(context, name, share, protocols, result, protocol);
}
API MockDisconnect(SCARDHANDLE card, DWORD disposition) {
    std::lock_guard<std::mutex> guard(lock);
    if (disposition != SCARD_RESET_CARD && mode != 5) return SCARD_E_INVALID_VALUE;
    return cards.erase(card) ? SCARD_S_SUCCESS : SCARD_E_INVALID_HANDLE;
}
API MockTransmit(SCARDHANDLE card, LPCSCARD_IO_REQUEST pci, LPCBYTE send, DWORD sendLength,
                  LPSCARD_IO_REQUEST receivePci, LPBYTE output, LPDWORD length) {
    {
        std::lock_guard<std::mutex> guard(lock);
        if (!cards.count(card)) return SCARD_E_INVALID_HANDLE;
    }
    if (!pci || pci->dwProtocol != static_cast<DWORD>(mode == 5 ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0) || pci->cbPciLength != sizeof(*pci))
        return SCARD_E_PROTO_MISMATCH;
    if (sendLength != 512) return SCARD_E_INVALID_PARAMETER;
    if (*length < 2) { *length = 2; return SCARD_E_INSUFFICIENT_BUFFER; }
    if (send[0] == 0xFA) { busy = 1; Sleep(150); busy = 0; }
    output[0] = 0x90; output[1] = 0;
    *length = 2;
    if (receivePci) { receivePci->dwProtocol = mode == 5 ? SCARD_PROTOCOL_T1 : SCARD_PROTOCOL_T0; receivePci->cbPciLength = sizeof(*receivePci); }
    return SCARD_S_SUCCESS;
}
