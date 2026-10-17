#ifndef PRODUCER_CONSUMER_H
#define PRODUCER_CONSUMER_H

#include <stdint.h>

/* Timing controls, expressed in milliseconds. */
typedef struct
{
    uint32_t producer_period_ms;
    uint32_t consumer_delay_ms;
    uint32_t send_timeout_ms;
} pc_control_t;

/* Each diagnostic field occupies one 32-bit word. */
typedef struct
{
    uint32_t created_count;
    uint32_t sent_count;
    uint32_t received_count;
    uint32_t send_timeouts;
    uint32_t sequence_errors;
    uint32_t payload_errors;
    uint32_t last_sequence;
    uint32_t last_value;
    uint32_t queue_depth;
    uint32_t queue_peak;
    uint32_t max_send_ticks;
    uint32_t monitor_count;
} pc_diagnostics_t;

extern volatile pc_control_t pc_control;
extern volatile pc_diagnostics_t pc_diagnostics;

/* Create the queue and tasks before starting the scheduler. */
void pc_start(void);

#endif