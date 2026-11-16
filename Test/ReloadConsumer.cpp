#include <windows.h>
#include <winscard.h>
#include <cstring>
#include <vector>

// Link against the SDK's WinSCard.lib so this has the same basename import
// as a decoder module, rather than resolving BonCasClient by absolute path.
extern "C" __declspec(dllexport) LONG WINAPI ProbeReaders() {
    SCARDCONTEXT context = 0;
    LONG rc = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, &context);
    if (rc != SCARD_S_SUCCESS) return rc;
    DWORD length = 0;
    rc = SCardListReadersA(context, NULL, NULL, &length);
    if (rc == SCARD_S_SUCCESS && length > 1 && length <= 1048576) {
        std::vector<char> names(length, 0);
        rc = SCardListReadersA(context, NULL, names.data(), &length);
        if (rc == SCARD_S_SUCCESS && std::strcmp(names.data(), "192.0.2.56:36999"))
            rc = SCARD_F_INTERNAL_ERROR;
    } else if (rc == SCARD_S_SUCCESS) rc = SCARD_F_INTERNAL_ERROR;
    SCardReleaseContext(context);
    return rc;
}
