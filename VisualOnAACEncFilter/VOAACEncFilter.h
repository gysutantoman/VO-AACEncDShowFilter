#pragma once
class VOAACEncFilter :
	public CTransformFilter,
	public IVOAACEnc
{
public:
	VOAACEncFilter(LPUNKNOWN pUnk, HRESULT *phr);
	~VOAACEncFilter(void);

	static CUnknown *WINAPI CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);
	DECLARE_IUNKNOWN;
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv);

	virtual HRESULT CheckInputType(const CMediaType *mtIn);
	virtual HRESULT CheckTransform(
		const CMediaType *mtIn, 
		const CMediaType *mtOut);
	virtual HRESULT DecideBufferSize(
		IMemAllocator *pAlloc, 
		ALLOCATOR_PROPERTIES *pProp);
	virtual HRESULT GetMediaType(int iPosition, CMediaType *pmt);
	virtual HRESULT SetMediaType(
		PIN_DIRECTION direction, 
		const CMediaType *pmt);
	virtual HRESULT StartStreaming();
	virtual HRESULT EndFlush();

	virtual HRESULT Receive(IMediaSample *pSample);
	virtual HRESULT GetDeliveryBuffer(IMediaSample **sample);

	// IVOAACEnc
	STDMETHODIMP GetVOAACEncConfig(VOAACEncParmas *config);
	STDMETHODIMP SetVOAACEncConfig(const VOAACEncParmas *config);

private:
	void CloseEncoder();
	bool OpenEncoder();

	bool GenerateDecSpecInfo();
private:
	//CTransformInputPin *m_pInputPin;
	//CTransformOutputPin *m_pOutputPin;

	// aac-filter interface
	VOAACEncParmas m_VOAACCommonParams;
	CCritSec m_lockParams;

	// vo-aac
	VO_HANDLE m_VOAACHandle;
	AACENC_PARAM m_VOAACParams;
	VO_AUDIO_CODECAPI m_VOAACCodecAPI;
	VO_MEM_OPERATOR m_VOAACMemOpr;
	VO_CODEC_INIT_USERDATA m_VOAACUserData;

	// dshow
	uint8_t m_DecoderSpecificInfo[2];
	uint8_t *m_AACOutputBuffer;
	REFERENCE_TIME m_tsOffset;
	REFERENCE_TIME m_aacFrameDuration;
	uint8_t m_CodedFrameCount;
	std::string m_OverageSamples;
};

