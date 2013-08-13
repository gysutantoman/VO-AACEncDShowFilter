#include "StdAfx.h"
#include <atlbase.h>

VOAACEncFilter::VOAACEncFilter(LPUNKNOWN pUnk, HRESULT *phr)
	: CTransformFilter("VisualOn AAC Encoder", pUnk, CLSID_VOAACEncFilter)
{
	m_pInput = new CTransformInputPin(TEXT("Input"), this, phr, TEXT("In"));
	m_pOutput = new CTransformOutputPin(TEXT("Output"), this, phr,  TEXT("Out"));

	m_VOAACHandle = NULL;
	m_AACOutputBuffer = NULL;
	m_AACOutputBuffer = new uint8_t[20480];

	memset(&m_VOAACParams, 0, sizeof(m_VOAACParams));
	m_VOAACParams.adtsUsed = 0;
	m_VOAACParams.bitRate = 128000;
}

VOAACEncFilter::~VOAACEncFilter(void)
{
	if (m_AACOutputBuffer) {
		delete [] m_AACOutputBuffer;
		m_AACOutputBuffer = NULL;
	}

	CloseEncoder();
}

CUnknown *WINAPI VOAACEncFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	auto *enc = new VOAACEncFilter(pUnk, phr);
	if (!enc && phr) {
		*phr = E_OUTOFMEMORY;
	} else if (phr && FAILED(*phr)) {
		delete enc;
		enc = NULL;
	}
	return enc;
}

STDMETHODIMP VOAACEncFilter::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if (IID_IVOAACEnc == riid) {
		return GetInterface((IVOAACEnc *)this, ppv);
	} else {
		return __super::NonDelegatingQueryInterface(riid, ppv);
	}
}

STDMETHODIMP VOAACEncFilter::GetVOAACEncConfig(VOAACEncParmas *config)
{
	if (NULL == config) {
		return E_POINTER;
	}
	CAutoLock lck(&m_lockParams);
	*config = m_VOAACCommonParams;

	return S_OK;
}

STDMETHODIMP VOAACEncFilter::SetVOAACEncConfig(const VOAACEncParmas *config)
{
	if (NULL == config) {
		return E_POINTER;
	}
	if (State_Stopped != m_State) {
		return E_FAIL;
	}

	CAutoLock lck(&m_lockParams);
	m_VOAACCommonParams = *config;

	return S_OK; 
}

HRESULT VOAACEncFilter::CheckInputType(const CMediaType *mtIn)
{
	if  ((mtIn->formattype != FORMAT_WaveFormatEx)  ||
		 (mtIn->majortype  != MEDIATYPE_Audio)      ||
	     (mtIn->subtype    != MEDIASUBTYPE_PCM)     ||
		 (mtIn->pbFormat   == NULL)) {
			 return E_FAIL;
	}

	WAVEFORMATEX *wfx = (WAVEFORMATEX *)mtIn->pbFormat;

	if (wfx->wBitsPerSample != 16) {
		AtlTrace("[%s] wBitPerSample != 16\n", __FUNCTION__);
		return E_FAIL;
	}
	if (wfx->nChannels != 2) {
		AtlTrace("[%s] Channel != 2\n", __FUNCTION__);
		return E_FAIL;
	}

	return NOERROR;
}

HRESULT VOAACEncFilter::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut)
{
	if (FAILED(CheckInputType(mtIn))) {
		return E_FAIL;
	}

	if (mtOut->majortype != MEDIATYPE_Audio || 
		mtOut->subtype   != MEDIASUBTYPE_RAW_AAC1) {
			return E_FAIL;
	}

	return S_OK;
}

