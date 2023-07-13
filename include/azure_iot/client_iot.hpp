#ifndef _INCLUDE_AZURE_IOT_CLIENT_IOT_HPP_
#define _INCLUDE_AZURE_IOT_CLIENT_IOT_HPP_

#include <memory>
#include <string>

#include "iotb/interface/client_iot.hpp"

namespace azure_iot
{
class ClientIot : public ::iotb::IClientIot
{
    class ClientIotImpl;

public:
    void connect() override;
    void disconnect() override;
    void subscribe(std::string_view topic) override;
    void unsubscribe(std::string_view topic) override;
    void publish(std::string_view topic, std::string_view payload) override;
    void setOnReceivedHandler(const on_received_handler& handler) override;
    void setConfig(const nlohmann::json& config) override;

    ~ClientIot();
    ClientIot();

private:
    std::string m_connection_string;
    on_received_handler m_handler;
    std::unique_ptr<ClientIotImpl> m_impl;
};
}  // namespace azure_iot

#endif  //_INCLUDE_AZURE_IOT_CLIENT_IOT_HPP_
