#ifndef _LIBS_BASE_QUEUE_TASK_H_
#define _LIBS_BASE_QUEUE_TASK_H_

#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/queue.h"
#include "third_party/freertos_kernel/include/semphr.h"
#include "third_party/freertos_kernel/include/task.h"

namespace valiant {

constexpr size_t kDefaultTaskStackDepth = configMINIMAL_STACK_SIZE;
template <typename Request, typename Response, const char *Name, size_t StackDepth, UBaseType_t Priority, UBaseType_t QueueLength>
class QueueTask {
  public:
    // Initialization function for the QueueTask.
    // If overridden, implementors should ensure that the super method is still called.
    // Initialized any needed data here, but beware that the FreeRTOS scheduler may not yet be running.
    virtual void Init() {
        request_queue_ = xQueueCreate(QueueLength, sizeof(Request));
        xTaskCreateStatic(
                StaticTaskMain, Name, stack_depth_, this,
                Priority, task_stack_, &task_);
    }
  protected:
    Response SendRequest(Request& req) {
        Response resp;
        SemaphoreHandle_t req_semaphore = xSemaphoreCreateBinary();
        req.callback =
            [req_semaphore, &resp](Response cb_resp) {
                resp = cb_resp;
                xSemaphoreGive(req_semaphore);
            };
        xQueueSend(request_queue_, &req, pdMS_TO_TICKS(200));
        xSemaphoreTake(req_semaphore, pdMS_TO_TICKS(200));
        vSemaphoreDelete(req_semaphore);
        return resp;
    }

    void SendRequestAsync(Request& req) {
        xQueueSend(request_queue_, &req, pdMS_TO_TICKS(200));
    }

    const size_t stack_depth_ = StackDepth;
    StaticTask_t task_;
    StackType_t task_stack_[StackDepth];
    QueueHandle_t request_queue_;
  private:
    static void StaticTaskMain(void *param) {
        ((QueueTask*)param)->TaskMain();
    }

    // Main QueueTask FreeRTOS task function.
    // Calls implementation-specific initialization, and then starts receiving from the queue.
    // This will block and yield the CPU if no messages are available.
    // When a message is received, the implementation-specific handler is invoked.
    void TaskMain() {
        TaskInit();

        Request msg;
        while (true) {
            if (xQueueReceive(request_queue_, &msg, portMAX_DELAY) == pdTRUE) {
                RequestHandler(&msg);
            }
        }
    }

    // Implementation-specific hook for initialization at the start of the task function.
    // FreeRTOS scheduler is running at this point, so it's okay to use functions that may require it.
    virtual void TaskInit() {};

    // Implementation-specific handler for messages coming from the queue.
    virtual void RequestHandler(Request* msg) = 0;
};

}  // namespace valiant

#endif  // _LIBS_BASE_QUEUE_TASK_H_