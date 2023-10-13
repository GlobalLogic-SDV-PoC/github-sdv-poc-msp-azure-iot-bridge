#pragma once
#include <cstdint>
#include <functional>
#include <iotb/client_iot.hpp>
#include <memory>

namespace azure_iot
{
class AzureClientIotImpl;

class AzureClientIot : public iotb::IClientIot
{
public:
    void connect(const std::shared_ptr<iotb::Context>& ctx, const nlohmann::json& config, const onReceivedHandler& rec) override;
    void disconnect() override;
    void subscribe(iotb::Span topic) override;
    void unsubscribe(iotb::Span topic) override;
    void publish(iotb::Span topic, iotb::Span payload) override;

private:
    std::shared_ptr<AzureClientIotImpl> m_impl;
};

}  // namespace aws_iot