// BonCasClient.cpp : DLL アプリケーションのエントリ ポイントを定義します。
//

#include "stdafx.h"
#include <WinScard.h>
#include "CasProxy.h"

#include <WS2tcpip.h>

#include <stdlib.h>
#include <wchar.h>
#include <stdarg.h>
#include <tchar.h>

#include <sstream>
#include <string>

#ifdef _MANAGED
#pragma managed(push, off)
#endif

#define __SPRITTER_CHAR_A	':'
#define __SPRITTER_CHAR_W	L':'
#define __SPRITTER_CHAR		_T(__SPRITTER_CHAR_A)


using namespace std;

string g_ReaderTableA;
wstring g_ReaderTableW;

typedef basic_string<TCHAR> tstring;

std::wstring convString(const std::string& input)
{
	size_t i;
	wchar_t* buffer = new wchar_t[input.size() + 1];
	mbstowcs_s(&i, buffer, input.size() + 1, input.c_str(), _TRUNCATE);
	std::wstring result = buffer;
	delete[] buffer;
	return result;
}

std::string convString(const std::wstring& input)
{
	size_t i;
	char* buffer = new char[input.size() * MB_CUR_MAX + 1];
	wcstombs_s(&i, buffer, input.size() * MB_CUR_MAX + 1, input.c_str(), _TRUNCATE);
	std::string result = buffer;
	delete[] buffer;
	return result;
}


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch(ul_reason_for_call){
		case DLL_PROCESS_ATTACH:
#ifdef _DEBUG
		// メモリリーク検出有効
			::_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | ::_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG)); 
#endif
			{
				TCHAR szDllPath[_MAX_PATH] = _T("");
				if (::GetModuleFileName(hModule, szDllPath, _MAX_PATH) != 0) {
					TCHAR szDrive[_MAX_DRIVE];
					TCHAR szDir[_MAX_DIR];
					TCHAR szFName[_MAX_FNAME];
					_tsplitpath_s(szDllPath, szDrive, _MAX_DRIVE, szDir, _MAX_DIR, szFName, _MAX_FNAME, NULL, 0);

					TCHAR szIniPath[_MAX_PATH] = _T("");
					_tmakepath_s(szIniPath, _MAX_PATH, szDrive, szDir, szFName, _T("ini"));

					TCHAR szBuff[64] = _T("");
					::GetPrivateProfileString(_T("Server"), _T("IP"), _T("127.0.0.1"), szBuff, _countof(szBuff), szIniPath);
					DWORD wPort = ::GetPrivateProfileInt(_T("Server"), _T("Port"), 6900UL, szIniPath);

					ostringstream ReaderTableA;
					wostringstream ReaderTableW;

#ifdef _UNICODE
					ReaderTableA << convString(wstring(szBuff)) << __SPRITTER_CHAR_A << wPort << '\0';
					ReaderTableW << szBuff << __SPRITTER_CHAR_W << wPort << L'\0';
#elif
					ReaderTableA << szBuff << __SPRITTER_CHAR_A << wPort << '\0';
					ReaderTableW << convString(string(szBuff)) << __SPRITTER_CHAR_W << wPort << L'\0';
#endif
					//以降拡張セクション
					for (int i = 0; i < 99; i++)
					{
						TCHAR key[16];
						TCHAR buf[1024];

						_stprintf_s(key, _countof(key), _T("Server%02d"), i);

						if( ::GetPrivateProfileString(_T("Server:Port"), key, NULL, buf, _countof(buf), szIniPath) == 0) break;// Keyが見つからなければ終了

						LPCTSTR pszPortStr = _tcsrchr(buf, __SPRITTER_CHAR);
						int port_num;
						if (pszPortStr == NULL || (port_num = _tstoi(pszPortStr + 1)) < 0 || port_num > USHRT_MAX) continue;// 不正な形式orポート番号なら読み飛ばす

						tstring server_name = tstring(buf, (int)(pszPortStr - buf));

#ifdef _UNICODE
						ReaderTableA << convString(server_name) << __SPRITTER_CHAR_A << port_num << '\0';
						ReaderTableW << server_name << __SPRITTER_CHAR_W << port_num << L'\0';
#else
						ReaderTableA << server_name << __SPRITTER_CHAR_A << port_num << '\0';
						ReaderTableW << convString(server_name) << __SPRITTER_CHAR_W << port_num << L'\0';
#endif
					}
					ReaderTableA << '\0';// 終端子
					ReaderTableW << L'\0';

					g_ReaderTableA = ReaderTableA.str();
					g_ReaderTableW = ReaderTableW.str();
				}
			}
			break;

		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL WINAPI SetConfig(const DWORD dwIP, const WORD wPort)
{
	return TRUE;
}

