﻿#define PXCH_DO_NOT_INCLUDE_STD_HEADERS_NOW
#define PXCH_DO_NOT_INCLUDE_STRSAFE_NOW
#include "includes_win32.h"
#include "common_generic.h"
#include <stdlib.h>
#include <WinSock2.h>
#include <Ws2Tcpip.h>
#include <wchar.h>
#include <strsafe.h>
	
static WCHAR g_HostPrintBuf[100];

const wchar_t* FormatHostPortToStr(const void* pHostPort, int iAddrLen)
{
	DWORD dwLen;
	dwLen = _countof(g_HostPrintBuf);
	g_HostPrintBuf[0] = L'\0';

	if (HostIsType(HOSTNAME, *(PXCH_HOST*)pHostPort)) {
		StringCchPrintfW(g_HostPrintBuf, dwLen, L"%ls%hu", ((PXCH_HOSTNAME*)pHostPort)->szValue, ntohs(((PXCH_HOSTNAME*)pHostPort)->wPort));
	} else {
		WSAAddressToStringW((struct sockaddr*)(pHostPort), iAddrLen, NULL, g_HostPrintBuf, &dwLen);
	}
	return g_HostPrintBuf;
}

void IndexToIp(const PROXYCHAINS_CONFIG* pPxchConfig, PXCH_IP_ADDRESS* pIp, PXCH_UINT32 iIndex)
{
	PXCH_HOST* pHost = (PXCH_HOST*)&pPxchConfig->FakeIpRange;
	ZeroMemory(pIp, sizeof(PXCH_IP_ADDRESS));
	if (HostIsType(IPV4, *pHost)) {
		struct sockaddr_in* pIpv4 = (struct sockaddr_in*)pIp;
		pIpv4->sin_family = PXCH_HOST_TYPE_IPV4;
		PXCH_UINT32 dwMaskInvert;
		PXCH_UINT32 dwToShift = pPxchConfig->dwFakeIpRangePrefix > 32 ? 0 : 32 - pPxchConfig->dwFakeIpRangePrefix;

		pIpv4->sin_addr = ((struct sockaddr_in*) & pPxchConfig->FakeIpRange)->sin_addr;
		dwMaskInvert = htonl((PXCH_UINT32)((((PXCH_UINT64)1) << dwToShift) - 1));
		pIpv4->sin_addr.s_addr &= ~dwMaskInvert;
		pIpv4->sin_addr.s_addr |= (htonl(iIndex) & dwMaskInvert);
		goto out_succ;
	}

	if (HostIsType(IPV6, *pHost)) {
		struct sockaddr_in6* pIpv6 = (struct sockaddr_in6*)pIp;
		pIpv6->sin6_family = PXCH_HOST_TYPE_IPV6;
		struct {
			PXCH_UINT64 First64;
			PXCH_UINT64 Last64;
		} MaskInvert, * pIpv6AddrInQwords;

		PXCH_UINT32 dwToShift = pPxchConfig->dwFakeIpRangePrefix > 128 ? 0 : 128 - pPxchConfig->dwFakeIpRangePrefix;
		PXCH_UINT32 dwShift1 = dwToShift >= 64 ? 64 : dwToShift;
		PXCH_UINT32 dwShift2 = dwToShift >= 64 ? (dwToShift - 64) : 0;

		MaskInvert.Last64 = dwShift1 == 64 ? 0xFFFFFFFFFFFFFFFFU : ((((PXCH_UINT64)1) << dwShift1) - 1);
		MaskInvert.First64 = dwShift2 == 64 ? 0xFFFFFFFFFFFFFFFFU : ((((PXCH_UINT64)1) << dwShift2) - 1);

		if (LITTLEENDIAN) {
			MaskInvert.Last64 = _byteswap_uint64(MaskInvert.Last64);
			MaskInvert.First64 = _byteswap_uint64(MaskInvert.First64);
		}


		pIpv6->sin6_addr = ((struct sockaddr_in6*) & pPxchConfig->FakeIpRange)->sin6_addr;
		pIpv6AddrInQwords = (void*)&pIpv6->sin6_addr;
		pIpv6AddrInQwords->First64 &= ~MaskInvert.First64;
		pIpv6AddrInQwords->Last64 &= ~MaskInvert.Last64;
		pIpv6AddrInQwords->Last64 |= (htonl(iIndex) & MaskInvert.Last64);
		goto out_succ;
	}
	return;

out_succ:
	;
	// LOGI(L"Map index to IP: " WPRDW L" -> %ls", iIndex, FormatHostPortToStr(pIp, sizeof(PXCH_IP_ADDRESS)));
}

void IpToIndex(const PROXYCHAINS_CONFIG* pPxchConfig, PXCH_UINT32* piIndex, const PXCH_IP_ADDRESS* pIp)
{
	PXCH_HOST* pHost = (PXCH_HOST*)&pPxchConfig->FakeIpRange;
	PXCH_HOST* pInHost = (PXCH_HOST*)pIp;
	if (HostIsType(IPV4, *pHost) && HostIsType(IPV4, *pInHost)) {
		struct sockaddr_in* pIpv4 = (struct sockaddr_in*)pIp;
		PXCH_UINT32 dwMaskInvert;
		PXCH_UINT32 dwToShift = pPxchConfig->dwFakeIpRangePrefix > 32 ? 0 : 32 - pPxchConfig->dwFakeIpRangePrefix;

		dwMaskInvert = htonl((PXCH_UINT32)((((PXCH_UINT64)1) << dwToShift) - 1));
		*piIndex = pIpv4->sin_addr.s_addr & dwMaskInvert;
		goto out_succ;
	}

	if (HostIsType(IPV6, *pHost) && HostIsType(IPV6, *pInHost)) {
		struct sockaddr_in6* pIpv6 = (struct sockaddr_in6*)pIp;
		struct {
			PXCH_UINT64 First64;
			PXCH_UINT64 Last64;
		} MaskInvert, * pIpv6AddrInQwords;

		PXCH_UINT32 dwToShift = pPxchConfig->dwFakeIpRangePrefix > 128 ? 0 : 128 - pPxchConfig->dwFakeIpRangePrefix;
		PXCH_UINT32 dwShift1 = dwToShift >= 64 ? 64 : dwToShift;
		PXCH_UINT32 dwShift2 = dwToShift >= 64 ? (dwToShift - 64) : 0;

		MaskInvert.Last64 = dwShift1 == 64 ? 0xFFFFFFFFFFFFFFFFU : ((((PXCH_UINT64)1) << dwShift1) - 1);
		MaskInvert.First64 = dwShift2 == 64 ? 0xFFFFFFFFFFFFFFFFU : ((((PXCH_UINT64)1) << dwShift2) - 1);

		if (LITTLEENDIAN) {
			MaskInvert.Last64 = _byteswap_uint64(MaskInvert.Last64);
			MaskInvert.First64 = _byteswap_uint64(MaskInvert.First64);
		}

		pIpv6AddrInQwords = (void*)&pIpv6->sin6_addr;

		*piIndex = (PXCH_UINT32)(pIpv6AddrInQwords->Last64 & MaskInvert.Last64);
		goto out_succ;
	}

	*piIndex = -1;
	return;

out_succ:
	;
	// LOGI(L"Map IP to index: %ls -> " WPRDW, FormatHostPortToStr(pIp, sizeof(PXCH_IP_ADDRESS)), *piIndex);
}