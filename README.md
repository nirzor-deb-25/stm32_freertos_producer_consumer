# FreeRTOS Producer-Consumer Queue on STM32

A producer-consumer demonstration running on the STM32F407G-DISC1.

The project uses a bounded FreeRTOS queue to transfer numbered messages between tasks. It demonstrates normal operation, backpressure when the consumer is slower, timeout retries, and recovery when the consumer speeds up.

Application GPIO configuration uses direct register access without STM32 HAL.

## Hardware and tools

- STM32F407G-DISC1 with its onboard ST-LINK.
- Windows PowerShell.
- GNU Arm compiler and GNU Make.
- STM32CubeProgrammer CLI.
- Git.

No external sensor, UART bridge, or jumper wiring is required.

## FreeRTOS dependency

FreeRTOS-Kernel is included as a Git submodule.

- Release: V11.3.1
- Commit: 3a22924e0a9ddbbc8b0758881c33b3422a5cc20d
- Port: GCC ARM_CM4F
- Heap implementation: heap_4.c
- Configured RTOS heap: 32 KiB
- Scheduler tick: 1 ms
- CPU clock: reset-default 16 MHz HSI

FreeRTOS retains its upstream license.

## Architecture

| Task | Priority | Stack allocation | Responsibility |
|---|---:|---:|---|
| Producer | 2 | 256 words | Create and send messages; retry timed-out sends |
| Consumer | 2 | 256 words | Receive messages and check sequence and payload |
| Monitor | 1 | 256 words | Sample queue depth every 100 ms |
| Heartbeat | 1 | 256 words | Toggle blue LD6 every 500 ms |

Stack allocations are in 32-bit words. Actual stack usage has not been measured.

The queue holds four messages. Each message contains two uint32_t fields:

- sequence: starts at 1 and advances after a successful send.
- value: sequence XOR 0xA5A55A5A.

FreeRTOS copies the message contents into queue storage. The consumer receives its own copy.

## Default operation

| Control | Default |
|---|---:|
| Producer delay after a successful send | 100 ms |
| Consumer processing delay after receiving | 20 ms |
| Timeout for each send attempt | 50 ms |

The consumer normally processes messages faster than the producer creates them, so the queue is usually empty.

The producer delay occurs after a successful send. Queue waiting adds to the production interval; this is not a fixed-frequency producer.

## Backpressure and retries

When the consumer delay is increased to 500 ms, the queue fills.

The producer then waits inside xQueueSend for up to the configured timeout. If the attempt times out:

1. The timeout counter increases.
2. The same message is retried.
3. Its sequence number remains unchanged.

The message is not deliberately discarded on timeout.

After restoring the consumer delay to 20 ms, the consumer drains the backlog and normal operation resumes without a reset.

The consumer uses xQueueReceive with portMAX_DELAY to wait for messages.

## Controls and diagnostics

pc_control contains three consecutive 32-bit words:

| Offset | Field |
|---:|---|
| 0 | producer_period_ms |
| 4 | consumer_delay_ms |
| 8 | send_timeout_ms |

Timing inputs are bounded to 1 through 5000 ms and converted to at least one scheduler tick.

pc_diagnostics contains twelve consecutive 32-bit words:

| Offset | Field | Meaning |
|---:|---|---|
| 0 | created_count | New messages constructed |
| 4 | sent_count | Successful queue sends |
| 8 | received_count | Messages checked by the consumer |
| 12 | send_timeouts | Send attempts that timed out |
| 16 | sequence_errors | Unexpected received sequence numbers |
| 20 | payload_errors | Values that fail the XOR check |
| 24 | last_sequence | Last received sequence |
| 28 | last_value | Last received value |
| 32 | queue_depth | Most recently sampled queue depth |
| 36 | queue_peak | Maximum sampled queue depth |
| 40 | max_send_ticks | Longest measured send attempt |
| 44 | monitor_count | Monitor iterations |

Queue depth is sampled every 100 ms, so queue_peak can miss short-lived peaks.

Diagnostics are updated by multiple tasks. A live memory read is not an atomic snapshot of all fields, and transient differences between related counters are possible.

max_send_ticks measures elapsed scheduler ticks around xQueueSend. It is not a precise measurement of time spent blocked.

## Build

Clone this repository with its FreeRTOS submodule:

    git clone --recurse-submodules <repository-url>

For an existing clone:

    git submodule update --init --recursive

From the project root in PowerShell:

    . .\setup.ps1
    make -j4 --output-sync=target

Flash the Discovery:

    make flash

The Makefile selects ST-LINK serial number 0671FF495170535067222342. Change STLINK_SN for your own board.

