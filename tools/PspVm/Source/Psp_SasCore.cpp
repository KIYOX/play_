#include "Psp_SasCore.h"
#include "Log.h"

#define LOGNAME	("Psp_SasCore")

using namespace Psp;

CSasCore::CSasCore(uint8* ram)
: m_spuRam(NULL)
, m_ram(ram)
, m_grain(0)
{
	m_spuRam = new uint8[SPURAMSIZE];
	memset(m_spuRam, 0, SPURAMSIZE);
	m_spu[0] = new Iop::CSpuBase(m_spuRam, SPURAMSIZE);
	m_spu[1] = new Iop::CSpuBase(m_spuRam, SPURAMSIZE);

	m_spu[0]->Reset();
	m_spu[1]->Reset();

	SPUMEMBLOCK endBlock;
	endBlock.address	= SPURAMSIZE;
	endBlock.size		= -1;
	m_blocks.push_back(endBlock);
}

CSasCore::~CSasCore()
{
	if(m_spuRam != NULL)
	{
		delete [] m_spuRam;
		m_spuRam = NULL;
	}

	if(m_spu[0] != NULL)
	{
		delete m_spu[0];
		m_spu[0] = NULL;
	}

	if(m_spu[1] != NULL)
	{
		delete m_spu[1];
		m_spu[1] = NULL;
	}
}

std::string CSasCore::GetName() const
{
	return "sceSasCore";
}

Iop::CSpuBase::CHANNEL* CSasCore::GetSpuChannel(uint32 channelId)
{
	assert(channelId < 32);
	if(channelId >= 32)
	{
		return NULL;
	}

	Iop::CSpuBase* spuBase(NULL);
	if(channelId < 24)
	{
		spuBase = m_spu[0];
	}
	else
	{
		spuBase = m_spu[1];
		channelId -= 24;
	}

	return &spuBase->GetChannel(channelId);
}

uint32 CSasCore::AllocMemory(uint32 size)
{
	const uint32 startAddress = 0x1000;
	uint32 currentAddress = startAddress;
	MemBlockList::iterator blockIterator(m_blocks.begin());
	while(blockIterator != m_blocks.end())
	{
		const SPUMEMBLOCK& block(*blockIterator);
		uint32 space = block.address - currentAddress;
		if(space >= size)
		{
			SPUMEMBLOCK newBlock;
			newBlock.address	= currentAddress;
			newBlock.size		= size;
			m_blocks.insert(blockIterator, newBlock);
			return currentAddress;
		}
		currentAddress += block.size;
		blockIterator++;
	}
	assert(0);
	return 0;
}

void CSasCore::FreeMemory(uint32 address)
{
	for(MemBlockList::iterator blockIterator(m_blocks.begin());
		blockIterator != m_blocks.end(); blockIterator++)
	{
		const SPUMEMBLOCK& block(*blockIterator);
		if(block.address == address)
		{
			m_blocks.erase(blockIterator);
			return;
		}
	}
	assert(0);
}

uint32 CSasCore::Init(uint32 contextAddr, uint32 grain, uint32 unknown2, uint32 unknown3, uint32 frequency)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "Init(contextAddr = 0x%0.8X, grain = %d, unk = 0x%0.8X, unk = 0x%0.8X, frequency = %d);\r\n",
		contextAddr, grain, unknown2, unknown3, frequency);
#endif
	m_grain = grain;
	return 0;
}

uint32 CSasCore::Core(uint32 contextAddr, uint32 bufferAddr)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "Core(contextAddr = 0x%0.8X, bufferAddr = 0x%0.8X);\r\n", contextAddr, bufferAddr);
#endif

	assert(bufferAddr != 0);
	if(bufferAddr == 0)
	{
		return -1;
	}

	int16* buffer = reinterpret_cast<int16*>(m_ram + bufferAddr);
	for(unsigned int i = 0; i < 1; i++)
	{
		m_spu[i]->Render(buffer, m_grain * 2, 44100);
	}

	return 0;
}

