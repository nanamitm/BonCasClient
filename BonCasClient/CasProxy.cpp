// CasProxy.cpp: CCasProxy �N���X�̃C���v�������e�[�V����
//
//////////////////////////////////////////////////////////////////////


#include "StdAfx.h"
#include "CasProxy.h"

#include <sysinfoapi.h>


#define WM_BONCASPROXY_EVENT		(WM_USER + 100U)
#define TCP_TIMEOUT					1000UL				// 1�b


ULONGLONG CCasProxy::m_ErrorDelayTime = 0ULL;


CCasProxy::CCasProxy(const HWND hProxyHwnd)
	: m_hProxyHwnd(hProxyHwnd)
	, m_dwIP(0x7F000001UL)
	, m_wPort(6900UL)
{

}

CCasProxy::~CCasProxy(void)
{
	// �T�[�o����ؒf
	m_Socket.Close();
}

const void CCasProxy::Setting(const DWORD dwIP, const WORD wPort)
{
	m_dwIP = dwIP;
	m_wPort = wPort;
}

const BOOL CCasProxy::Connect(void)
{
	// �G���[�������̃K�[�h�C���^�[�o��
	if(m_ErrorDelayTime){
		if((::GetTickCount64() - m_ErrorDelayTime) < TCP_TIMEOUT)return FALSE;
		else m_ErrorDelayTime = 0ULL;
		}

	// �T�[�o�ɐڑ�
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
	// ���M�f�[�^����
	BYTE SendBuf[256]{};
	SendBuf[0] = (BYTE)dwSendSize;
	::CopyMemory(&SendBuf[1], pSendData, dwSendSize);

	try{
		// ���N�G�X�g���M
		if(!m_Socket.Send(SendBuf, dwSendSize + 1UL, TCP_TIMEOUT))throw (const DWORD)__LINE__;
	
		// ���X�|���X�w�b�_��M
		if(!m_Socket.Recv(SendBuf, 1UL, TCP_TIMEOUT))throw (const DWORD)__LINE__;

		// ���X�|���X�f�[�^��M
		if(!m_Socket.Recv(pRecvData, SendBuf[0], TCP_TIMEOUT))throw (const DWORD)__LINE__;
		}
#pragma warning(disable: 4101) // warning C4101: "���[�J���ϐ��� 1 �x���g���Ă��܂���B"
	catch(const DWORD dwLine){
#pragma warning(default: 4101)
		// �ʐM�G���[����
		m_Socket.Close();
//		SendProxyEvent(CPEI_DISCONNECTED);
		return 0UL;
		}
		
	return SendBuf[0];
}
/*
void CCasProxy::SendEnterProcessEvent(const HWND hProxyHwnd)
{
	// �v���Z�X�J�n�ʒm���M
	::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, CPEI_ENTERPROCESS, ::GetCurrentProcessId());
}

void CCasProxy::SendExitProcessEvent(const HWND hProxyHwnd)
{
	// �v���Z�X�I���ʒm���M
	::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, CPEI_EXITPROCESS, ::GetCurrentProcessId());
}

const DWORD CCasProxy::SendProxyEvent(WPARAM wParam) const
{
	// �z�X�g�ɃC�x���g�𑗐M����
	return SendProxyEvent2(m_hProxyHwnd, wParam, ::GetCurrentProcessId());
}

DWORD CCasProxy::SendProxyEvent2(const HWND hProxyHwnd, WPARAM wParam, LPARAM lParam)
{
	// �z�X�g�ɃC�x���g�𑗐M����(static)
	return (DWORD)::SendMessage(hProxyHwnd, WM_BONCASPROXY_EVENT, wParam, lParam);
}
*/
