#include "client_iot_impl.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <regex>

#include "spdlog/spdlog.h"

namespace azure_iot
{
ClientIot::ClientIotImpl::ClientIotImpl(const std::string& conn_string, const on_received_handler& handler)
    : m_conn_string(conn_string)
    , m_on_received_handler(handler)
    , m_device_ll_handler(nullptr, &IoTHubDeviceClient_LL_Destroy)

{
    assert(handler);
}

ClientIot::ClientIotImpl::~ClientIotImpl() { stop(); }

void ClientIot::ClientIotImpl::start()
{
    // Used to initialize IoTHub SDK subsystem
    if (IoTHub_Init() != 0)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Failed to create IoTHub instance");
    }

    SPDLOG_DEBUG("[iotb][azure_iot] Creating IoTHub Device handle");
    // Create the iothub handle here
    m_device_ll_handler = decltype(m_device_ll_handler)(
        IoTHubDeviceClient_LL_CreateFromConnectionString(m_conn_string.c_str(), MQTT_Protocol),
        &IoTHubDeviceClient_LL_Destroy);

    if (m_device_ll_handler == nullptr)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Failure creating IotHub device. Hint: Check your connection string.");
    }

    else
    {
        // Set the log options
        // For available mqtt_client_connect options please see the
        // iothub_sdk_options.md documentation
        bool traceon = true;
        if (IoTHubDeviceClient_LL_SetOption(m_device_ll_handler.get(), OPTION_LOG_TRACE, &traceon) != IOTHUB_CLIENT_OK)
        {
            SPDLOG_CRITICAL("[iotb][azure_iot] IoTHubDeviceClient_LL_SetOption failed");
            return;
        }

        // Set the receiving message from Client to Device callback (from IoTHub)
        if (IoTHubDeviceClient_LL_SetMessageCallback(m_device_ll_handler.get(), receiveMsgCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            SPDLOG_CRITICAL("[iotb][azure_iot] Callback of receiving message set is failed");
            return;
        }

        // Set the subscribe to IoTHub common topics callback
        if (IoTHubDeviceClient_LL_SetDeviceMethodCallback(m_device_ll_handler.get(), deviceMethodCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            SPDLOG_CRITICAL("[iotb][azure_iot] There is a problem with subscription to IoTHub");
            return;
        }

        // Set the connect to IoTHub callback
        if (IoTHubDeviceClient_LL_SetConnectionStatusCallback(m_device_ll_handler.get(), connectionStatusCallback, this)
            != IOTHUB_CLIENT_OK)
        {
            SPDLOG_CRITICAL("[iotb][azure_iot] There is a problem with receiving the messages from IoTHub");
            return;
        }

        // Send the first stub message to IoTHub for the instant connection after the all setup
        std::string message = "Connection message";
        sendIoTHubMessage(std::move(message), m_device_ll_handler.get());
    }
}

void ClientIot::ClientIotImpl::subscribe(std::string_view topic) { m_topicSet.emplace(topic); }

void ClientIot::ClientIotImpl::unsubscribe(std::string_view topic) { m_topicSet.erase(std::string(topic)); }

void ClientIot::ClientIotImpl::publish(std::string_view topic, std::string_view payload)
{
    std::string message = "@" + std::string(topic) + "@" + std::to_string(payload.size()) + "@" + std::string(payload);

    sendIoTHubMessage(std::move(message), m_device_ll_handler.get());
}

IOTHUBMESSAGE_DISPOSITION_RESULT ClientIot::ClientIotImpl::receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler,
                                                                              void* user_context)
{
    auto* self = reinterpret_cast<ClientIotImpl*>(user_context);
    return self->receiveMsgCallback(message_handler);
}

void ClientIot::ClientIotImpl::sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* user_context)
{
    auto* self = reinterpret_cast<ClientIotImpl*>(user_context);
    self->sendConfirmCallback(result);
}

