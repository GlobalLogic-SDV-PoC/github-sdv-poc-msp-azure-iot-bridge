#include "azure_iot/client_iot_impl.hpp"

#include <cassert>
#include <rclcpp/logging.hpp>
#include <regex>

#include "azure_iot/client_iot_impl.hpp"

namespace azure_iot
{
AzureClientIotImpl::AzureClientIotImpl(const std::string& conn_string, const std::shared_ptr<iotb::Context>& ctx, const onReceivedHandler& handler)
    : m_ctx(ctx)
    , m_conn_string(conn_string)
    , m_on_received_handler(handler)
    , m_device_ll_handler(nullptr, &IoTHubDeviceClient_LL_Destroy)

{
    assert(handler);
    // Used to initialize IoTHub SDK subsystem
    if (IoTHub_Init() != 0)
    {
        RCLCPP_FATAL(m_ctx->node->get_logger(), "Failed to create IoTHub instance");
        exit(-1);
    }
    RCLCPP_INFO(m_ctx->node->get_logger(), "Creating IoTHub Device handle");
    // Create the iothub handle here
    m_device_ll_handler = DeviceHandler(
        IoTHubDeviceClient_LL_CreateFromConnectionString(m_conn_string.c_str(), MQTT_Protocol),
        &IoTHubDeviceClient_LL_Destroy);

    if (m_device_ll_handler == nullptr)
    {
        RCLCPP_FATAL(m_ctx->node->get_logger(), "Failure creating IotHub device. Hint: Check your connection string.");
        exit(-1);
    }
    else
    {
        // Set the log options
        // For available mqtt_client_connect options please see the
        // iothub_sdk_options.md documentation
        bool traceon = true;
        if (IoTHubDeviceClient_LL_SetOption(m_device_ll_handler.get(), OPTION_LOG_TRACE, &traceon) != IOTHUB_CLIENT_OK)
        {
            RCLCPP_FATAL(m_ctx->node->get_logger(), "IoTHubDeviceClient_LL_SetOption failed");
            exit(-1);
        }

        // Set the receiving message from Client to Device callback (from IoTHub)
        if (IoTHubDeviceClient_LL_SetMessageCallback(m_device_ll_handler.get(), receiveMsgCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            RCLCPP_FATAL(m_ctx->node->get_logger(), "Callback of receiving message set is failed");
            exit(-1);
        }

        // Set the subscribe to IoTHub common topics callback
        if (IoTHubDeviceClient_LL_SetDeviceMethodCallback(m_device_ll_handler.get(), deviceMethodCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            RCLCPP_FATAL(m_ctx->node->get_logger(), "There is a problem with subscription to IoTHub");
            exit(-1);
        }

        // Set the connect to IoTHub callback
        if (IoTHubDeviceClient_LL_SetConnectionStatusCallback(m_device_ll_handler.get(), connectionStatusCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            RCLCPP_FATAL(m_ctx->node->get_logger(), "There is a problem with receiving the messages from IoTHub");
            exit(-1);
        }

        // Send the first stub message to IoTHub for the instant connection after the all setup
        std::string message = "Connection message";
        sendIoTHubMessage(std::move(message), m_device_ll_handler.get());
    }
}

AzureClientIotImpl::~AzureClientIotImpl()
{
    // Set the unsubscribe from IoTHub common topics callback
    IoTHubDeviceClient_LL_SetDeviceMethodCallback(m_device_ll_handler.get(), nullptr, nullptr);

    // Free all the sdk subsystem
    IoTHub_Deinit();
}

void AzureClientIotImpl::subscribe(iotb::Span topic) { m_topicSet.emplace(static_cast<const char*>(topic.buf), topic.len); }

void AzureClientIotImpl::unsubscribe(iotb::Span topic) { m_topicSet.erase(std::string(static_cast<const char*>(topic.buf), topic.len)); }

void AzureClientIotImpl::publish(iotb::Span topic, iotb::Span payload)
{
    std::string message = "@" + std::string(static_cast<const char*>(topic.buf), topic.len) + "@" + std::to_string(payload.len) + "@" + std::string(std::string(static_cast<const char*>(payload.buf), payload.len));

    sendIoTHubMessage(std::move(message), m_device_ll_handler.get());
}

IOTHUBMESSAGE_DISPOSITION_RESULT AzureClientIotImpl::receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler,
                                                                        void* user_context)
{
    auto* self = static_cast<AzureClientIotImpl*>(user_context);
    return self->receiveMsgCallback(message_handler);
}

void AzureClientIotImpl::sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* user_context)
{
    auto* self = static_cast<AzureClientIotImpl*>(user_context);
    self->sendConfirmCallback(result);
}

void AzureClientIotImpl::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                                  IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                                  void* user_context)
{
    auto* self = static_cast<AzureClientIotImpl*>(user_context);
    self->connectionStatusCallback(result, reason);
}

int32_t AzureClientIotImpl::deviceMethodCallback(const char* method_name,
                                                 const unsigned char* payload,
                                                 size_t size,
                                                 unsigned char** response,
                                                 size_t* response_size,
                                                 void* user_context)
{
    auto* self = static_cast<AzureClientIotImpl*>(user_context);
    return self->deviceMethodCallback(method_name, payload, size, response, response_size);
}

IOTHUBMESSAGE_DISPOSITION_RESULT AzureClientIotImpl::receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler)
{
    // Get the content type of the receiving message
    // It might be only byte array type
    const auto contenttype = IoTHubMessage_GetContentType(message_handler);
    if (contenttype != IOTHUBMESSAGE_BYTEARRAY)
    {
        RCLCPP_ERROR(m_ctx->node->get_logger(), "Can't recognize a message");
        return IOTHUBMESSAGE_REJECTED;
    }

    const unsigned char* buffmsg;
    size_t bufflen;

    // Read the message from stream
    if (IoTHubMessage_GetByteArray(message_handler, &buffmsg, &bufflen) != IOTHUB_MESSAGE_OK)
    {
        RCLCPP_ERROR(m_ctx->node->get_logger(), "Failure retrieving message");
        return IOTHUBMESSAGE_REJECTED;
    }
    // TODO: fix this memory disaster
    // Create the string from the byte array message
    std::string message(static_cast<const char*>(static_cast<const void*>(buffmsg)), bufflen);
    // TODO: (Andrew) what the actual fuck is this regex parsing
    const std::regex messageformat("@([a-zA-Z\\-_/0-9]+)@([0-9]+)@(.*)");
    std::smatch messageparts;

    if (!std::regex_match(message, messageparts, messageformat) || messageparts.size() != 4)
    {
        RCLCPP_ERROR(m_ctx->node->get_logger(), "Wrong message format");
        return IOTHUBMESSAGE_REJECTED;
    }

    std::string topic_name = messageparts[1];
    std::string payload = messageparts[3];

    // If the topic is not found in a set of receiving topics, the message is broken.
    if (m_topicSet.find(topic_name) == m_topicSet.end())
    {
        RCLCPP_ERROR(m_ctx->node->get_logger(), "No registered topic to send");
        return IOTHUBMESSAGE_REJECTED;
    }

    // Send the topic and payload to ipc
    m_on_received_handler({topic_name.data(), topic_name.size()}, {payload.data(), payload.size()});

    // Returning IOTHUBMESSAGE_ACCEPTED causes the SDK to acknowledge receipt of
    // the message to the service.  The application does not need to take
    // further action tstatico ACK at this point.
    return IOTHUBMESSAGE_ACCEPTED;
}

