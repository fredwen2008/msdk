#/************************************************************************/
/* 
Author:

lourking. .All rights reserved.

Create Time:

	4,1th,2014

Module Name:

	dsHftpInterface.h  

Abstract: 内部网络任务


*/
/************************************************************************/
#include <wininet.h>
#include <string>
#include <iostream>
using namespace std;

#pragma comment(lib, "wininet.lib")

//下载
#define  DOWNHELPER_AGENTNAME         _T("ds_lourking")
#define  LEN_OF_BUFFER_FOR_QUERYINFO  128 

class dsHftpInterface;

typedef LRESULT (CALLBACK *SPEEDPROC)(dsHftpInterface* /*phi*/, BOOL /*bRetPerAPI*/, DWORD /*dwDownBytesCount*/, LPVOID /*lpParam*/, DWORD /*dwLastError*/);



typedef struct dsLittlePieceInfo
{
	BOOL bRet;
	DWORD dwDownBytesCount;
	DWORD dwLastError;

	inline void SetInfo(BOOL _bRet, DWORD _dwDownBytesCount, DWORD _dwLastError){
		bRet = _bRet;
		dwDownBytesCount = _dwDownBytesCount;
		dwLastError = _dwLastError;
	}

}DSLPI,*PDSLPI;


static dsLock _lockSend;

class dsHftpInterface
{
public:
	DWORD m_dwDownloadSizePerRead;
	DWORD m_dwMaxDownloadSize;
	UINT m_uRequestTryTimes;
	UINT m_uTimeOut; 
	HINTERNET m_hRequestGet;//HTTP Request
	HINTERNET m_hRequestHead;//HTTP Request
	HINTERNET m_hConnect;//HTTP连接
	HINTERNET m_hInet;//打开internet连接handle
	DWORD m_dwContentLen;
	CString strUrl;

public:
	dsHftpInterface():
		m_dwDownloadSizePerRead(1024),
		m_dwMaxDownloadSize(1000*1024*1024),
		m_uRequestTryTimes(20),
		m_hInet(NULL),
		m_hConnect(NULL),
		m_hRequestGet(NULL),
		m_hRequestHead(NULL),
		m_uTimeOut(100),
		m_dwContentLen(0)
	{

	}

	~dsHftpInterface(){

	}
public:

	BOOL Init();
	void Close(){
		if (m_hRequestGet)
			InternetCloseHandle(m_hRequestGet);
		//if (m_hRequestHead)
		//	InternetCloseHandle(m_hRequestHead);
		if (m_hConnect)
			InternetCloseHandle(m_hConnect);
		if (m_hInet)
			InternetCloseHandle(m_hInet);

	}

public:

