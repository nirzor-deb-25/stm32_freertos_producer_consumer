#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

#include "stm32_regs.h"
#include "rtos_hooks.h"
#include "producer_consumer.h"

#define BLUE_LED_MASK (1UL << 15)
#define ALL_LED_MASK  (0xFUL << 12)

#define LED_MODER     REG32(0x40020C00UL)
#define LED_OTYPER    REG32(0x40020C04UL)
#define LED_OSPEEDR   REG32(0x40020C08UL)
#define LED_PUPDR     REG32(0x40020C0CUL)
#define LED_BSRR      REG32(0x40020C18UL)

volatile uint32_t pc_stage = 0U;
volatile uint32_t heartbeat_count = 0U;

static void leds_init(void)
{
    RCC_AHB1ENR |= (1UL << 3);
    (void)RCC_AHB1ENR;

    LED_BSRR = ALL_LED_MASK << 16;

    LED_OTYPER &= ~ALL_LED_MASK;
    LED_OSPEEDR &= ~(0xFFUL << 24);
    LED_PUPDR &= ~(0xFFUL << 24);

    LED_MODER =
        (LED_MODER & ~(0xFFUL << 24)) |
        (0x55UL << 24);
}

static void heartbeat_task(void *argument)
{
    (void)argument;

    bool on = false;
    TickType_t previous_wake = xTaskGetTickCount();

    pc_stage = 3U;

    for (;;)
    {
        on = !on;

        LED_BSRR = on
            ? BLUE_LED_MASK
            : BLUE_LED_MASK << 16;

        heartbeat_count++;

        (void)xTaskDelayUntil(
            &previous_wake,
            pdMS_TO_TICKS(500U)
        );
    }
}

int main(void)
{
    leds_init();

    pc_stage = 1U;

    pc_start();

    BaseType_t result = xTaskCreate(
        heartbeat_task,
        "Heartbeat",
        256U,
        0,
        1U,
        0
    );

    configASSERT(result == pdPASS);

    pc_stage = 2U;

    vTaskStartScheduler();

    rtos_panic(
        RTOS_FAILURE_SCHEDULER,
        __FILE__,
        __LINE__
    );
}