void ClientIot::ClientIotImpl::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                                        IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason,
                                                        void* user_context)
{
    auto* self = reinterpret_cast<ClientIotImpl*>(user_context);
    self->connectionStatusCallback(result, reason);
}

int32_t ClientIot::ClientIotImpl::deviceMethodCallback(const char* method_name,
                                                       const unsigned char* payload,
                                                       size_t size,
                                                       unsigned char** response,
                                                       size_t* response_size,
                                                       void* user_context)
{
    auto* self = reinterpret_cast<ClientIotImpl*>(user_context);
    return self->deviceMethodCallback(method_name, payload, size, response, response_size);
}

IOTHUBMESSAGE_DISPOSITION_RESULT ClientIot::ClientIotImpl::receiveMsgCallback(IOTHUB_MESSAGE_HANDLE message_handler)
{
    // Get the content type of the receiving message
    // It might be only byte array type
    const auto contenttype = IoTHubMessage_GetContentType(message_handler);
    if (contenttype != IOTHUBMESSAGE_BYTEARRAY)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Can't recognize a message");
        return IOTHUBMESSAGE_REJECTED;
    }

    const unsigned char* buffmsg;
    size_t bufflen;

    // Read the message from stream
    if (IoTHubMessage_GetByteArray(message_handler, &buffmsg, &bufflen) != IOTHUB_MESSAGE_OK)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Failure retrieving message");
        return IOTHUBMESSAGE_REJECTED;
    }

    const size_t MAX_MESSAGE = 512;
    const char delimiter = '@';

    // Check whether the message is read well and is not bigger than 4 kbytes
    if (buffmsg[0] == '\0' || buffmsg[0] != delimiter || bufflen > MAX_MESSAGE)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Can't convert a message");
        return IOTHUBMESSAGE_REJECTED;
    }

    // Count the delimiters in message
    size_t delimiter_count = 1;

    for (size_t i = 1; i < bufflen; i++)
    {
        if (buffmsg[i] == delimiter)
        {
            delimiter_count++;
        }
    }

    // Check whether the delimiters count is correct
    if (delimiter_count != 3)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] Wrong message format");
        return IOTHUBMESSAGE_REJECTED;
    }

    // Topic name, payload size and payload tokenizing
    // By using the delimiter and idx
    // to keep the delimiter position to divide after
    size_t idx = 0;
    size_t payload_size = 0;
    std::string topic_name;
    std::string payload;

    // Divide the topic name and save after the second delimiter idx
    for (size_t i = 1; i < bufflen; i++)
    {
        if (buffmsg[i] != delimiter)
        {
            topic_name += buffmsg[i];
        }
        else
        {
            idx = i + 1;
            break;
        }
    }

    // Parse the payload size to value and save after the third delimiter idx
    for (size_t i = idx; i < bufflen; i++)
    {
        if (buffmsg[i] != delimiter)
        {
            payload_size = payload_size * 10 + (buffmsg[i] - '0');
        }
        else
        {
            idx = i + 1;
            break;
        }
    }

    // Copy the payload message part correctly after the third (last) delimiter
    payload.reserve(payload_size);
    std::copy(buffmsg + idx, buffmsg + idx + payload_size, std::back_inserter(payload));

    // If the topic is not found in a set of receiving topics, the message is broken.
    if (m_topicSet.find(topic_name) != m_topicSet.end())
    {
        // Send the topic and payload to ipc
        m_on_received_handler(topic_name, payload);

        // Returning IOTHUBMESSAGE_ACCEPTED causes the SDK to acknowledge receipt of
        // the message to the service.  The application does not need to take
        // further action tstatico ACK at this point.
        return IOTHUBMESSAGE_ACCEPTED;
    }

    for (auto it = m_topicSet.begin(); it != m_topicSet.end(); ++it)
    {
        if (areWildcardsMatched(*it, topic_name))
        {
            // Send the topic and payload to ipc
            m_on_received_handler(topic_name, payload);

            // Returning IOTHUBMESSAGE_ACCEPTED causes the SDK to acknowledge receipt of
            // the message to the service.  The application does not need to take
            // further action tstatico ACK at this point.
            return IOTHUBMESSAGE_ACCEPTED;
        }
    }

    SPDLOG_DEBUG("[iotb][azure_iot] No registered topic to send");
    return IOTHUBMESSAGE_REJECTED;
}

