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
#include "oatpp/core/data/stream/Stream.hpp"
#include "oatpp/core/provider/Provider.hpp"
#include "oatpp/core/utils/ConversionUtils.hpp"
#include "oatpp/network/Address.hpp"
#include "oatpp/network/udp/Connection.hpp"

#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <bits/sockaddr.h>
#include <netinet/in.h>
#include <sys/socket.h>

namespace oatpp { namespace network { namespace udp { namespace client {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider

ConnectionProvider::ConnectionProvider(const Address& address)
  : m_invalidator(std::make_shared<ConnectionInvalidator>())
    , m_address(address) {
  setProperty(PROPERTY_HOST, address.host);
  setProperty(PROPERTY_PORT, utils::conversion::int32ToStr(m_address.port));
}

std::shared_ptr<ConnectionProvider> ConnectionProvider::createShared(const Address& address) {
  return std::make_shared<ConnectionProvider>(address);
}

void ConnectionProvider::stop() {
  // DO NOTHING
}

provider::ResourceHandle<data::stream::IOStream> ConnectionProvider::get() {
  const auto port_str = utils::conversion::int32ToStr(m_address.port);

  addrinfo hints = {};
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

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

  addrinfo* result = nullptr;
  const auto res = getaddrinfo(m_address.host->c_str(), port_str->c_str(), &hints, &result);

  if (res != 0) {
#if defined(WIN32) || defined(_WIN32)
    throw std::runtime_error("[oatpp::network::udp::client::ConnectionProvider::get()]. "
                             "Error. Call to getaddrinfo() failed with code " + std::to_string(res));
#else
    std::string errorString =
      "[oatpp::network::udp::client::ConnectionProvider::get()]. Error. Call to getaddrinfo() failed: ";
    throw std::runtime_error(errorString.append(gai_strerror(res)));
#endif
  }

  if (result == nullptr) {
    throw std::runtime_error(
      "[oatpp::network::udp::client::ConnectionProvider::get()]. Error. Call to getaddrinfo() returned no results.");
  }

  const addrinfo* curr_result = result;
  v_io_handle clientHandle = INVALID_IO_HANDLE;
  sa_family_t family = 0;

  while (curr_result != nullptr) {
    clientHandle = socket(curr_result->ai_family, curr_result->ai_socktype, curr_result->ai_protocol);
    if (clientHandle >= 0) {
      family = curr_result->ai_family;
      break;
    }
    curr_result = curr_result->ai_next;
  }

  freeaddrinfo(result);

  if (curr_result == nullptr) {
    throw std::runtime_error("[oatpp::network::udp::client::ConnectionProvider::get()]: Error. Can't connect");
  }

#ifdef SO_NOSIGPIPE
  int yes = 1;
  v_int32 ret = setsockopt(clientHandle, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int));
  if(ret < 0) {
    OATPP_LOGD("[oatpp::network::udp::client::ConnectionProvider::get()]", "Warning. Failed to set %s for socket", "SO_NOSIGPIPE")
  }
#endif

  sockaddr_in addr{};
  addr.sin_family = family;
  addr.sin_port = htons(m_address.port);
  addr.sin_addr.s_addr = INADDR_ANY;

  return {std::make_shared<Connection>(clientHandle, addr), m_invalidator};
}

async::CoroutineStarterForResult<const provider::ResourceHandle<data::stream::IOStream>&>
ConnectionProvider::getAsync() {
  // TODO(fpasqualini)
  throw std::runtime_error("[oatpp::network::udp::client::ConnectionProvider::getAsync()]: Error. Not implemented.");
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
