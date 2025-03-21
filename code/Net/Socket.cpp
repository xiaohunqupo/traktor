/*
 * TRAKTOR
 * Copyright (c) 2022 Anders Pistol.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */
#if defined(__LINUX__) || defined(__RPI__) || defined(__APPLE__) || defined(__ANDROID__)
#	include <sys/ioctl.h>
#endif
#include "Net/Platform.h"
#include "Net/Socket.h"

namespace traktor::net
{

T_IMPLEMENT_RTTI_CLASS(L"traktor.net.Socket", Socket, Object)

Socket::Socket()
:	m_socket(INVALID_SOCKET)
{
}

Socket::Socket(handle_t socket_)
:	m_socket(socket_)
{
}

Socket::~Socket()
{
	if (m_socket != INVALID_SOCKET)
	{
		CLOSE_SOCKET(m_socket);
		m_socket = INVALID_SOCKET;
	}
}

void Socket::close()
{
	if (m_socket != INVALID_SOCKET)
	{
#if defined(_WIN32)
		::shutdown(m_socket, SD_BOTH);
#elif !defined(__LINUX__) && !defined(__RPI__)
		::send(m_socket, 0, 0, 0);
#endif
		CLOSE_SOCKET(m_socket);
		m_socket = INVALID_SOCKET;
	}
}

int Socket::select(bool read, bool write, bool except, int timeout)
{
	timeval to = { timeout / 1000, (timeout % 1000) * 1000 };
	fd_set* fds[] = { 0, 0, 0 };
	fd_set readfds, writefds, exceptfds;

	if (read == true)
	{
		FD_ZERO(&readfds);
		FD_SET(m_socket, &readfds);
		fds[0] = &readfds;
	}

	if (write == true)
	{
		FD_ZERO(&writefds);
		FD_SET(m_socket, &writefds);
		fds[1] = &writefds;
	}

	if (except == true)
	{
		FD_ZERO(&exceptfds);
		FD_SET(m_socket, &exceptfds);
		fds[2] = &exceptfds;
	}

	return ::select(m_socket + 1, fds[0], fds[1], fds[2], &to);
}

int Socket::send(const void* data, int length)
{
	return int(::send(m_socket, static_cast<const char*>(data), length, 0));
}

int Socket::recv(void* data, int length)
{
	return int(::recv(m_socket, static_cast<char*>(data), length, 0));
}

int Socket::send(int byte)
{
	unsigned char tmp = static_cast<unsigned char>(byte);
	return send(&tmp, sizeof(tmp));
}

int Socket::recv()
{
	unsigned char tmp;
	int result = recv(&tmp, sizeof(tmp));
	return (result == sizeof(tmp)) ? tmp : -1;
}

bool Socket::ioctl(IoctlCommand cmd, unsigned long* argp)
{
#if defined(_WIN32)
	int ncmd;
	switch (cmd)
	{
	case IccNonBlockingIo:
		ncmd = FIONBIO;
		break;

	case IccReadPending:
		ncmd = FIONREAD;
		break;

	default:
		return false;
	}
	return bool(ioctlsocket(m_socket, ncmd, argp) == 0);
#elif defined(__LINUX__) || defined(__RPI__) || defined(__APPLE__) || defined(__ANDROID__)
	int ret = 0;
	switch (cmd)
	{
	case IccReadPending:
		if (::ioctl(m_socket, FIONREAD, &ret) >= 0)
		{
			*argp = ret;
			return true;
		}
		break;

	default:
		break;
	}
	return false;
#else
	return false;
#endif
}

Socket::handle_t Socket::handle() const
{
	return m_socket;
}

}
