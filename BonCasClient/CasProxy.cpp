// CasProxy.cpp: CCasProxy クラスのインプリメンテーション
//
//////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "CasProxy.h"

#include <sysinfoapi.h>


#define WM_BONCASPROXY_EVENT		(WM_USER + 100U)
#define TCP_TIMEOUT					1000UL				// 1秒


ULONGLONG CCasProxy::m_ErrorDelayTime = 0ULL;


CCasProxy::CCasProxy(const HWND hProxyHwnd)
	: m_hProxyHwnd(hProxyHwnd)
	, m_dwIP(0x7F000001UL)
	, m_wPort(6900UL)
{

}

CCasProxy::~CCasProxy(void)
{
	// サーバから切断
	m_Socket.Close();
}

const void CCasProxy::Setting(const DWORD dwIP, const WORD wPort)
{
	m_dwIP = dwIP;
	m_wPort = wPort;
}

const BOOL CCasProxy::Connect(void)
{
	// エラー発生時のガードインターバル
	if(m_ErrorDelayTime){
		if((::GetTickCount64() - m_ErrorDelayTime) < TCP_TIMEOUT)return FALSE;
		else m_ErrorDelayTime = 0ULL;
		}

	// サーバに接続
	//if(m_Socket.Connect(SendProxyEvent(CPEI_GETSERVERIP), (WORD)SendProxyEvent(CPEI_GETSERVERPORT), TCP_TIMEOUT)){
	if(m_Socket.Connect(m_dwIP, m_wPort, TCP_TIMEOUT)){
//		SendProxyEvent(CPEI_CONNECTSUCCESS);
		return TRUE;
		}
	else{
		m_ErrorDelayTime = ::GetTickCount64();
//		SendProxyEvent(CPEI_CONNECTFAILED);
		return FALSE;
		}
}

const DWORD CCasProxy::TransmitCommand(const BYTE *pSendData, const DWORD dwSendSize, BYTE *pRecvData)
{
	// 送信データ準備
	BYTE SendBuf[256]{};
	SendBuf[0] = (BYTE)dwSendSize;
	::CopyMemory(&SendBuf[1], pSendData, dwSendSize);

	try{
		// リクエスト送信
		if(!m_Socket.Send(SendBuf, dwSendSize + 1UL, TCP_TIMEOUT))throw (const DWORD)__LINE__;
	
		// レスポンスヘッダ受信
		if(!m_Socket.Recv(SendBuf, 1UL, TCP_TIMEOUT))throw (const DWORD)__LINE__;

		// レスポンスデータ受信
		if(!m_Socket.Recv(pRecvData, SendBuf[0], TCP_TIMEOUT))throw (const DWORD)__LINE__;
		}
#pragma warning(disable: 4101) // warning C4101: "ローカル変数は 1 度も使われていません。"
	catch(const DWORD dwLine){
#pragma warning(default: 4101)
		// 通信エラー発生
		m_Socket.Close();
//		SendProxyEvent(CPEI_DISCONNECTED);
		return 0UL;
		}
		
	return SendBuf[0];
}
/*
void CCasProxy::SendEnterProcessEvent(const HWND hProxyHwnd)
{
	// プロセス開始通知送信
	::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, CPEI_ENTERPROCESS, ::GetCurrentProcessId());
}

void CCasProxy::SendExitProcessEvent(const HWND hProxyHwnd)
{
	// プロセス終了通知送信
	::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, CPEI_EXITPROCESS, ::GetCurrentProcessId());
}

const DWORD CCasProxy::SendProxyEvent(WPARAM wParam) const
{
	// ホストにイベントを送信する
	return SendProxyEvent2(m_hProxyHwnd, wParam, ::GetCurrentProcessId());
}

DWORD CCasProxy::SendProxyEvent2(const HWND hProxyHwnd, WPARAM wParam, LPARAM lParam)
{
	// ホストにイベントを送信する(static)
	return (DWORD)::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, wParam, lParam);
}
*/
