/***************************************************************************
 *
 * Project         _____    __   ____   _      _
 *                (  _  )  /__\ (_  _)_| |_  _| |_
 *                 )(_)(  /(__)\  )( (_   _)(_   _)
 *                (_____)(__)(__)(__)  |_|    |_|
 *
 *
 * Copyright 2018-present, Leonid Stryzhevskyi <lganzzzo@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************************/

#include "Connection.hpp"

#include "oatpp/core/IODefinitions.hpp"
#include "oatpp/core/async/Coroutine.hpp"
#include "oatpp/core/base/Environment.hpp"
#include "oatpp/core/data/stream/Stream.hpp"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace oatpp { namespace network { namespace udp {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Connection

data::stream::DefaultInitializedContext Connection::DEFAULT_CONTEXT(data::stream::StreamType::STREAM_INFINITE);

Connection::Connection(const v_io_handle handle, const sockaddr_in addr)
  : m_handle(handle),
    m_addr(addr) {

#if defined(WIN32) || defined(_WIN32)

  // in Windows, there is no reliable method to get if a socket is blocking or not.
  // Eevery socket is created blocking in Windows so we assume this state and pray.

  setStreamIOMode(data::stream::BLOCKING);

#else

  const auto flags = fcntl(m_handle, F_GETFL);

  if (flags < 0) {
    throw std::runtime_error("[oatpp::network::udp::Connection::Connection()]: Error. Can't get socket flags.");
  }

  if ((flags & O_NONBLOCK) > 0) {
    m_mode = data::stream::IOMode::ASYNCHRONOUS;
  }
  else {
    m_mode = data::stream::IOMode::BLOCKING;
  }

#endif

}

Connection::~Connection() {
  close();
}

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wlogical-op"
#endif

v_io_size Connection::write(const void* buff, const v_buff_size count, async::Action& action) {
#if defined(WIN32) || defined(_WIN32)
  // TODO(fpasqualini)
  throw std::runtime_error("[oatpp::network::udp::Connection::write()]: Error. Not implemented.");
#else

  errno = 0;
  v_int32 flags = 0;

#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif

  const auto result = sendto(m_handle, buff, static_cast<size_t>(count), flags,
                             reinterpret_cast<const struct sockaddr*>(&m_addr), sizeof(m_addr));
  if (result < 0) {
    const auto err = errno;

    const bool retry = err == EAGAIN || err == EWOULDBLOCK;

    if (retry) {
      if (m_mode == data::stream::ASYNCHRONOUS) {
        action = async::Action::createIOWaitAction(m_handle, async::Action::IOEventType::IO_EVENT_WRITE);
      }
      return RETRY_WRITE; // For async io. In case socket is non-blocking
    }

    if (err == EINTR) {
      return RETRY_WRITE;
    }

    if (err == EPIPE) {
      return BROKEN_PIPE;
    }

    //OATPP_LOGD("Connection", "write errno=%d", e)
    return BROKEN_PIPE; // Consider all other errors as a broken pipe.
  }
  return result;
#endif
}

v_io_size Connection::read(void* buff, const v_buff_size count, async::Action& action) {
#if defined(WIN32) || defined(_WIN32)
  // TODO(fpasqualini)
  throw std::runtime_error("[oatpp::network::udp::Connection::read()]: Error. Not implemented.");
#else
  errno = 0;

  v_sock_size len = sizeof(m_addr);
  const auto result = recvfrom(m_handle, buff, static_cast<size_t>(count), 0,
                               reinterpret_cast<struct sockaddr*>(&m_addr), &len);
  if (result < 0) {
    const auto err = errno;

    const bool retry = err == EAGAIN || err == EWOULDBLOCK;

    if (retry) {
      if (m_mode == data::stream::ASYNCHRONOUS) {
        action = async::Action::createIOWaitAction(m_handle, async::Action::IOEventType::IO_EVENT_READ);
      }
      return RETRY_READ; // For async io. In case socket is non-blocking
    }

    if (err == EINTR) {
      return RETRY_READ;
    }

    if (err == ECONNRESET) {
      return BROKEN_PIPE;
    }

    //OATPP_LOGD("Connection", "write errno=%d", e)
    return BROKEN_PIPE; // Consider all other errors as a broken pipe.
  }

  return result;
#endif
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#if defined(WIN32) || defined(_WIN32)
void Connection::setStreamIOMode(oatpp::data::stream::IOMode ioMode) {

  u_long flags;

  switch(ioMode) {
    case data::stream::BLOCKING:
      flags = 0;
    if(NO_ERROR != ioctlsocket(m_handle, FIONBIO, &flags)) {
      throw std::runtime_error("[oatpp::network::tcp::Connection::setStreamIOMode()]: Error. Can't set stream I/O mode to IOMode::BLOCKING.");
    }
    m_mode = data::stream::BLOCKING;
    break;
    case data::stream::ASYNCHRONOUS:
      flags = 1;
    if(NO_ERROR != ioctlsocket(m_handle, FIONBIO, &flags)) {
      throw std::runtime_error("[oatpp::network::tcp::Connection::setStreamIOMode()]: Error. Can't set stream I/O mode to IOMode::ASYNCHRONOUS.");
    }
    m_mode = data::stream::ASYNCHRONOUS;
    break;
    default:
      break;
  }

}
#else
void Connection::setStreamIOMode(const data::stream::IOMode ioMode) {
  auto flags = fcntl(m_handle, F_GETFL);
  if (flags < 0) {
    throw std::runtime_error("[oatpp::network::udp::Connection::setStreamIOMode()]: Error. Can't get socket flags.");
  }

  switch (ioMode) {
    case data::stream::IOMode::BLOCKING:
      flags = flags & ~O_NONBLOCK;
      if (fcntl(m_handle, F_SETFL, flags) < 0) {
        throw std::runtime_error(
          "[oatpp::network::tcp::Connection::setStreamIOMode()]: Error. Can't set stream I/O mode to IOMode::BLOCKING.");
      }
      m_mode = data::stream::BLOCKING;
      break;

    case data::stream::IOMode::ASYNCHRONOUS:
      flags = flags | O_NONBLOCK;
      if (fcntl(m_handle, F_SETFL, flags) < 0) {
        throw std::runtime_error(
          "[oatpp::network::tcp::Connection::setStreamIOMode()]: Error. Can't set stream I/O mode to IOMode::ASYNCHRONOUS.");
      }
      m_mode = data::stream::ASYNCHRONOUS;
      break;

    default:
      break;
  }
}
#endif

void Connection::setOutputStreamIOMode(const data::stream::IOMode ioMode) {
  setStreamIOMode(ioMode);
}

data::stream::IOMode Connection::getOutputStreamIOMode() {
  return m_mode;
}

data::stream::Context& Connection::getOutputStreamContext() {
  return DEFAULT_CONTEXT;
}

void Connection::setInputStreamIOMode(const data::stream::IOMode ioMode) {
  setStreamIOMode(ioMode);
}

data::stream::IOMode Connection::getInputStreamIOMode() {
  return m_mode;
}

data::stream::Context& Connection::getInputStreamContext() {
  return DEFAULT_CONTEXT;
}

void Connection::close() const {
#if defined(WIN32) || defined(_WIN32)
  ::closesocket(m_handle);
#else
  ::close(m_handle);
#endif
}

v_io_handle Connection::getHandle() const {
  return m_handle;
}

}}}