uint32 CSasCore::SetVoice(uint32 contextAddr, uint32 voice, uint32 dataPtr, uint32 dataSize, uint32 loop)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetVoice(contextAddr = 0x%0.8X, voice = %d, dataPtr = 0x%0.8X, dataSize = 0x%0.8X, loop = %d);\r\n",
		contextAddr, voice, dataPtr, dataSize, loop);
#endif
	assert(dataPtr != NULL);
	if(dataPtr == NULL)
	{
		return -1;
	}

	Iop::CSpuBase::CHANNEL* channel = GetSpuChannel(voice);
	if(channel == NULL) return -1;
	uint8* samples = m_ram + dataPtr;

	uint32 currentAddress = channel->address;
	uint32 allocationSize = ((dataSize + 0xF) / 0x10) * 0x10;

	if(currentAddress != 0)
	{
		FreeMemory(currentAddress);
	}

	currentAddress = AllocMemory(dataSize);
	assert(currentAddress != 0);
	if(currentAddress != 0)
	{
		memcpy(m_spuRam + currentAddress, samples, dataSize);
	}
	channel->address = currentAddress;

	return 0;
}

uint32 CSasCore::SetPitch(uint32 contextAddr, uint32 voice, uint32 pitch)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetPitch(contextAddr = 0x%0.8X, voice = %d, pitch = 0x%0.4X);\r\n",
		contextAddr, voice, pitch);
#endif
	Iop::CSpuBase::CHANNEL* channel = GetSpuChannel(voice);
	if(channel == NULL) return -1;
	channel->pitch = pitch;
	return 0;
}

uint32 CSasCore::SetVolume(uint32 contextAddr, uint32 voice, uint32 left, uint32 right, uint32 effectLeft, uint32 effectRight)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetVolume(contextAddr = 0x%0.8X, voice = %d, left = 0x%0.4X, right = 0x%0.4X, effectLeft = 0x%0.4X, effectRight = 0x%0.4X);\r\n",
		contextAddr, voice, left, right, effectLeft, effectRight);
#endif
	Iop::CSpuBase::CHANNEL* channel = GetSpuChannel(voice);
	if(channel == NULL) return -1;
	channel->volumeLeft <<= left;
	channel->volumeRight <<= right;
	return 0;
}

uint32 CSasCore::SetSimpleADSR(uint32 contextAddr, uint32 voice, uint32 adsr1, uint32 adsr2)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetSimpleADSR(contextAddr = 0x%0.8X, voice = %d, adsr1 = 0x%0.4X, adsr2 = 0x%0.4X);\r\n",
		contextAddr, voice, adsr1, adsr2);
#endif
	Iop::CSpuBase::CHANNEL* channel = GetSpuChannel(voice);
	if(channel == NULL) return -1;
	channel->adsrLevel <<= adsr1;
	channel->adsrRate <<= adsr2;
	return 0;
}

uint32 CSasCore::SetKeyOn(uint32 contextAddr, uint32 voice)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetKeyOn(contextAddr = 0x%0.8X, voice = %d);\r\n", contextAddr, voice);
#endif

	assert(voice < 32);
	if(voice >= 32)
	{
		return -1;
	}

	Iop::CSpuBase* spuBase(NULL);
	if(voice < 24)
	{
		spuBase = m_spu[0];
	}
	else
	{
		spuBase = m_spu[1];
		voice -= 24;
	}

	spuBase->SendKeyOn(1 << voice);

	return 0;
}

uint32 CSasCore::SetKeyOff(uint32 contextAddr, uint32 voice)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "SetKeyOff(contextAddr = 0x%0.8X, voice = %d);\r\n", contextAddr, voice);
#endif

	assert(voice < 32);
	if(voice >= 32)
	{
		return -1;
	}

	Iop::CSpuBase* spuBase(NULL);
	if(voice < 24)
	{
		spuBase = m_spu[0];
	}
	else
	{
		spuBase = m_spu[1];
		voice -= 24;
	}

	spuBase->SendKeyOff(1 << voice);

	return 0;
}

