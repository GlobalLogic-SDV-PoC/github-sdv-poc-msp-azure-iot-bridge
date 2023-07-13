#ifndef _INCLUDE_AZURE_IOT_CLIENT_IOT_IMPL_HPP_
#define _INCLUDE_AZURE_IOT_CLIENT_IOT_IMPL_HPP_

#include <set>

#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_iot/client_iot.hpp"
#include "iothub.h"
#include "iothub_client_options.h"
#include "iothub_device_client_ll.h"
#include "iothub_message.h"
#include "iothubtransportmqtt.h"
#include "spdlog/spdlog.h"

namespace azure_iot
{
class ClientIot::ClientIotImpl
{
public:
    ClientIotImpl(const std::string& conn_string, const on_received_handler& handler);

    ~ClientIotImpl();

    // A method to create the IoTHub instance
    // to create the communication with connection string
    // to set all the necessary options and callbacks
    // (connection, subscribe/unsubscribe, publish)
    // after the successfull settings, it sends the stub message
    // to confirm communication process with IoTHub immediately
    void start();

    // A method to set the receiving topics
    void subscribe(std::string_view topic);

    // A method to delete the receiving topics from collection
    void unsubscribe(std::string_view topic);

    // A method to create the Device to Client message with appropriate style
    // and sending mechanism
    void publish(std::string_view topic, std::string_view payload);

private:
    // A field for sending message to IoTHub confirmation
    static constexpr size_t M_MESSAGE_COUNT_SEND_CONFIRMATIONS = 1;

    std::string m_conn_string;
    on_received_handler m_on_received_handler;

    std::set<std::string> m_topicSet;

    std::unique_ptr<struct IOTHUB_CLIENT_CORE_LL_HANDLE_DATA_TAG, decltype(&IoTHubDeviceClient_LL_Destroy)>
        m_device_ll_handler;

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
                                  [[maybe_unused]] IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason);

    // A subscribe/unsubscribe to/from IoTHub specific topics callback
    int32_t deviceMethodCallback([[maybe_unused]] const char* method_name,
                                 [[maybe_unused]] const unsigned char* payload,
                                 [[maybe_unused]] size_t size,
                                 [[maybe_unused]] unsigned char** response,
                                 [[maybe_unused]] size_t* response_size);

    // A specific method to send the message to IoTHub with Azure-sdk
    void sendIoTHubMessage(std::string message, IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handler);

    // A method to stop the communication process with an IoTHub
    // and to destroy the IoTHub instance
    void stop();
};

}  // namespace azure_iot

#endif  //_INCLUDE_AZURE_IOT_CLIENT_IOT_IMPL_HPP_