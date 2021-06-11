#include "libs/base/ipc.h"
#include "libs/base/tasks.h"
#include "third_party/nxp/rt1176-sdk/middleware/multicore/mcmgr/src/mcmgr.h"

namespace valiant {

void IPC::StaticFreeRtosMessageEventHandler(uint16_t eventData, void *context) {
    GetSingleton()->FreeRtosMessageEventHandler(eventData, context);
}

void IPC::FreeRtosMessageEventHandler(uint16_t eventData, void *context) {
    BaseType_t higher_priority_woken = pdFALSE;
    // TODO(atv): Get the base address of the shmem from a linker sym
    xStreamBufferSendCompletedFromISR(
        reinterpret_cast<StreamBufferHandle_t>(0x202C0000U | eventData),
        &higher_priority_woken);
    portYIELD_FROM_ISR(higher_priority_woken);
}

void IPC::RegisterAppMessageHandler(AppMessageHandler handler, void *param) {
    app_handler_ = handler;
    app_handler_param_ = param;
}

void IPC::HandleAppMessage(const uint8_t data[ipc::kMessageBufferDataSize]) {
    if (app_handler_) {
        app_handler_(data, app_handler_param_);
    }
}

void IPC::SendMessage(const ipc::Message& message) {
    if (!tx_task_ || !tx_semaphore_) {
        return;
    }
    xTaskNotifyIndexed(tx_task_, kSendMessageNotification, (uint32_t)&message, eSetValueWithOverwrite);
    xSemaphoreTake(tx_semaphore_, portMAX_DELAY);
}

void IPC::StaticTxTaskFn(void *param) {
    GetSingleton()->TxTaskFn(param);
}

void IPC::StaticRxTaskFn(void *param) {
    GetSingleton()->RxTaskFn(param);
}

void IPC::TxTaskFn(void *param) {
    while (true) {
        ipc::Message* message;
        xTaskNotifyWaitIndexed(kSendMessageNotification, 0, 0, reinterpret_cast<uint32_t*>(&message), portMAX_DELAY);
        xMessageBufferSend(tx_queue_->message_buffer, message, sizeof(*message), portMAX_DELAY);
        xSemaphoreGive(tx_semaphore_);
    }
}

void IPC::RxTaskFn(void *param) {
    size_t rx_bytes;
    while (true) {
        ipc::Message rx_message;
        rx_bytes = xMessageBufferReceive(rx_queue_->message_buffer, &rx_message, sizeof(rx_message), portMAX_DELAY);
        if (rx_bytes == 0) {
            continue;
        }
        switch (rx_message.type) {
            case ipc::MessageType::SYSTEM:
                HandleSystemMessage(rx_message.message.system);
                break;
            case ipc::MessageType::APP:
                HandleAppMessage(rx_message.message.data);
                break;
            default:
                printf("Unhandled IPC message type %d\r\n", static_cast<int>(rx_message.type));
                break;
        }
    }
}

void IPC::Init() {
    tx_semaphore_ = xSemaphoreCreateBinary();
    MCMGR_RegisterEvent(kMCMGR_FreeRtosMessageBuffersEvent, StaticFreeRtosMessageEventHandler, 0);
    xTaskCreate(IPC::StaticTxTaskFn, "ipc_tx_task", configMINIMAL_STACK_SIZE * 10, NULL, IPC_TASK_PRIORITY, &tx_task_);
    xTaskCreate(IPC::StaticRxTaskFn, "ipc_rx_task", configMINIMAL_STACK_SIZE * 10, NULL, IPC_TASK_PRIORITY, &rx_task_);
    vTaskSuspend(tx_task_);
    vTaskSuspend(rx_task_);
}

}  // namespace valiant