void ClientIot::ClientIotImpl::sendConfirmCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
    // When a message is sent this callback will get invoked
    SPDLOG_DEBUG("[iotb][azure_iot] Confirmation callback received for message {} with the result {}",
                 (unsigned long)M_MESSAGE_COUNT_SEND_CONFIRMATIONS,
                 MU_ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}

void ClientIot::ClientIotImpl::connectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result,
                                                        [[maybe_unused]] IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason)
{
    // This sample DOES NOT take into consideration network outages.
    if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] The device client is connected to iothub");
    }
    else
    {
        SPDLOG_DEBUG("[iotb][azure_iot] The device client has been disconnected");
    }
}

int32_t ClientIot::ClientIotImpl::deviceMethodCallback([[maybe_unused]] const char* method_name,
                                                       [[maybe_unused]] const unsigned char* payload,
                                                       [[maybe_unused]] size_t size,
                                                       [[maybe_unused]] unsigned char** response,
                                                       [[maybe_unused]] size_t* response_size)
{
    return 200;
}

void ClientIot::ClientIotImpl::sendIoTHubMessage(std::string message, IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handler)
{
    size_t messagesent = 0;
    bool continuerunning = true;
    do
    {
        // Check whether the message was sent
        if (messagesent < M_MESSAGE_COUNT_SEND_CONFIRMATIONS)
        {
            // Create a message (payload)
            std::unique_ptr<struct IOTHUB_MESSAGE_HANDLE_DATA_TAG, decltype(&IoTHubMessage_Destroy)> message_handler(
                IoTHubMessage_CreateFromByteArray(reinterpret_cast<const unsigned char*>(message.data()),
                                                  message.size()),
                IoTHubMessage_Destroy);

            if (!message_handler)
            {
                SPDLOG_DEBUG("[iotb][azure_iot] Failed to create a message to IoTHub");
                return;
            }

            // Send the message to IoTHub and set the confirmation callback
            if (IoTHubDeviceClient_LL_SendEventAsync(device_ll_handler, message_handler.get(), sendConfirmCallback,
                                                     this)
                != IOTHUB_CLIENT_OK)
            {
                SPDLOG_DEBUG("[iotb][azure_iot] Failed to send message to IoTHub");
                return;
            }

            // Incrementing the variable. The message has been sent successfully
            messagesent++;
        }
        else if (messagesent >= M_MESSAGE_COUNT_SEND_CONFIRMATIONS)
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

void ClientIot::ClientIotImpl::stop()
{
    // Set the unsubscribe from IoTHub common topics callback
    if (IoTHubDeviceClient_LL_SetDeviceMethodCallback(m_device_ll_handler.get(), nullptr, nullptr) != IOTHUB_CLIENT_OK)
    {
        SPDLOG_DEBUG("[iotb][azure_iot] There is nothing to unsubscribe");
        return;
    }

    // Free all the sdk subsystem
    IoTHub_Deinit();
}

bool ClientIot::ClientIotImpl::areWildcardsMatched(std::string_view first, std::string_view second)
{
    for (auto it1 = first.begin(), it2 = second.begin(); it1 != first.end() || it2 != second.end();)
    {
        if (*it1 == *it2)
        {
            it1++, it2++;
        }
        else if (*it1 == '#' || *it2 == '#')
        {
            return true;
        }
        else if (*it1 == '+')
        {
            it2 = std::find(it2, second.end(), '/');
            it1++;
        }
        else if (*it2 == '+')
        {
            it1 = std::find(it1, first.end(), '/');
            it2++;
        }
        else
        {
            return false;
        }
    }
    return true;
}

}  // namespace azure_iot