
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
void AzureClientIot::subscribe(std::string topic)
{
    m_impl->subscribe(std::move(topic));
}
void AzureClientIot::unsubscribe(std::string topic)
{
    m_impl->unsubscribe(std::move(topic));
}
void AzureClientIot::publish(std::string topic, std::string payload)
{
    m_impl->publish(std::move(topic), std::move(payload));
}
}  // namespace azure_iot