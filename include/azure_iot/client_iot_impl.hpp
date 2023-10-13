#pragma once

#include <cassert>
#include <iotb/client_iot.hpp>
#include <iotb/context.hpp>
#include <memory>
#include <unordered_set>

#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_iot/client_iot.hpp"
#include "iothub.h"
#include "iothub_client_options.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransportmqtt.h"

namespace azure_iot
{
class AzureClientIotImpl
{
    using onReceivedHandler = iotb::IClientIot::onReceivedHandler;
    using DeviceHandler = std::unique_ptr<IOTHUB_CLIENT_CORE_LL_HANDLE_DATA_TAG, decltype(&IoTHubDeviceClient_LL_Destroy)>;

public:
    AzureClientIotImpl(const std::string& conn_string, const std::shared_ptr<iotb::Context>& ctx, const onReceivedHandler& handler);

    ~AzureClientIotImpl();
    void subscribe(iotb::Span topic);
    void unsubscribe(iotb::Span topic);
    void publish(iotb::Span topic, iotb::Span payload);

protected:
    // A receiving from IoTHub message (C2D) callback wrapper
    static IOTHUBMESSAGE_DISPOSITION_RESULT receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler,
                                                               void* user_context);

    // A confirmation of sending the message to IoTHub (D2C) callback wrapper
    static void sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* user_context);

    // A connect/disconnect to/from IoTHub callback wrapper
    static void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                         IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                         void* user_context);

    // A subscribe/unsubscribe to/from IoTHub specific topics callback wrapper
    static int32_t deviceMethodCallback(const char* method_name,
                                        const unsigned char* payload,
                                        size_t size,
                                        unsigned char** response,
                                        size_t* response_size,
                                        void* user_context);

    // A receiving from IoTHub message (C2D) callback
    // Also this method parse and analyze
    // whether the message meets the accepted standard
    IOTHUBMESSAGE_DISPOSITION_RESULT receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler);

    // A confirmation of sending the message to IoTHub (D2C) callback
    void sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result);

    // A connect/disconnect to/from IoTHub specific topics callback
    void connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                  IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);

    // A subscribe/unsubscribe to/from IoTHub specific topics callback
    int32_t deviceMethodCallback(const char* method_name,
                                 const unsigned char* payload,
                                 size_t size,
                                 unsigned char** response,
                                 size_t* response_size);

    // A specific method to send the message to IoTHub with Azure-sdk
    void sendIoTHubMessage(std::string message, IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handler);

    // A method to stop the communication process with an IoTHub
    // and to destroy the IoTHub instance
    void stop();

protected:
    static constexpr std::size_t MESSAGE_COUNT_SEND_CONFIRMATIONS = 1;

    std::shared_ptr<iotb::Context> m_ctx;

    std::string m_conn_string;
    onReceivedHandler m_on_received_handler;

    std::unordered_set<std::string> m_topicSet;

    DeviceHandler m_device_ll_handler;
};
}  // namespace azure_iot