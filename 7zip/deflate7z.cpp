#include "deflate7z.h"

#include "CPP/7zip/Archive/StdAfx.h"

#include "C/CpuArch.h"

#include "CPP/Common/MyInitGuid.h"
#include "CPP/Common/ComTry.h"
#include "CPP/Common/MyCom.h"

#include "CPP/Windows/PropVariant.h"

#include "CPP/7zip/IStream.h"
#include "CPP/7zip/Archive/DeflateProps.h"
#include "CPP/7zip/Compress/DeflateEncoder.h"

#include <propvarutil.h>

namespace Deflate7z {

static const wchar_t *propNames[] = {L"X", L"PASS", L"FB", L"A", L"MC"};

class CInBlockStream:
  public ISequentialInStream,
  public CMyUnknownImp
{
public:
	virtual ~CInBlockStream() {
	}

	void Init(const void *buffer, uint32_t size);

	MY_UNKNOWN_IMP1(ISequentialInStream)
	STDMETHOD(Read)(void *data, UInt32 size, UInt32 *processedSize);

private:
	const uint8_t *buffer_;
	uint32_t size_;
	uint32_t pos_;
};

class COutBlockStream:
  public ISequentialOutStream,
  public CMyUnknownImp
{
public:
	virtual ~COutBlockStream() {
	}

	void Init(void *buffer, uint32_t size);
	uint32_t DataSize() {
		return pos_;
	}

	MY_UNKNOWN_IMP1(ISequentialOutStream)
	STDMETHOD(Write)(const void *data, UInt32 size, UInt32 *processedSize);

private:
	uint8_t *buffer_;
	uint32_t size_;
	uint32_t pos_;
};

void CInBlockStream::Init(const void *buffer, uint32_t size) {
	buffer_ = reinterpret_cast<const uint8_t *>(buffer);
	size_ = size;
	pos_ = 0;
}

void COutBlockStream::Init(void *buffer, uint32_t size) {
	buffer_ = reinterpret_cast<uint8_t *>(buffer);
	size_ = size;
	pos_ = 0;
}

HRESULT CInBlockStream::Read(void *data, UInt32 size, UInt32 *processedSize) {
	if (pos_ + size > size_) {
		size = size_ - pos_;
	}
	if (size_ != 0) {
		memcpy(data, buffer_ + pos_, size);
		pos_ += size;
	}
	*processedSize = size;
	return S_OK;
}

HRESULT COutBlockStream::Write(const void *data, UInt32 size, UInt32 *processedSize) {
	if (pos_ + size > size_) {
		size = size_ - pos_;
	}
	if (size == 0) {
		*processedSize = size;
		return E_FAIL;
	}
	memcpy(buffer_ + pos_, data, size);
	pos_ += size;
	*processedSize = size;
	return S_OK;
}

struct Context {
	CInBlockStream *in;
	COutBlockStream *out;
	NCompress::NDeflate::NEncoder::CCOMCoder *coder;
};

void SetDefaults(Options *opts) {
	opts->level = 0xFFFFFFFF;
	opts->passes = 0xFFFFFFFF;
	opts->fastbytes = 0xFFFFFFFF;
	opts->algo = 0xFFFFFFFF;
	opts->matchcycles = 0xFFFFFFFF;
}

static void SetupProperties(const Options *opts, NArchive::CDeflateProps &deflateProps) {
	PROPVARIANT propValues[5] = {0};

	InitPropVariantFromUInt32(opts->level, &propValues[0]);
	InitPropVariantFromUInt32(opts->passes, &propValues[1]);
	InitPropVariantFromUInt32(opts->fastbytes, &propValues[2]);
	InitPropVariantFromUInt32(opts->algo, &propValues[3]);
	InitPropVariantFromUInt32(opts->matchcycles, &propValues[4]);
	deflateProps.SetProperties(propNames, propValues, 5);
}

void Alloc(Context **ctx, const Options *opts) {
	Context *c = new Context();
	*ctx = c;

	NArchive::CDeflateProps deflateProps;
	SetupProperties(opts, deflateProps);

	c->in = new CInBlockStream();
	c->out = new COutBlockStream();
	c->in->AddRef();
	c->out->AddRef();

	c->coder = new NCompress::NDeflate::NEncoder::CCOMCoder();
	deflateProps.SetCoderProperties(c->coder);
}

void Release(Context **ctx) {
	Context *c = *ctx;

	c->in->Release();
	c->out->Release();
	delete c->coder;

	delete *ctx;
	*ctx = nullptr;
}

bool Deflate(Context *ctx, void *dst, uint32_t dstSize, const void *src, uint32_t srcSize, uint32_t *resultSize) {
	ctx->in->Init(src, srcSize);
	ctx->out->Init(dst, dstSize);

	if (ctx->coder->Code(ctx->in, ctx->out, NULL, NULL, NULL) == SZ_OK) {
		*resultSize = ctx->out->DataSize();
		return true;
	}
	return false;
}

bool Deflate(const Options *opts, void *dst, uint32_t dstSize, const void *src, uint32_t srcSize, uint32_t *resultSize) {
	Context *ctx = nullptr;
	Alloc(&ctx, opts);
	bool success = Deflate(ctx, dst, dstSize, src, srcSize, resultSize);
	Release(&ctx);
	return success;
}

};
