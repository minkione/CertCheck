#include "pch.h"

#ifndef _WIN32_WINNT // Minimum required platform is Windows Vista.
#define _WIN32_WINNT 0x0600 // Change this to the appropriate value to target other versions of Windows.
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <tchar.h>
#include <wininet.h> 
#pragma comment(lib, "WinInet.lib")
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")
#include <string>
#include <sstream>
#include <atlstr.h>
using std::wstring;

int _tmain(int argc, _TCHAR* argv[])
{
	//change URL below
	wstring url = L"https://github.com";
	wstring result;
	::URL_COMPONENTS urlComp;
	::ZeroMemory((void *)&urlComp, sizeof(URL_COMPONENTS));
	urlComp.dwSchemeLength = -1;
	urlComp.dwHostNameLength = -1;
	urlComp.dwUrlPathLength = -1;
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	if (!::InternetCrackUrl(url.c_str(), wcslen(url.c_str()), 0, &urlComp)) {
		result = L"InternetCrackUrl failed";
		return 1;
	}
	if (urlComp.nScheme != INTERNET_SCHEME_HTTPS) {
		result= L"URL is not HTTPS";
		return 1;
	}
	std::wcout << "opening connection to host: " << (LPWSTR)urlComp.lpszHostName << std::endl;

	HINTERNET hInternet = ::InternetOpen(_T(""), INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!hInternet) {
		result = L"InternetOpen() failed";
		return 1;
	}
	HINTERNET hConnect = ::InternetConnect(hInternet, urlComp.lpszHostName, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, INTERNET_FLAG_SECURE, NULL);
	if (!hConnect) {
		result = L"InternetConnect() failed";
		return 1;
	}
	HINTERNET hRequest = ::HttpOpenRequest(hConnect, _T("GET"), NULL, NULL, NULL, NULL, INTERNET_FLAG_SECURE, NULL);

	if (hRequest == NULL) {
		result = L"Error opening";
		return 1;
	}

	// set ignore options to request
	DWORD dwFlags;
	DWORD dwBuffLen = sizeof(dwFlags);
	InternetQueryOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, (LPVOID)&dwFlags, &dwBuffLen);
	dwFlags |= SECURITY_FLAG_IGNORE_UNKNOWN_CA;
	dwFlags |= SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
	dwFlags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
	dwFlags |= SECURITY_FLAG_IGNORE_WEAK_SIGNATURE;
	dwFlags |= SECURITY_FLAG_IGNORE_REVOCATION;
	InternetSetOption(hRequest, INTERNET_OPTION_SECURITY_FLAGS, &dwFlags, sizeof(dwFlags));

	BOOL fRet = ::HttpSendRequest(hRequest, _T(""), 0, NULL, 0);
	if (!fRet) {
		result = L"";
		return 1;
	}

	PCCERT_CHAIN_CONTEXT CertCtx = NULL;
	DWORD cbCertSize = sizeof(&CertCtx);

	if (InternetQueryOption(hRequest, INTERNET_OPTION_SERVER_CERT_CHAIN_CONTEXT, (LPVOID)&CertCtx, &cbCertSize))
	{
		CERT_SIMPLE_CHAIN *simpleCertChain = NULL;
		for (int i = 0; i < CertCtx->cChain; i++)
		{
			simpleCertChain = CertCtx->rgpChain[i];

			for (int j = 0; j < simpleCertChain->cElement; j++)
			{
				PCCERT_CONTEXT pCertContext = simpleCertChain->rgpElement[j]->pCertContext;

				DWORD cbSize;
				if (!(cbSize = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0))) {
					// could not get cert name
					return 1;
				}
				LPTSTR pszName;
				if (!(pszName = (LPTSTR)malloc(cbSize * sizeof(TCHAR))))
				{
					// could not allocate memory for cert name string
					return 1;
				}
				if (CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, pszName, cbSize)) {
					std::wcout << "Cert Name: " << pszName << std::endl;
				}

				CString certname;
				DWORD dwBufferSize = 0;
				CertGetCertificateContextProperty(pCertContext, CERT_HASH_PROP_ID, NULL, &dwBufferSize);
				if (dwBufferSize)
				{
					CStringW sBuffer;
					CertGetCertificateContextProperty(pCertContext, CERT_HASH_PROP_ID, sBuffer.GetBufferSetLength(dwBufferSize / sizeof(wchar_t)), &dwBufferSize);
					sBuffer.ReleaseBuffer();
					certname = sBuffer;
				}

				DWORD dwLength = 0;
				CertGetCertificateContextProperty(pCertContext, CERT_HASH_PROP_ID, NULL, &dwLength);
				BYTE thumbprint[512];
				CertGetCertificateContextProperty(pCertContext, CERT_HASH_PROP_ID, thumbprint, &dwLength);
				std::stringstream ss;
				ss << std::hex;
				for (int i(0); i < dwLength; ++i)
					ss << (int)thumbprint[i];
				std::cout << "Thumbprint: " << ss.str() << std::endl;
			}
		}
		CertFreeCertificateChain(CertCtx);
	}
}
