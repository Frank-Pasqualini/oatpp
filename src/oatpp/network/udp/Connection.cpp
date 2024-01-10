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

#include <cstring>
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
}

Connection::~Connection() {
  close();
}

v_io_size Connection::write(const void* buff, const v_buff_size count, async::Action& action) {
  socklen_t len = sizeof(m_addr);

  const auto send_result = sendto(m_handle, buff, static_cast<size_t>(count), 0,
                                  reinterpret_cast<const struct sockaddr*>(&m_addr), len);

  char answer[count];
  recvfrom(m_handle, answer, static_cast<size_t>(count), 0, reinterpret_cast<sockaddr*>(&m_addr), &len);

  return send_result;
}

v_io_size Connection::read(void* buff, const v_buff_size count, async::Action& action) {
  socklen_t len = sizeof(m_addr);

  const auto recv_result = recvfrom(m_handle, buff, static_cast<size_t>(count), 0,
                                    reinterpret_cast<struct sockaddr*>(&m_addr), &len);

  sendto(m_handle, buff, static_cast<size_t>(count), 0, reinterpret_cast<const struct sockaddr*>(&m_addr), len);

  return recv_result;
}

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
  ::close(m_handle);
}

v_io_handle Connection::getHandle() const {
  return m_handle;
}

}}}
