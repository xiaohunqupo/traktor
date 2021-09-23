#include <algorithm>
#include <cstring>
#include <sstream>
#include "Core/Log/Log.h"
#include "Core/Misc/TString.h"
#include "Core/Thread/Acquire.h"
#include "Editor/Pipeline/Memcached/MemcachedPipelineCache.h"
#include "Editor/Pipeline/Memcached/MemcachedPutStream.h"
#include "Editor/Pipeline/Memcached/MemcachedProto.h"


namespace traktor
{
	namespace editor
	{

T_IMPLEMENT_RTTI_CLASS(L"traktor.editor.MemcachedPutStream", MemcachedPutStream, IStream)

MemcachedPutStream::MemcachedPutStream(MemcachedPipelineCache* cache, MemcachedProto* proto, const std::string& key)
:	m_cache(cache)
,	m_proto(proto)
,	m_key(key)
,	m_inblock(0)
,	m_index(0)
{
}

void MemcachedPutStream::close()
{
	if (!m_proto)
		return;

	flush();
	uploadEndBlock();

	{
		T_ANONYMOUS_VAR(Acquire< Semaphore >)(m_cache->m_lock);
		m_cache->m_protos.push_back(m_proto);
		m_proto = nullptr;
	}
}

bool MemcachedPutStream::canRead() const
{
	return false;
}

bool MemcachedPutStream::canWrite() const
{
	return true;
}

bool MemcachedPutStream::canSeek() const
{
	return false;
}

int64_t MemcachedPutStream::tell() const
{
	return 0;
}

int64_t MemcachedPutStream::available() const
{
	return 0;
}

int64_t MemcachedPutStream::seek(SeekOriginType origin, int64_t offset)
{
	return 0;
}

int64_t MemcachedPutStream::read(void* block, int64_t nbytes)
{
	return 0;
}

int64_t MemcachedPutStream::write(const void* block, int64_t nbytes)
{
	const uint8_t* blockPtr = static_cast< const uint8_t* >(block);
	int64_t written = 0;

	while (written < nbytes)
	{
		int64_t avail = MaxBlockSize - m_inblock;
		int64_t copy = std::min(nbytes - written, avail);

		std::memcpy(&m_block[m_inblock], blockPtr, copy);

		blockPtr += copy;
		m_inblock += copy;
		written += copy;

		if (m_inblock >= MaxBlockSize)
		{
			if (uploadBlock())
				m_inblock = 0;
			else
				break;
		}
	}

	return written;
}

void MemcachedPutStream::flush()
{
	if (m_inblock > 0)
		uploadBlock();
}

bool MemcachedPutStream::uploadBlock()
{
	if (!m_proto)
		return false;

	std::stringstream ss;
	std::string command;
	std::string reply;

	ss << "set " << m_key << ":" << m_index << " 0 0 " << m_inblock;

	command = ss.str();
	T_DEBUG(mbstows(command));

	if (!m_proto->sendCommand(command))
	{
		log::error << L"Unable to store cache block; unable to send command." << Endl;
		m_proto = nullptr;
		return false;
	}

	if (!m_proto->writeData(m_block, m_inblock))
	{
		log::error << L"Unable to store cache block; unable to write data." << Endl;
		m_proto = nullptr;
		return false;
	}

	if (!m_proto->readReply(reply))
	{
		log::error << L"Unable to store cache block; unable to read reply." << Endl;
		m_proto = nullptr;
		return false;
	}

	if (reply != "STORED")
	{
		log::error << L"Unable to store cache block; server unable to store data." << Endl;
		m_proto = nullptr;
		return false;
	}

	m_index++;
	return true;
}

void MemcachedPutStream::uploadEndBlock()
{
	if (!m_proto)
		return;

	std::stringstream ss;
	std::string command;
	std::string reply;

	ss << "set " << m_key << ":END 0 0 1";

	command = ss.str();
	T_DEBUG(mbstows(command));

	if (!m_proto->sendCommand(command))
	{
		log::error << L"Unable to store cache block; unable to send command." << Endl;
		m_proto = nullptr;
		return;
	}

	uint8_t endData[3] = { 0x11, 0x00, 0x00 };
	if (!m_proto->writeData(endData, 1))
	{
		log::error << L"Unable to store cache block; unable to write data." << Endl;
		m_proto = nullptr;
		return;
	}

	if (!m_proto->readReply(reply))
	{
		log::error << L"Unable to store cache block; unable to read reply." << Endl;
		m_proto = nullptr;
		return;
	}

	if (reply != "STORED")
	{
		log::error << L"Unable to store cache block; server unable to store data." << Endl;
		m_proto = nullptr;
		return;
	}
}

	}
}