	//HTTP下载函数，通过先请求HEAD的方式然后GET，可以通过HEAD对下载的文件类型和大小做限制
	//HTTP下载函数，通过先请求HEAD的方式然后GET，可以通过HEAD对下载的文件类型和大小做限制
	BOOL Init(LPCTSTR lpszUrl, __out DWORD *pdwBufSize, DWORD dwOffset = 0)//, byte *pbufForRecv, UINT uBufSize)
	{
		BOOL bRet = FALSE;

		if(NULL == lpszUrl || 0 == _tcslen(lpszUrl) /*|| NULL == pbufForRecv || uBufSize <= 0*/)
			return FALSE;

		char* pBuf = NULL; //缓冲区
		DWORD dwDownBytes = 0; //每次下载的大小
		char bufQueryInfo[LEN_OF_BUFFER_FOR_QUERYINFO] = {0}; //用来查询信息的buffer
		DWORD dwBufQueryInfoSize = sizeof(bufQueryInfo);
		DWORD dwStatusCode = 0;
		DWORD dwContentLen = 0;
		DWORD dwSizeDW = sizeof(DWORD);

		//分割URL
		TCHAR pszHostName[INTERNET_MAX_HOST_NAME_LENGTH] = {0};
		TCHAR pszUserName[INTERNET_MAX_USER_NAME_LENGTH] = {0};
		TCHAR pszPassword[INTERNET_MAX_PASSWORD_LENGTH] = {0};
		TCHAR pszURLPath[INTERNET_MAX_URL_LENGTH] = {0};
		TCHAR szURL[INTERNET_MAX_URL_LENGTH] = {0};

		URL_COMPONENTS urlComponents = {0};
		urlComponents.dwStructSize = sizeof(URL_COMPONENTSA);
		urlComponents.lpszHostName = pszHostName;
		urlComponents.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
		urlComponents.lpszUserName = pszUserName;
		urlComponents.dwUserNameLength = INTERNET_MAX_USER_NAME_LENGTH;
		urlComponents.lpszPassword = pszPassword;
		urlComponents.dwPasswordLength = INTERNET_MAX_PASSWORD_LENGTH;
		urlComponents.lpszUrlPath = pszURLPath;
		urlComponents.dwUrlPathLength = INTERNET_MAX_URL_LENGTH;

		bRet = InternetCrackUrl(lpszUrl, 0, NULL, &urlComponents);
		bRet = (bRet && urlComponents.nScheme == INTERNET_SERVICE_HTTP);
		if (!bRet)
		{
			goto _END_OF_DOWNLOADURL;
		}


		//打开一个internet连接
		m_hInet = InternetOpen(DOWNHELPER_AGENTNAME, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, NULL);
		if (!m_hInet)
		{
			bRet = FALSE;
			goto _END_OF_DOWNLOADURL;
		}


		ATLTRACE("interface init 1\n");

		//打开HTTP连接
		m_hConnect = InternetConnect(m_hInet, pszHostName, urlComponents.nPort, pszUserName, pszPassword, INTERNET_SERVICE_HTTP, 0, NULL);
		if (!m_hConnect)
		{
			bRet = FALSE;
			goto _END_OF_DOWNLOADURL;
		}


		ATLTRACE("interface init 2\n");

		//创建HTTP request句柄
		if (urlComponents.dwUrlPathLength !=  0)
			_tcscpy(szURL, urlComponents.lpszUrlPath);
		else
			_tcscpy(szURL, _T("/"));


		////校验完成后再请求GET，下载文件
		static CONST TCHAR *szAcceptType[2]={_T("Accept: */*"),NULL};
		m_hRequestGet = HttpOpenRequest(m_hConnect, _T("GET"), szURL, _T("HTTP/1.1"), _T(""), szAcceptType, INTERNET_FLAG_RELOAD | INTERNET_FLAG_DONT_CACHE, 0);

		ATLTRACE("interface init 3\n");

		if(0 != dwOffset)
		{
			WCHAR szOffset[MAX_PATH];
			wsprintf (szOffset, L"Range:bytes=%d-\r\n",dwOffset);
			HttpAddRequestHeaders(m_hRequestGet,szOffset,-1,HTTP_ADDREQ_FLAG_ADD|HTTP_ADDREQ_FLAG_REPLACE);

			if(NULL == m_hRequestGet)
				ATLTRACE("interface init failed\n");
		}

		bRet = _TryHttpSendRequest2(m_hRequestGet, m_uRequestTryTimes);

		dwContentLen = 0;
		dwSizeDW = sizeof(DWORD);
		bRet = HttpQueryInfo(m_hRequestGet, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &dwContentLen, &dwSizeDW, NULL);

		if (bRet)
		{
			if(NULL != pdwBufSize)
				*pdwBufSize = dwContentLen;

			//检查是否文件过大
			if (dwContentLen > m_dwMaxDownloadSize)
			{
				bRet = FALSE;
				goto _END_OF_DOWNLOADURL;
			}
		}

		if (!bRet)
		{
			goto _END_OF_DOWNLOADURL; //请求HEAD失败
		} 


		ATLASSERT(bRet);


		ATLTRACE("interface init ok\n");

		//清理
_END_OF_DOWNLOADURL:
		//Close();

		return bRet;
	}





	inline BOOL SetOption(__in DWORD dwOption, __in_opt LPVOID lpBuffer, __in DWORD dwBufferLength ){

		return InternetSetOption(m_hRequestGet, dwOption, lpBuffer, dwBufferLength);
	}

	inline DWORD SetFileOffset(LONG lOffset){

		wchar_t szHeader[256];
		wsprintf(szHeader, L"Range: bytes=%d-\r\n\r\n", lOffset, m_dwContentLen);
	
		wsprintf(szHeader,L"GET /%s HTTP/1.1\r\n"
			//"Host: %s\r\n"
			L"Range: bytes=%d-%d\r\n"	//从m_receivedDataSize位置开始
			L"Connection: keep-alive\r\n"
			L"\r\n",strUrl,/*DEST_IP,*/lOffset,m_dwContentLen);  

		if(::HttpAddRequestHeaders(m_hRequestGet,szHeader,wcslen(szHeader),HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE))
			return lOffset;
			

		DWORD dwError = GetLastError();
		return ::InternetSetFilePointer(m_hRequestGet, lOffset, NULL, FILE_BEGIN, 0);
	}

	BOOL DownloadPieceOnce(byte *pbufForRecv, UINT uDownSize, DWORD &dwDownBytesCount){
		//memset(pbufForRecv, 0, uDownSize*sizeof(char));
		return InternetReadFile(m_hRequestGet, pbufForRecv, uDownSize, &dwDownBytesCount);
	}