void AzureClientIotImpl::sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
    // When a message is sent this callback will get invoked
    RCLCPP_DEBUG(m_ctx->node->get_logger(), "Confirmation callback received for message %zu with the result %s", MESSAGE_COUNT_SEND_CONFIRMATIONS,
                 MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

void AzureClientIotImpl::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                                  IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
    (void)reason;
    // This sample DOES NOT take into consideration network outages.
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
    {
        RCLCPP_INFO(m_ctx->node->get_logger(), "The device client is connected to iothub");
    }
    else
    {
        RCLCPP_INFO(m_ctx->node->get_logger(), "The device client has been disconnected");
    }
}

int32_t AzureClientIotImpl::deviceMethodCallback(const char* method_name,
                                                 const unsigned char* payload,
                                                 size_t size,
                                                 unsigned char** response,
                                                 size_t* response_size)
{
    (void)method_name;
    (void)payload;
    (void)size;
    (void)response;
    (void)response_size;
    return 200;
}

void AzureClientIotImpl::sendIoTHubMessage(std::string message, IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handler)
{
    size_t messagesent = 0;
    bool continuerunning = true;
    do
    {
        // Check whether the message was sent
        if (messagesent < MESSAGE_COUNT_SEND_CONFIRMATIONS)
        {
            // Create a message (payload)
            std::unique_ptr<IOTHUB_MESSAGE_HANDLE_DATA_TAG, decltype(&IoTHubMessage_Destroy)> message_handler(
                IoTHubMessage_CreateFromByteArray(static_cast<const unsigned char*>(static_cast<void*>(message.data())),
                                                  message.size()),
                IoTHubMessage_Destroy);

            if (!message_handler)
            {
                RCLCPP_ERROR(m_ctx->node->get_logger(), "Failed to create a message to IoTHub");
                return;
            }

            // Send the message to IoTHub and set the confirmation callback
            if (IoTHubDeviceClient_LL_SendEventAsync(device_ll_handler, message_handler.get(), sendConfirmCallback,
                                                     this)
                != IOTHUB_CLIENT_OK)
            {
                RCLCPP_ERROR(m_ctx->node->get_logger(), "Failed to send message to IoTHub");
                return;
            }

            // Incrementing the variable. The message has been sent successfully
            messagesent++;
        }
        else if (messagesent >= MESSAGE_COUNT_SEND_CONFIRMATIONS)
        {
            // If the message has been already sent, stop the next attempt
            continuerunning = false;
        }

        // The main function of azure-iot-sdk
        // It executes all the set and sending mechanisms
        IoTHubDeviceClient_LL_DoWork(m_device_ll_handler.get());

        // Waiting for CONNACK and confirmation callback messages
        ThreadAPI_Sleep(5000);
    } while (continuerunning);
}

}  // namespace azure_iot