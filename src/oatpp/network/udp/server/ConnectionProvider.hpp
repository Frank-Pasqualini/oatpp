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

#ifndef oatpp_network_udp_server_ConnectionProvider_hpp
#define oatpp_network_udp_server_ConnectionProvider_hpp

#include "oatpp/core/IODefinitions.hpp"
#include "oatpp/core/async/Coroutine.hpp"
#include "oatpp/core/data/stream/Stream.hpp"
#include "oatpp/core/provider/Invalidator.hpp"
#include "oatpp/core/provider/Provider.hpp"
#include "oatpp/network/Address.hpp"
#include "oatpp/network/ConnectionProvider.hpp"

#include <atomic>
#include <memory>

namespace oatpp { namespace network { namespace udp { namespace server {

/**
 * Simple provider of server UDP connections.
 */
class ConnectionProvider final : public ServerConnectionProvider {
public:

  /**
   * Constructor.
   * @param address - &id:oatpp::network::Address;.
   */
  explicit ConnectionProvider(Address address);

  /**
   * Create shared server ConnectionProvider.
   * @param address - &id:oatpp::network::Address;.
   * @return - &id:std::shared_ptr<oatpp::network::udp::server::ConnectionProvider>;.
   */
  static std::shared_ptr<ConnectionProvider> createShared(const Address& address);

  /**
   * Destructor.
   */
  ~ConnectionProvider() override;

  ConnectionProvider(const ConnectionProvider&) = delete;

  ConnectionProvider& operator=(const ConnectionProvider&) = delete;

  ConnectionProvider(ConnectionProvider&&) = delete;

  ConnectionProvider& operator=(ConnectionProvider&&) = delete;

  /**
   * Implements &id:oatpp::provider::Provider::stop;.
   */
  void stop() override;

  /**
   * Get connection.
   * @return &id:oatpp::provider::ResourceHandle<oatpp::data::stream::IOStream>;.
   */
  provider::ResourceHandle<data::stream::IOStream> get() override;

  /**
   * Get connection in asynchronous manner.
   * @return &id:oatpp::async::CoroutineStarterForResult<const oatpp::provider::ResourceHandle<oatpp::data::stream::IOStream>&>;.
   */
  async::CoroutineStarterForResult<const provider::ResourceHandle<data::stream::IOStream>&> getAsync() override;

private:

  class ConnectionInvalidator final : public provider::Invalidator<data::stream::IOStream> {
  public:

    void invalidate(const std::shared_ptr<data::stream::IOStream>& connection) override;

  };

  std::shared_ptr<ConnectionInvalidator> m_invalidator;
  Address m_address;
  std::atomic<bool> m_closed;
  v_io_handle m_serverHandle;
};

}}}}

#endif /* oatpp_netword_udp_server_ConnectionProvider_hpp */
