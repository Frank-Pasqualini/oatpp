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

#include "ConnectionProvider.hpp"

#include "oatpp/core/IODefinitions.hpp"
#include "oatpp/core/async/Coroutine.hpp"
#include "oatpp/core/base/Environment.hpp"
#include "oatpp/core/data/stream/Stream.hpp"
#include "oatpp/core/provider/Provider.hpp"
#include "oatpp/core/utils/ConversionUtils.hpp"
#include "oatpp/network/Address.hpp"
#include "oatpp/network/udp/Connection.hpp"

#include <fcntl.h>
#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <utility>
#include <asm-generic/socket.h>
#include <bits/types/struct_timeval.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

namespace oatpp { namespace network { namespace udp { namespace server {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider

ConnectionProvider::ConnectionProvider(Address address)
  : m_invalidator(std::make_shared<ConnectionInvalidator>())
    , m_address(std::move(address))
    , m_closed(false)
    , m_serverHandle(INVALID_IO_HANDLE) {
  setProperty(PROPERTY_HOST, m_address.host);
  setProperty(PROPERTY_PORT, utils::conversion::int32ToStr(m_address.port));

#if defined(WIN32) || defined(_WIN32)
  // TODO(fpasqualini)
  throw std::runtime_error("[oatpp::network::udp::server::ConnectionProvider::ConnectionProvider()]: Error. Not implemented.");
#else

  constexpr int yes = 1;

  addrinfo* result = nullptr;
  addrinfo hints = {};

  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE;

  switch (m_address.family) {
    case Address::IP_4:
      hints.ai_family = AF_INET;
      break;
    case Address::IP_6:
      hints.ai_family = AF_INET6;
      break;
    case Address::UNSPEC:
    default:
      hints.ai_family = AF_UNSPEC;
  }

  const auto port_str = utils::conversion::int32ToStr(m_address.port);

  const auto res = getaddrinfo(m_address.host->c_str(), port_str->c_str(), &hints, &result);
  if (res != 0) {
    throw std::runtime_error(
      "[oatpp::network::udp::server::ConnectionProvider::ConnectionProvider()]: Error. Call to getaddrinfo() failed.");
  }

  const addrinfo* curr_result = result;
  while (curr_result != nullptr) {
    m_serverHandle = socket(curr_result->ai_family, curr_result->ai_socktype, curr_result->ai_protocol);
    if (m_serverHandle >= 0) {
      if (setsockopt(m_serverHandle, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) != 0) {
        OATPP_LOGW("[oatpp::network::udp::server::ConnectionProvider::ConnectionProvider()]",
                   "Warning. Failed to set %s for accepting socket", "SO_REUSEADDR")
      }

      if (bind(m_serverHandle, curr_result->ai_addr, curr_result->ai_addrlen) == 0) {
        break;
      }

      close(m_serverHandle);
    }

    curr_result = curr_result->ai_next;
  }

  freeaddrinfo(result);

  if (curr_result == nullptr) {
    throw std::runtime_error("[oatpp::network::udp::server::ConnectionProvider::ConnectionProvider()]: "
      "Error. Couldn't bind");
  }

  fcntl(m_serverHandle, F_SETFL, O_NONBLOCK);

  // Update port after binding (typicaly in case of port = 0)
  sockaddr_in addr{};
  v_sock_size s_in_len = sizeof(addr);
  getsockname(m_serverHandle, reinterpret_cast<sockaddr*>(&addr), &s_in_len);
  setProperty(PROPERTY_PORT, utils::conversion::int32ToStr(ntohs(addr.sin_port)));
#endif
}

std::shared_ptr<ConnectionProvider> ConnectionProvider::createShared(const Address& address) {
  return std::make_shared<ConnectionProvider>(address);
}

ConnectionProvider::~ConnectionProvider() {
  stop();
}

void ConnectionProvider::stop() {
  if (!m_closed) {
    m_closed = true;
#if defined(WIN32) || defined(_WIN32)
    closesocket(m_serverHandle);
#else
    close(m_serverHandle);
#endif
  }
}

provider::ResourceHandle<data::stream::IOStream> ConnectionProvider::get() {
  while (!m_closed) {
    fd_set set;
    timeval timeout{};
    FD_ZERO(&set);
    FD_SET(m_serverHandle, &set);

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    const auto res = select(
#if defined(WIN32) || defined(_WIN32)
      static_cast<int>(m_serverHandle + 1),
#else
      m_serverHandle + 1,
#endif
      &set,
      nullptr,
      nullptr,
      &timeout);

    if (res >= 0) {
      break;
    }
  }

  if (!isValidIOHandle(m_serverHandle)) {
    return nullptr;
  }

#ifdef SO_NOSIGPIPE
  int yes = 1;
  v_int32 ret = setsockopt(m_serverHandle, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int));
  if(ret < 0) {
    OATPP_LOGD("[oatpp::network::udp::server::ConnectionProvider::get()]", "Warning. Failed to set %s for socket", "SO_NOSIGPIPE")
  }
#endif
  constexpr sockaddr_in addr{};
  return {std::make_shared<Connection>(m_serverHandle, addr), m_invalidator};
}

async::CoroutineStarterForResult<const provider::ResourceHandle<data::stream::IOStream>&>
ConnectionProvider::getAsync() {
  /*
     *  No need to implement this.
     *  For Asynchronous IO in oatpp it is considered to be a good practice
     *  to accept connections in a seperate thread with the blocking accept()
     *  and then process connections in Asynchronous manner with non-blocking read/write
     *
     *  It may be implemented later
     */
  throw std::runtime_error("[oatpp::network::udp::server::ConnectionProvider::getAsync()]: Error. Not implemented.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider::ConnectionInvalidator

void ConnectionProvider::ConnectionInvalidator::invalidate(const std::shared_ptr<data::stream::IOStream>& connection) {
  const auto conn = std::static_pointer_cast<Connection>(connection);
  const v_io_handle handle = conn->getHandle();
#if defined(WIN32) || defined(_WIN32)
  shutdown(handle, SD_BOTH);
#else
  shutdown(handle, SHUT_RDWR);
#endif
}

}}}}