LONG CasLinkConnect(LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol, DWORD dwIP, WORD wPort)
{
	// プロキシインスタンス生成
	CCasProxy *pCasProxy = new CCasProxy(NULL);

	// IP,Port設定
	pCasProxy->Setting(dwIP, wPort);
	
	// サーバに接続
	if (!pCasProxy->Connect()) {
		delete pCasProxy;
		*phCard = NULL;
		return SCARD_E_READER_UNAVAILABLE;
	}

	// ハンドルに埋め込む
	*phCard = reinterpret_cast<SCARDHANDLE>(pCasProxy);
	if (pdwActiveProtocol)	*pdwActiveProtocol = SCARD_PROTOCOL_T1;

	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkConnectA(SCARDCONTEXT hContext, LPCSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LPCSTR p = g_ReaderTableA.c_str();

	while (*p)
	{
		if (strcmp(p, szReader) == 0)
		{
			LPCSTR port_str = strrchr(p, __SPRITTER_CHAR_A);
			int port_num;
			if (port_str == NULL || (port_num = atoi(port_str + 1)) < 0 || port_num > USHRT_MAX) return SCARD_E_READER_UNAVAILABLE;

			string server = string(p, (int)(port_str - p));
			DWORD ip = 0;

			if (::InetPtonA(AF_INET, server.c_str(), &ip) != 1) return SCARD_E_READER_UNAVAILABLE;

			return CasLinkConnect(phCard, pdwActiveProtocol, htonl(ip), port_num);
		}
		p += strlen(p) + 1;
	}
	return SCARD_E_READER_UNAVAILABLE;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkConnectW(SCARDCONTEXT hContext, LPCWSTR szReader, DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard, LPDWORD pdwActiveProtocol)
{
	LPCWSTR p = g_ReaderTableW.c_str();

	while (*p)
	{
		if (wcscmp(p, szReader) == 0)
		{
			LPCWSTR port_str = wcsrchr(p, __SPRITTER_CHAR_W);
			int port_num;
			if (port_str == NULL || (port_num = _wtoi(port_str + 1)) < 0 || port_num > USHRT_MAX) return SCARD_E_READER_UNAVAILABLE;

			wstring server = wstring(p, (int)(port_str - p));
			DWORD ip = 0;

			if (::InetPtonW(AF_INET, server.c_str(), &ip) != 1) return SCARD_E_READER_UNAVAILABLE;

			return CasLinkConnect(phCard, pdwActiveProtocol, htonl(ip), port_num);
		}
		p += wcslen(p) + 1;
	}
	return SCARD_E_READER_UNAVAILABLE;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	// サーバから切断
	CCasProxy *pCasProxy = reinterpret_cast<CCasProxy *>(hCard);
	if (pCasProxy)	delete pCasProxy;

	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkEstablishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkFreeMemory(SCARDCONTEXT hContext, LPCVOID pvMem)
{
	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkListReadersA(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders)
{
	if(pcchReaders){
		if ((*pcchReaders == SCARD_AUTOALLOCATE) && mszReaders) {
			*((LPCSTR*)mszReaders) = g_ReaderTableA.c_str();
			return SCARD_S_SUCCESS;
		}else{
			*pcchReaders = (DWORD)g_ReaderTableA.length();//文字数を返す
		}
	}

	if (mszReaders && pcchReaders)
	{
		DWORD len = (*pcchReaders < g_ReaderTableA.length()) ? *pcchReaders : (DWORD)g_ReaderTableA.length();

		::CopyMemory(mszReaders, g_ReaderTableA.c_str(), len * sizeof(char));
	}

	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkListReadersW(SCARDCONTEXT hContext, LPCWSTR mszGroups, LPWSTR mszReaders, LPDWORD pcchReaders)
{
	if (pcchReaders) {
		if ((*pcchReaders == SCARD_AUTOALLOCATE) && mszReaders) {
			*((LPCWSTR*)mszReaders) = g_ReaderTableW.c_str();
			return SCARD_S_SUCCESS;
		}
		else {
			*pcchReaders = (DWORD)g_ReaderTableW.length();//文字数を返す
		}
	}

	if (mszReaders && pcchReaders)
	{
		DWORD len = (*pcchReaders < g_ReaderTableW.length()) ? *pcchReaders : (DWORD)g_ReaderTableW.length();
		
		::CopyMemory(mszReaders, g_ReaderTableW.c_str(), len * sizeof(wchar_t));
	}

	return SCARD_S_SUCCESS;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci, LPCBYTE pbSendBuffer, DWORD cbSendLength, LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer, LPDWORD pcbRecvLength)
{
	// サーバにリクエスト送受信
	CCasProxy *pCasProxy = reinterpret_cast<CCasProxy *>(hCard);
	if (!pCasProxy)	return SCARD_E_READER_UNAVAILABLE;

	const DWORD dwRecvLen = pCasProxy->TransmitCommand(pbSendBuffer, cbSendLength, pbRecvBuffer);
	if (pcbRecvLength)	*pcbRecvLength = dwRecvLen;

	return (dwRecvLen)? SCARD_S_SUCCESS : SCARD_E_TIMEOUT;
}

extern "C" __declspec(dllexport) LONG WINAPI CasLinkReleaseContext(SCARDCONTEXT hContext)
{
	return SCARD_S_SUCCESS;
}


#ifdef _MANAGED
#pragma managed(pop)
#endif