	BOOL EnterDownloadLoop(byte *pbufForRecv, UINT uBufSize, DWORD &dwDownBytesCount, SPEEDPROC proc = NULL, LPVOID lpParam = NULL)
	{
		//分配缓冲
		
		dwDownBytesCount = 0;

		int nRetryCount = 0;
		BOOL bRet = FALSE;
		DWORD dwTempCount;
		DWORD dwReadCount = 0;
		while (TRUE)
		{
			dwTempCount = 0;
			//memset(pbufForRecv + dwTempCount, 0, m_dwDownloadSizePerRead*sizeof(char));

			dwReadCount = min(m_dwDownloadSizePerRead, uBufSize - dwDownBytesCount);

			bRet = InternetReadFile(m_hRequestGet, pbufForRecv + dwDownBytesCount, dwReadCount, &dwTempCount);
			DWORD dwError = WSAGetLastError(); 

			if(NULL != proc)
				if(-1 == proc(this, bRet, dwTempCount, lpParam, dwError)) //回调，可用于计算下载速度
				{

					break;
				}

			if (bRet)
			{
				if (dwTempCount > 0)
				{
					dwDownBytesCount += dwTempCount;
				}
				else if (0 == dwTempCount)
				{
					break; //下载到文件最末尾
				}

				if(dwDownBytesCount == uBufSize)
					break; //切片下载完成
			}
			else if(ERROR_INTERNET_TIMEOUT == dwError && nRetryCount++ < m_uRequestTryTimes){
				cout<<"请求超时，继续重试下载，已累计次数："<<nRetryCount<<endl;

				if (dwTempCount > 0)
				{
					dwDownBytesCount += dwTempCount;
				}
				else if (0 == dwTempCount)
				{
					break; //下载到文件最末尾
				}

				if(dwDownBytesCount == uBufSize)
					break; //切片下载完成

			}
			else{
				cout<<"接收数据失败"<<"错误码："<<dwError<<endl;
				bRet = FALSE;
				break;
			}
		}

		return bRet;
	}

	//多次发送请求函数
	BOOL _TryHttpSendRequest(LPVOID hRequest, int nMaxTryTimes)
	{

		

		BOOL bRet = FALSE;
		DWORD dwStatusCode = 0;
		DWORD dwSizeDW = sizeof(DWORD);
		while (hRequest && (nMaxTryTimes-- > 0)) //多次尝试发送请求
		{
			//发送请求
	
			bRet = HttpSendRequest(hRequest, NULL, 0, NULL, 0);
			if (!bRet)
			{
				continue;
			}
			else
			{
				//判断HTTP返回的状态码
				dwStatusCode = 0;
				dwSizeDW = sizeof(DWORD);
				bRet = HttpQueryInfo(hRequest, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatusCode, &dwSizeDW, NULL);
				if (bRet)
				{
					//检查状态码
					if (HTTP_STATUS_OK == dwStatusCode) //200 OK
					{
						break;
					}
					else
					{
						ATLTRACE("interface _TryHttpSendRequest loop statuscode: %d\n", dwStatusCode);

						bRet = FALSE;
						continue;
					}
				}
			}
		}

		ATLTRACE("interface _TryHttpSendRequest loop: %d\n", bRet);

		return bRet;
	}


	//多次发送请求函数
	BOOL _TryHttpSendRequest2(LPVOID hRequest, int nMaxTryTimes)
	{



		BOOL bRet = FALSE;
		DWORD dwStatusCode = 0;
		DWORD dwSizeDW = sizeof(DWORD);
		while (hRequest && (nMaxTryTimes-- > 0)) //多次尝试发送请求
		{
			//发送请求

			bRet = HttpSendRequest(hRequest, NULL, 0, NULL, 0);
			if (!bRet)
			{
				continue;
			}
			else
			{

				break;
				////判断HTTP返回的状态码
				//dwStatusCode = 0;
				//dwSizeDW = sizeof(DWORD);
				//bRet = HttpQueryInfo(hRequest, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &dwStatusCode, &dwSizeDW, NULL);
				//if (bRet)
				//{
				//	//检查状态码
				//	if (HTTP_STATUS_OK == dwStatusCode) //200 OK
				//	{
				//		break;
				//	}
				//	else
				//	{
				//		ATLTRACE("interface _TryHttpSendRequest loop statuscode: %d\n", dwStatusCode);

				//		bRet = FALSE;
				//		continue;
				//	}
				//}
			}
		}

		ATLTRACE("interface _TryHttpSendRequest loop: %d\n", bRet);

		return bRet;
	}


public:



};





