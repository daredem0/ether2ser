#ifndef V24_TX_STATE_H
#define V24_TX_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    V24_TX_PHASE_IDLE = 0,
    V24_TX_PHASE_RTS_SETTLE,
    V24_TX_PHASE_SENDING,
    V24_TX_PHASE_DRAINING,
    V24_TX_PHASE_RTS_HOLD,
    V24_TX_PHASE_INTERFRAME_HOLD,
} v24_tx_phase_t;

typedef enum
{
    V24_TX_RESTART_NONE = 0,
    V24_TX_RESTART_CTS,
    V24_TX_RESTART_UNDERRUN,
} v24_tx_restart_reason_t;

typedef uint32_t v24_tx_action_t;
enum
{
    V24_TX_ACTION_NONE           = 0U,
    V24_TX_ACTION_ASSERT_RTS     = 1U << 0U,
    V24_TX_ACTION_ALLOW_WRITE    = 1U << 1U,
    V24_TX_ACTION_CLEAR_STALL    = 1U << 2U,
    V24_TX_ACTION_RESET_PIO      = 1U << 3U,
    V24_TX_ACTION_DEASSERT_RTS   = 1U << 4U,
    V24_TX_ACTION_RESTART_FRAME  = 1U << 5U,
    V24_TX_ACTION_FRAME_COMPLETE = 1U << 6U,
};

typedef struct
{
    v24_tx_phase_t          phase;
    v24_tx_restart_reason_t restart_reason;
    uint32_t                holdoff_us;
    uint64_t                deadline_us;
    uint32_t                frame_cts_baseline;
    bool                    frame_cts_baseline_valid;
    bool                    cts_restart_enabled;
} v24_tx_state_t;

void v24_tx_state_init(v24_tx_state_t* state, uint32_t holdoff_us,
                       bool cts_restart_enabled);
void v24_tx_state_reconfigure(v24_tx_state_t* state, uint32_t holdoff_us);
v24_tx_action_t v24_tx_state_request_write(v24_tx_state_t* state, uint64_t now_us,
                                            uint32_t cts_seq, bool pio_stalled);
v24_tx_action_t v24_tx_state_frame_queued(v24_tx_state_t* state);
v24_tx_action_t v24_tx_state_poll(v24_tx_state_t* state, uint64_t now_us, uint32_t cts_seq,
                                  bool pio_stalled);
void v24_tx_state_set_cts_restart(v24_tx_state_t* state, bool enabled);

#endif /* V24_TX_STATE_H */
