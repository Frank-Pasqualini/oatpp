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

#include "oatpp/core/async/Coroutine.hpp"
#include "oatpp/core/data/stream/Stream.hpp"
#include "oatpp/core/provider/Invalidator.hpp"
#include "oatpp/core/provider/Provider.hpp"
#include "oatpp/core/utils/ConversionUtils.hpp"
#include "oatpp/network/Address.hpp"
#include "oatpp/network/udp/Connection.hpp"

#include <memory>
#include <netdb.h>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>

namespace oatpp { namespace network { namespace udp { namespace client {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider

ConnectionProvider::ConnectionProvider(const Address& address)
  : m_invalidator(std::make_shared<ConnectionInvalidator>())
    , m_address(address)
    , m_closed(false) {
  setProperty(PROPERTY_HOST, address.host);
  const auto portStr = utils::conversion::int32ToStr(m_address.port);
  setProperty(PROPERTY_PORT, portStr);

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

  addrinfo* result;
  const auto res = getaddrinfo(m_address.host->c_str(), portStr->c_str(), &hints, &result);

  if (res != 0) {
    std::string errorString =
      "[oatpp::network::udp::client::ConnectionProvider::ConnectionProvider()]. Error. Call to getaddrinfo() failed: ";
    throw std::runtime_error(errorString.append(gai_strerror(res)));
  }

  if (result == nullptr) {
    throw std::runtime_error(
      "[oatpp::network::udp::client::ConnectionProvider::ConnectionProvider()]. Error. Call to getaddrinfo() returned no results.");
  }

  const addrinfo* currResult = result;
  m_clientHandle = INVALID_IO_HANDLE;

  while (currResult != nullptr) {
    m_clientHandle = socket(currResult->ai_family, currResult->ai_socktype, currResult->ai_protocol);
    if (m_clientHandle >= 0) {
      m_addr = currResult->ai_addr;
      break;
    }
    currResult = currResult->ai_next;
  }

  freeaddrinfo(result);

  if (currResult == nullptr) {
    throw std::runtime_error(
      "[oatpp::network::udp::client::ConnectionProvider::ConnectionProvider()]: Error. Can't create socket");
  }

#ifdef SO_NOSIGPIPE
  int yes = 1;
  v_int32 ret = setsockopt(m_clientHandle, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(int));
  if(ret < 0) {
    OATPP_LOGD("[oatpp::network::udp::client::ConnectionProvider::ConnectionProvider()]", "Warning. Failed to set %s for socket", "SO_NOSIGPIPE")
  }
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
    close(m_clientHandle);
  }
}

provider::ResourceHandle<data::stream::IOStream> ConnectionProvider::get() {
  return provider::ResourceHandle<data::stream::IOStream>(
    std::make_shared<Connection>(m_clientHandle, m_addr),
    m_invalidator
  );
}

async::CoroutineStarterForResult<const provider::ResourceHandle<data::stream::IOStream>&>
ConnectionProvider::getAsync() {
  // TODO
  throw std::runtime_error("[oatpp::network::udp::client::ConnectionProvider::getAsync()]: Error. Not implemented.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider::ConnectionInvalidator

void ConnectionProvider::ConnectionInvalidator::invalidate(const std::shared_ptr<data::stream::IOStream>& connection) {
  // TODO
  throw std::runtime_error(
    "[oatpp::network::udp::server::ConnectionProvider::ConnectionInvalidator::invalidate()]: Error. Not implemented.");
}

}}}}
