# Regression tests

From an x64 or x86 Visual Studio developer PowerShell with the v143 C++
toolset and Windows SDK installed:

```powershell
./Test/Run.ps1 -Platform x64
# In the x86 developer shell:
./Test/Run.ps1 -Platform Win32
```

The tests build the DLL, load a temporary copy with an isolated INI, and
use loopback TCP servers. No card reader or BonCasServer is required.
They cover send/receive timeout settings, synchronous and asynchronous
connections, refused connections, independent endpoint backoff, legacy
`[Server]` / `[Server:Port]` enumeration, ANSI/Unicode list buffer guards,
automatic allocation ownership, invalid command sizes, response buffer
guards, and context release while a transmit is in flight.

The workflow runs this suite for both architectures. Hardware/decoder
compatibility still needs to be checked with real applications.

The transport fixes and endpoint backoff are adapted from stuayu/BonCasClient:

- https://github.com/stuayu/BonCasClient/commit/68e722ad5db4699785c754d08cad08edb62c33f1
- https://github.com/stuayu/BonCasClient/commit/7494b45d4ac2b4e5734c85d791c39a7f90cb1a26
- https://github.com/stuayu/BonCasClient/commit/3094d9887e1c0644778bf00b7be1810e7e426ddd

The API guards and registry are implemented against this fork's existing
endpoint-name configuration. Rust, the `[Readers]` format, and additional
WinSCard exports are not included. `SetConfig` retains this fork's existing
no-op behavior.

## Local WinSCard provider tests

`Run.ps1` also builds `LocalReader.dll` and `LocalProviderTest.exe`. Each
scenario runs in a separate process with an isolated UTF-16 INI and a DLL
filename containing Unicode characters. The tests cover A/W enumeration,
deduplication, automatic allocation, share/protocol forwarding, APDUs over
255 bytes, native handle translation, provider errors, and disconnect or
context release during a transmit. The original network tests also run
with the local provider enabled.

Failure scenarios include disabled configuration, missing/relative/self
paths, a DLL without the required exports, failed context establishment,
empty/failed enumeration, and recursive calls back into BonCasClient.
No extra WinSCard exports beyond the existing nine APIs are required by
this feature; enumeration uses caller-owned buffers in the provider.

To check a locally built SCardHub of matching architecture after running
the suite, use the following command (x64 example):

```powershell
./artifacts/LocalProviderTest.exe x64/Release/BonCasClient.dll artifacts/LocalReader.dll hub C:\WinSCard.dll
```

This stages a copy of SCardHub with an isolated `SCardHub.ini`, and routes
enumeration, connection, and APDUs through SCardHub into the mock reader.
It does not change the original SCardHub configuration or use real cards.
Provider modules can remain mapped until process exit; temporary provider
files are therefore removed on a best-effort basis.

## DLL unload/reload regression

`ReloadConsumer.dll` links against the SDK's `WinSCard.lib`, as a decoder
would. `ReloadTest.exe` stages BonCasClient as `WinSCard.dll`, releases the
explicit DLL reference and the consumer module, and loads the consumer
again for three cycles. Each cycle checks the actual import address table
owner and verifies that the network reader is still enumerated. The test
runs with local forwarding disabled and with the system provider enabled.
It fails on the unpinned implementation when the second consumer binds to
the system WinSCard DLL instead of BonCasClient.

BonCasClient pins itself on the first valid `SCardEstablishContext` call.
Pinned modules remain mapped until process exit, so the runner removes
the isolated test directories after each child process has exited. Native
provider cleanup is skipped during process termination to avoid IPC while
Windows holds the loader lock. The `exit-active` scenario leaves a native
context open and fails the process if its release function is called at
termination.
