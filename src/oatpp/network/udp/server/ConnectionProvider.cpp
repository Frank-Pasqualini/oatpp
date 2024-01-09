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
#include "oatpp/network/Address.hpp"

#include <memory>

namespace oatpp { namespace network { namespace udp { namespace server {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider

ConnectionProvider::ConnectionProvider(const Address& address)
  : m_invalidator(std::make_shared<ConnectionInvalidator>())
    , m_address(address) {
  // TODO
}

std::shared_ptr<ConnectionProvider> ConnectionProvider::createShared(const Address& address) {
  return std::make_shared<ConnectionProvider>(address);
}

ConnectionProvider::~ConnectionProvider() {
  stop();
}

void ConnectionProvider::stop() {
  // TODO
}

provider::ResourceHandle<data::stream::IOStream> ConnectionProvider::get() {
  // TODO
  return nullptr;
}

async::CoroutineStarterForResult<const provider::ResourceHandle<data::stream::IOStream>&>
ConnectionProvider::getAsync() {
  // TODO
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ConnectionProvider::ConnectionInvalidator

void ConnectionProvider::ConnectionInvalidator::invalidate(const std::shared_ptr<data::stream::IOStream>& connection) {
  // TODO
}

}}}}