Outputs:

    build/stm32_freertos_producer_consumer.elf
    build/stm32_freertos_producer_consumer.bin
    build/stm32_freertos_producer_consumer.hex

## Find runtime addresses

Always obtain addresses from the ELF corresponding to the installed firmware:

    arm-none-eabi-nm -n .\build\stm32_freertos_producer_consumer.elf |
        Select-String 'pc_control$|pc_diagnostics$|rtos_failure_code$'

The tested build placed:

| Symbol | Address |
|---|---|
| pc_control | 0x20000000 |
| pc_diagnostics | 0x2000001C |
| rtos_failure_code | 0x20000058 |

These addresses can change after rebuilding modified source.

## Reproduce the queue experiment

The following commands use addresses from the tested build.

Read normal-operation diagnostics:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -r32 0x2000001C 48

Set the consumer delay to 500 ms:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -w32 0x20000004 0x000001F4

Wait at least five seconds, then read diagnostics again.

Restore the consumer delay to 20 ms:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -w32 0x20000004 0x00000014

Wait at least three seconds, then compare two diagnostic reads.

Read the failure code:

    STM32_Programmer_CLI.exe -c port=SWD mode=HOTPLUG sn=0671FF495170535067222342 -r32 0x20000058 4

Explicitly reset through ST-LINK:

    STM32_Programmer_CLI.exe -c port=SWD mode=UR sn=0671FF495170535067222342 -rst

RAM timing changes and diagnostic history are cleared by startup after reset.

## Recorded hardware results

All values below are decimal.

### Normal operation

| Diagnostic | First read | Second read |
|---|---:|---:|
| Created | 281 | 312 |
| Sent | 281 | 312 |
| Received | 281 | 312 |
| Send timeouts | 0 | 0 |
| Sequence errors | 0 | 0 |
| Payload errors | 0 | 0 |
| Sampled queue depth | 0 | 0 |

The RTOS failure code was zero.

### Slow consumer

The consumer delay was changed to 500 ms.

| Diagnostic | First read | Second read |
|---|---:|---:|
| Created | 1229 | 1242 |
| Sent | 1228 | 1241 |
| Received | 1224 | 1237 |
| Send timeouts | 357 | 448 |
| Sequence errors | 0 | 0 |
| Payload errors | 0 | 0 |
| Sampled queue depth | 4 | 4 |
| Sampled peak depth | 4 | 4 |
| Maximum send duration, ticks | 50 | 50 |

The observed differences are consistent with four queued messages and one producer message awaiting a successful send.

### Recovery without reset

The consumer delay was restored to 20 ms.

| Diagnostic | First read | Second read |
|---|---:|---:|
| Created, sent, and received | 1486 | 1549 |
| Send timeouts | 1673 | 1673 |
| Sequence errors | 0 | 0 |
| Payload errors | 0 | 0 |
| Sampled queue depth | 0 | 0 |
| Historical peak depth | 4 | 4 |

The queue drained and timeout accumulation stopped.

Reads were performed interactively. Their spacing includes command execution and user interaction, so these readings are not a throughput benchmark.

### Restart through ST-LINK

After an explicit reset command:

- Created, sent, and received counts were 113 at the recorded read.
- Send timeouts were zero.
- Sequence and payload errors were zero.
- Sampled queue depth and historical peak were zero.
- Maximum recorded send duration was zero.

Earlier reads following attempted physical-button resets retained the previous counters. Physical RESET-button operation was therefore not confirmed by those observations.

## Indicators and failure handling

Blue LD6 is configured to toggle every 500 ms, producing one complete cycle per second.

The diagnostic panic handler disables interrupts and lights red LD5 when an assertion, allocation failure, or detected task-stack overflow occurs.

A heartbeat alone does not prove correct queue operation; inspect the message and error counters.

## Scope and limitations

- One producer and one consumer.
- Synthetic payload, without a sensor or UART interface.
- Queue operations occur in task context.
- Timeout retries preserve the pending message within the running session.
- No persistent delivery across reset.
- No exhaustive overload, timing, or long-duration wraparound validation.
- No measured stack high-water marks or worst-case execution times.
- Failure hooks are implemented, but fault injection was not performed.
- Hardware observations apply to the tested conditions.

## References

- FreeRTOS-Kernel: https://github.com/FreeRTOS/FreeRTOS-Kernel
- STM32F407G-DISC1: https://www.st.com/en/evaluation-tools/stm32f407g-disc1.html
- STM32CubeProgrammer: https://www.st.com/en/development-tools/stm32cubeprog.html
