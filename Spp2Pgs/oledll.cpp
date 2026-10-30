/*----------------------------------------------------------------------------
* spp2pgs - Generates BluRay PG Stream from Subtitles or AviSynth scripts
* by Giton Xu <adm@subelf.net>
*
* CoCreateInstance from a dll file. This file is free and open sourced.
*----------------------------------------------------------------------------*/

#include "stdafx.h"

#include <mmdeviceapi.h>

#include "oledll.h"

DECLSPEC_NOINLINE HRESULT WINAPI CoCreateInstanceFormDllFile(const IID& rclsid, const IID& riid, LPVOID* ppv, LPCWSTR lpszDllFile)
{
	register HRESULT hr = E_FAIL;
	HMODULE hComDll = GetModuleHandleW(lpszDllFile);
	if (!hComDll)
		hComDll = LoadLibraryExW(lpszDllFile, NULL, 0);
	if (hComDll)
	{
		typedef HRESULT(WINAPI* _DllGetClassObject)(const IID&, const IID&, LPVOID*);
		_DllGetClassObject DllGetClassObject = (_DllGetClassObject)GetProcAddress(hComDll, "DllGetClassObject");
		if (DllGetClassObject)
		{
			IClassFactory* pClassFactory;
			hr = DllGetClassObject(rclsid, IID_PPV_ARGS(&pClassFactory));
			if (SUCCEEDED(hr))
			{
				hr = pClassFactory->CreateInstance(NULL, riid, ppv);
				pClassFactory->Release();
			}
		}
		else
		{
			hr = HRESULT_FROM_WIN32(ERROR_BAD_DLL_ENTRYPOINT);
			SetLastError(ERROR_BAD_DLL_ENTRYPOINT);
		}
	}
	else
	{
		DWORD const loadError = ::GetLastError();
		hr = (loadError != ERROR_SUCCESS) ? HRESULT_FROM_WIN32(loadError) : E_FAIL;
		SetLastError(loadError);
	}

	return hr;
}
