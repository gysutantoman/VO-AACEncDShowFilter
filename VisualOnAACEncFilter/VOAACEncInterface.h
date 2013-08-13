#ifndef __VOAACENC_INTERFACE__
#define __VOAACENC_INTERFACE__

struct VOAACEncParmas {
	bool adtsuse;
	int bitrates;	// bps, 128000 for 128 kbps
};

#endif

#ifndef __VOAACENC_FILTER_CLSID__
#define __VOAACENC_FILTER_CLSID__

// {D4BBD31D-7702-41F9-BF13-23A2FC0A1490}
static const GUID CLSID_VOAACEncFilter = 
{ 0xd4bbd31d, 0x7702, 0x41f9, { 0xbf, 0x13, 0x23, 0xa2, 0xfc, 0xa, 0x14, 0x90 } };

// {3E58D9B4-700C-485A-99CF-C4B3659DB2BD}
static const GUID IID_IVOAACEnc = 
{ 0x3e58d9b4, 0x700c, 0x485a, { 0x99, 0xcf, 0xc4, 0xb3, 0x65, 0x9d, 0xb2, 0xbd } };

DECLARE_INTERFACE_(IVOAACEnc, IUnknown) {
	STDMETHOD(GetVOAACEncConfig)(VOAACEncParmas *config) PURE;
	STDMETHOD(SetVOAACEncConfig)(const VOAACEncParmas *config) PURE;
};

#endif


