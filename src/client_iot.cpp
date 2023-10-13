
#include "azure_iot/client_iot.hpp"

#include <memory>

#include "azure_iot/client_iot_impl.hpp"

namespace azure_iot
{
void AzureClientIot::connect(const std::shared_ptr<iotb::Context>& ctx, const nlohmann::json& config, const onReceivedHandler& rec)
{
    m_impl = std::make_shared<AzureClientIotImpl>(config["connection_string"].get_ref<const std::string&>(), ctx, rec);
}
void AzureClientIot::disconnect()
{
    m_impl.reset();
}
void AzureClientIot::subscribe(iotb::Span topic)
{
    m_impl->subscribe(topic);
}
void AzureClientIot::unsubscribe(iotb::Span topic)
{
    m_impl->unsubscribe(topic);
}
void AzureClientIot::publish(iotb::Span topic, iotb::Span payload)
{
    m_impl->publish(topic, payload);
}
}  // namespace aws_iot