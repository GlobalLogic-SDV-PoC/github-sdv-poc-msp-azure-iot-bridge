#include "azure_iot/client_iot.hpp"

#include <cassert>

#include "client_iot_impl.hpp"

namespace azure_iot
{
void ClientIot::connect()
{
    assert(!m_connection_string.empty());
    assert(m_handler);
    m_impl.reset(new ClientIotImpl(m_connection_string, m_handler));
    m_impl->start();
}
void ClientIot::disconnect() { m_impl.reset(); }
void ClientIot::subscribe(std::string_view topic)
{
    assert(m_impl);
    m_impl->subscribe(topic);
}
void ClientIot::unsubscribe(std::string_view topic)
{
    assert(m_impl);
    m_impl->unsubscribe(topic);
}
void ClientIot::publish(std::string_view topic, std::string_view payload)
{
    assert(m_impl);
    m_impl->publish(topic, payload);
}
void ClientIot::setOnReceivedHandler(const on_received_handler& handler) { m_handler = handler; }
void ClientIot::setConfig(const nlohmann::json& config) { m_connection_string = config["connection_string"]; }

ClientIot::~ClientIot() = default;
ClientIot::ClientIot() = default;
}  // namespace azure_iot