uint32 CSasCore::GetAllEnvelope(uint32 contextAddr, uint32 envelopeAddr)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "GetAllEnvelope(contextAddr = 0x%0.8X, envelopeAddr = 0x%0.8X);\r\n", contextAddr, envelopeAddr);
#endif
	assert(envelopeAddr != 0);
	if(envelopeAddr == 0)
	{
		return -1;
	}
	uint32* envelope = reinterpret_cast<uint32*>(m_ram + envelopeAddr);
	for(unsigned int i = 0; i < 32; i++)
	{
		Iop::CSpuBase::CHANNEL* channel = GetSpuChannel(i);
		envelope[i] = channel->adsrVolume;
	}
	return 0;
}

uint32 CSasCore::GetPauseFlag(uint32 contextAddr)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "GetPauseFlag(contextAddr = 0x%0.8X);\r\n", contextAddr);
#endif
	return 0;
}

uint32 CSasCore::GetEndFlag(uint32 contextAddr)
{
#ifdef _DEBUG
	CLog::GetInstance().Print(LOGNAME, "GetEndFlag(contextAddr = 0x%0.8X);\r\n", contextAddr);
#endif
	uint32 result = m_spu[0]->GetEndFlags().f | (m_spu[1]->GetEndFlags().f << 24);
	return result;
}

void CSasCore::Invoke(uint32 methodId, CMIPS& context)
{
	switch(methodId)
	{
	case 0x42778A9F:
		context.m_State.nGPR[CMIPS::V0].nV0 = Init(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0,
			context.m_State.nGPR[CMIPS::A2].nV0,
			context.m_State.nGPR[CMIPS::A3].nV0,
			context.m_State.nGPR[CMIPS::T0].nV0);
		break;
	case 0xA3589D81:
		context.m_State.nGPR[CMIPS::V0].nV0 = Core(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0);
		break;
	case 0x99944089:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetVoice(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0,
			context.m_State.nGPR[CMIPS::A2].nV0,
			context.m_State.nGPR[CMIPS::A3].nV0,
			context.m_State.nGPR[CMIPS::T0].nV0);
		break;
	case 0xAD84D37F:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetPitch(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0,
			context.m_State.nGPR[CMIPS::A2].nV0);
		break;
	case 0x440CA7D8:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetVolume(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0,
			context.m_State.nGPR[CMIPS::A2].nV0,
			context.m_State.nGPR[CMIPS::A3].nV0,
			context.m_State.nGPR[CMIPS::T0].nV0,
			context.m_State.nGPR[CMIPS::T1].nV0);
		break;
	case 0xCBCD4F79:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetSimpleADSR(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0,
			context.m_State.nGPR[CMIPS::A2].nV0,
			context.m_State.nGPR[CMIPS::A3].nV0);
		break;
	case 0x76F01ACA:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetKeyOn(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0);
		break;
	case 0xA0CF2FA4:
		context.m_State.nGPR[CMIPS::V0].nV0 = SetKeyOff(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0);
		break;
	case 0x07F58C24:
		context.m_State.nGPR[CMIPS::V0].nV0 = GetAllEnvelope(
			context.m_State.nGPR[CMIPS::A0].nV0,
			context.m_State.nGPR[CMIPS::A1].nV0);
		break;
	case 0x2C8E6AB3:
		context.m_State.nGPR[CMIPS::V0].nV0 = GetPauseFlag(
			context.m_State.nGPR[CMIPS::A0].nV0);
		break;
	case 0x68A46B95:
		context.m_State.nGPR[CMIPS::V0].nV0 = GetEndFlag(
			context.m_State.nGPR[CMIPS::A0].nV0);
		break;
	default:
		CLog::GetInstance().Print(LOGNAME, "Unknown function called 0x%0.8X\r\n", methodId);
		break;
	}
}