HRESULT VOAACEncFilter::GetMediaType(int iPosition, CMediaType *pmt)
{
	if (iPosition < 0) {
		return E_INVALIDARG;
	}
	if (iPosition > 0) {
		return VFW_S_NO_MORE_ITEMS;
	}
	if (FALSE == m_pInput->IsConnected()) {
		return E_FAIL;
	}
	if (!m_VOAACHandle) {
		return E_FAIL;
	}

	pmt->majortype	= MEDIATYPE_Audio;
	pmt->subtype	= MEDIASUBTYPE_RAW_AAC1;
	pmt->formattype	= FORMAT_WaveFormatEx;
	pmt->lSampleSize = 2048 * 2;

	WAVEFORMATEX *wfx	= (WAVEFORMATEX*)pmt->AllocFormatBuffer(sizeof(*wfx) + 2/*m_DecoderSpecificInfo*/);
	memset(wfx, 0, sizeof(*wfx));
	wfx->cbSize			= 2;
	wfx->nChannels		= m_VOAACParams.nChannels;
	wfx->nSamplesPerSec	= m_VOAACParams.sampleRate;
	wfx->wBitsPerSample = 16;
	wfx->wFormatTag		= 0xff;

	// decoder specific info
	BYTE *ex = ((BYTE *)wfx) + sizeof(*wfx);
	memcpy(ex, m_DecoderSpecificInfo, 2);

	return NOERROR;
}

HRESULT VOAACEncFilter::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProp)
{
	pProp->cbBuffer = 2048 * 2;
	pProp->cBuffers = 2;

	ALLOCATOR_PROPERTIES act;
	HRESULT hr = pAlloc->SetProperties(pProp, &act);
	if (FAILED(hr)) {
		return hr;
	}

	if (act.cbBuffer < pProp->cbBuffer) {
		return E_FAIL;
	}
	return NOERROR;
}

HRESULT VOAACEncFilter::SetMediaType(PIN_DIRECTION direction, const CMediaType *pmt)
{
	if (direction == PINDIR_INPUT) {
		WAVEFORMATEX *wfx = (WAVEFORMATEX *)pmt->pbFormat;

		m_VOAACParams.nChannels = wfx->nChannels;
		m_VOAACParams.sampleRate = wfx->nSamplesPerSec;

		if (m_VOAACHandle) {
			CloseEncoder();
		}

		if (!OpenEncoder()) {
			return E_FAIL;
		}

		m_aacFrameDuration = 
			20480000000/(m_VOAACParams.sampleRate*m_VOAACParams.nChannels);
	}

	return NOERROR;
}

HRESULT VOAACEncFilter::Receive(IMediaSample *pSample)
{
	long &&in_len = pSample->GetActualDataLength();
	long in_remain = in_len;
	BYTE *in_data(NULL);
	if (0 == in_len || FAILED(pSample->GetPointer(&in_data))) {
		return S_OK;
	}
	REFERENCE_TIME in_start(0LL), in_stop(0LL);
	pSample->GetTime(&in_start, &in_stop);

	if (m_OverageSamples.size() == 0) {
		m_tsOffset = in_start;
		m_CodedFrameCount = 0;
	}

	m_OverageSamples.append((const char *)in_data, in_len);
	VO_CODECBUFFER vo_aac_input = { 0 }, vo_aac_output = { 0 };
	vo_aac_output.Buffer = m_AACOutputBuffer;

	HRESULT hr(S_OK);
	while (m_OverageSamples.size() >= 4096) {
		vo_aac_input.Buffer = (uint8_t *)m_OverageSamples.data();
		vo_aac_input.Length = 4096;
		vo_aac_output.Length = 20480;
		
		auto &&ret2 = m_VOAACCodecAPI.SetInputData(m_VOAACHandle, &vo_aac_input);
		if (VO_ERR_NONE != ret2) {
			AtlTrace("m_VOAACCodecAPI.SetInputData return error:%u\n", ret2);
			return S_OK;
		}

		auto &&ret = m_VOAACCodecAPI.GetOutputData(m_VOAACHandle, &vo_aac_output, NULL);
		if (VO_ERR_NONE != ret) {
			AtlTrace("m_VOAACCodecAPI.GetOutputData return error:%u\n", ret);
			return S_OK;
		}

		REFERENCE_TIME out_start = m_tsOffset + m_CodedFrameCount * m_aacFrameDuration;
		REFERENCE_TIME out_stop = out_start + m_aacFrameDuration - 1;
		IMediaSample *out_sample(NULL);
		if (SUCCEEDED(GetDeliveryBuffer(&out_sample))) {
			out_sample->SetTime(&out_start, &out_stop);
			out_sample->SetSyncPoint(TRUE);
			byte *out_data(NULL);
			if (SUCCEEDED(out_sample->GetPointer(&out_data))) {
				memcpy(out_data, vo_aac_output.Buffer, vo_aac_output.Length);
				out_sample->SetActualDataLength(vo_aac_output.Length);
				hr = m_pOutput->Deliver(out_sample);
			}
			out_sample->Release();
		}

		m_CodedFrameCount++;
		m_OverageSamples.erase(0, 4096);
	}

	return hr;
}

