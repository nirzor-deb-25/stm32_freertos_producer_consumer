#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "producer_consumer.h"

#define PC_QUEUE_LENGTH 4U

typedef struct
{
    uint32_t sequence;
    uint32_t value;
} pc_message_t;

volatile pc_control_t pc_control = {
    .producer_period_ms = 100U,
    .consumer_delay_ms = 20U,
    .send_timeout_ms = 50U
};

volatile pc_diagnostics_t pc_diagnostics = {0};

static QueueHandle_t message_queue;

static uint32_t message_value(uint32_t sequence)
{
    return sequence ^ 0xA5A55A5AUL;
}

/*
 * Bound debugger-controlled timings and ensure that delays
 * remain at least one tick.
 */
static TickType_t bounded_ticks(uint32_t milliseconds)
{
    if (milliseconds < 1U)
    {
        milliseconds = 1U;
    }
    else if (milliseconds > 5000U)
    {
        milliseconds = 5000U;
    }

    TickType_t ticks = pdMS_TO_TICKS(milliseconds);

    return (ticks == 0U) ? (TickType_t)1U : ticks;
}

static void producer_task(void *argument)
{
    (void)argument;

    uint32_t sequence = 1U;

    for (;;)
    {
        const pc_message_t message = {
            .sequence = sequence,
            .value = message_value(sequence)
        };

        pc_diagnostics.created_count++;

        for (;;)
        {
            TickType_t timeout = bounded_ticks(
                pc_control.send_timeout_ms
            );

            TickType_t start = xTaskGetTickCount();

            BaseType_t result = xQueueSend(
                message_queue,
                &message,
                timeout
            );

            TickType_t elapsed = xTaskGetTickCount() - start;

            if ((uint32_t)elapsed >
                pc_diagnostics.max_send_ticks)
            {
                pc_diagnostics.max_send_ticks =
                    (uint32_t)elapsed;
            }

            if (result == pdPASS)
            {
                pc_diagnostics.sent_count++;
                break;
            }

            /*
             * Keep this message and retry it.
             * Do not advance its sequence number.
             */
            pc_diagnostics.send_timeouts++;
        }

        sequence++;

        /*
         * Delay after a successful send.
         * Queue blocking can therefore reduce production rate.
         */
        vTaskDelay(
            bounded_ticks(pc_control.producer_period_ms)
        );
    }
}

static void consumer_task(void *argument)
{
    (void)argument;

    uint32_t expected_sequence = 1U;

    for (;;)
    {
        pc_message_t message;

        if (xQueueReceive(
                message_queue,
                &message,
                portMAX_DELAY) != pdPASS)
        {
            continue;
        }

        if (message.sequence != expected_sequence)
        {
            pc_diagnostics.sequence_errors++;
        }

        if (message.value != message_value(message.sequence))
        {
            pc_diagnostics.payload_errors++;
        }

        expected_sequence = message.sequence + 1U;

        pc_diagnostics.last_sequence = message.sequence;
        pc_diagnostics.last_value = message.value;
        pc_diagnostics.received_count++;

        /* Simulate the time required to process a message. */
        vTaskDelay(
            bounded_ticks(pc_control.consumer_delay_ms)
        );
    }
}

static void monitor_task(void *argument)
{
    (void)argument;

    for (;;)
    {
        uint32_t depth = (uint32_t)uxQueueMessagesWaiting(
            message_queue
        );

        pc_diagnostics.queue_depth = depth;

        if (depth > pc_diagnostics.queue_peak)
        {
            pc_diagnostics.queue_peak = depth;
        }

        pc_diagnostics.monitor_count++;

        vTaskDelay(pdMS_TO_TICKS(100U));
    }
}

void pc_start(void)
{
    configASSERT(message_queue == 0);
    configASSERT(pdMS_TO_TICKS(100U) > 0U);

    message_queue = xQueueCreate(
        PC_QUEUE_LENGTH,
        sizeof(pc_message_t)
    );

    configASSERT(message_queue != 0);

    vQueueAddToRegistry(message_queue, "Messages");

    BaseType_t result = xTaskCreate(
        producer_task,
        "Producer",
        256U,
        0,
        2U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        consumer_task,
        "Consumer",
        256U,
        0,
        2U,
        0
    );

    configASSERT(result == pdPASS);

    result = xTaskCreate(
        monitor_task,
        "Monitor",
        256U,
        0,
        1U,
        0
    );

    configASSERT(result == pdPASS);
}