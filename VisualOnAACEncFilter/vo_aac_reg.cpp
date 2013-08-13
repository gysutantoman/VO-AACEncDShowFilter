#include "stdafx.h"

const AMOVIESETUP_MEDIATYPE AACPinTypes[] = {
	{ &MEDIATYPE_Audio, &MEDIASUBTYPE_PCM      },
	{ &MEDIATYPE_Audio, &MEDIASUBTYPE_RAW_AAC1 }
};

const AMOVIESETUP_PIN AACPins[] = { 
	{ L"Input",  FALSE,  FALSE, FALSE, FALSE, &CLSID_NULL, NULL, 1, &AACPinTypes[0] }, 
	{ L"Output", FALSE,  TRUE,  FALSE, FALSE, &CLSID_NULL, NULL, 1, &AACPinTypes[1] } 
};

const AMOVIESETUP_FILTER VOAACEncSetup = 
	{ &CLSID_VOAACEncFilter, L"Visual On AAC Encoder", MERIT_DO_NOT_USE, 2, AACPins };                     

CFactoryTemplate g_Templates[]=	{
	{ L"VisualOn AAC Encoder", &CLSID_VOAACEncFilter, VOAACEncFilter::CreateInstance, NULL, &VOAACEncSetup }
};
int g_cTemplates = sizeof(g_Templates)/sizeof(g_Templates[0]);

STDAPI DllRegisterServer(){return AMovieDllRegisterServer2(TRUE);}
STDAPI DllUnregisterServer(){return AMovieDllRegisterServer2(FALSE);}

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved) {
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}