HRESULT VOAACEncFilter::GetDeliveryBuffer(IMediaSample **sample)
{
    IMediaSample *pOutSample(NULL);
    HRESULT hr = m_pOutput->GetDeliveryBuffer(&pOutSample, NULL, NULL, 0);
    if (FAILED(hr)) {
        return hr;
    }
	*sample = pOutSample;

	return NOERROR;
}

HRESULT VOAACEncFilter::StartStreaming()
{
	if (!m_VOAACHandle) {
		return E_FAIL;
	}
	if (!m_AACOutputBuffer) {
		return E_OUTOFMEMORY;
	}

	m_tsOffset = 0LL;
	m_CodedFrameCount = 0;
	m_OverageSamples.clear();

	return NOERROR;
}

HRESULT VOAACEncFilter::EndFlush()
{
	return __super::EndFlush();
}

void VOAACEncFilter::CloseEncoder()
{
	if (m_VOAACHandle) {
		m_VOAACCodecAPI.Uninit(m_VOAACHandle);
		m_VOAACHandle = NULL;
	}
}

bool VOAACEncFilter::OpenEncoder()
{
	if (m_VOAACHandle) {
		return false;
	}

	memset(&m_VOAACCodecAPI, 0, sizeof(m_VOAACCodecAPI));
	voGetAACEncAPI(&m_VOAACCodecAPI);
	m_VOAACMemOpr.Alloc = cmnMemAlloc;
	m_VOAACMemOpr.Copy = cmnMemCopy;
	m_VOAACMemOpr.Free = cmnMemFree;
	m_VOAACMemOpr.Set = cmnMemSet;
	m_VOAACMemOpr.Check = cmnMemCheck;

	memset(&m_VOAACUserData, 0, sizeof(m_VOAACUserData));
	m_VOAACUserData.memflag = VO_IMF_USERMEMOPERATOR;
	m_VOAACUserData.memData = &m_VOAACMemOpr;

	if (VO_ERR_NONE != m_VOAACCodecAPI.Init(&m_VOAACHandle, VO_AUDIO_CodingAAC, &m_VOAACUserData)) {
		return false;
	}
	if (VO_ERR_NONE != m_VOAACCodecAPI.SetParam(m_VOAACHandle, VO_PID_AAC_ENCPARAM, &m_VOAACParams)) {
		return false;
	}

	GenerateDecSpecInfo();
	return true;
}

uint8_t GetSampleRateIndexBySampleRate(int sample_rate)
{
	uint8_t sr_index = 0;
	switch (sample_rate)
	{
	case 96000: 
		sr_index = 0;
		break;
	case 88200: 
		sr_index = 1;
		break;
	case 64000: 
		sr_index = 2;
		break;
	case 48000: 
		sr_index = 3;
		break;
	case 44100: 
		sr_index = 4;
		break;
	case 32000: 
		sr_index = 5;
		break;
	case 24000: 
		sr_index = 6;
		break;
	case 22050: 
		sr_index = 7;
		break;
	case 16000: 
		sr_index = 8;
		break;
	case 12000: 
		sr_index = 9;
		break;
	case 11025: 
		sr_index = 10;
		break;
	case 8000: 
		sr_index = 11;
		break;
	case 7350: 
		sr_index = 12;
		break;
	default:
		sr_index = 15;
		break;
	}
	return sr_index;
}

bool VOAACEncFilter::GenerateDecSpecInfo()
{
	memset(m_DecoderSpecificInfo, 0, 2);
	// profile = 1 (low Complexity)
	m_DecoderSpecificInfo[0] |= 0x10;
	auto const &&sr_index = GetSampleRateIndexBySampleRate(m_VOAACParams.sampleRate);
	// 3b samplerate
	m_DecoderSpecificInfo[0] |= sr_index>>1;
	// 1b samplerate + 4b channels(2channels)
	m_DecoderSpecificInfo[1] |= sr_index<<7 | 0x10;

	return true;
}