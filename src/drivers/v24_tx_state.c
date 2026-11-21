#include "drivers/v24_tx_state.h"

#include <stddef.h>
#include <string.h>

static v24_tx_action_t v24_tx_state_check_cts(v24_tx_state_t* state, uint64_t now_us,
                                               uint32_t cts_seq)
{
    if (state->phase != V24_TX_PHASE_SENDING && state->phase != V24_TX_PHASE_DRAINING)
    {
        return V24_TX_ACTION_NONE;
    }

    if (!state->frame_cts_baseline_valid)
    {
        state->frame_cts_baseline       = cts_seq;
        state->frame_cts_baseline_valid = true;
        return V24_TX_ACTION_NONE;
    }

    if (!state->cts_restart_enabled)
    {
        state->frame_cts_baseline = cts_seq;
        return V24_TX_ACTION_NONE;
    }

    if (cts_seq == state->frame_cts_baseline)
    {
        return V24_TX_ACTION_NONE;
    }

    state->phase                    = V24_TX_PHASE_RTS_SETTLE;
    state->deadline_us              = now_us + state->holdoff_us;
    state->frame_cts_baseline_valid = false;
    return V24_TX_ACTION_RESET_PIO | V24_TX_ACTION_CLEAR_STALL |
           V24_TX_ACTION_RESTART_FRAME;
}

void v24_tx_state_init(v24_tx_state_t* state, uint32_t holdoff_us,
                       bool cts_restart_enabled)
{
    if (state == NULL)
    {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->phase               = V24_TX_PHASE_IDLE;
    state->holdoff_us          = holdoff_us;
    state->cts_restart_enabled = cts_restart_enabled;
}

void v24_tx_state_reconfigure(v24_tx_state_t* state, uint32_t holdoff_us)
{
    if (state == NULL)
    {
        return;
    }

    bool cts_restart_enabled = state->cts_restart_enabled;
    v24_tx_state_init(state, holdoff_us, cts_restart_enabled);
}

v24_tx_action_t v24_tx_state_request_write(v24_tx_state_t* state, uint64_t now_us,
                                            uint32_t cts_seq)
{
    if (state == NULL)
    {
        return V24_TX_ACTION_NONE;
    }

    if (state->phase == V24_TX_PHASE_IDLE)
    {
        state->phase       = V24_TX_PHASE_RTS_SETTLE;
        state->deadline_us = now_us + state->holdoff_us;
        return V24_TX_ACTION_ASSERT_RTS;
    }

    if (state->phase == V24_TX_PHASE_RTS_SETTLE)
    {
        if (now_us < state->deadline_us)
        {
            return V24_TX_ACTION_NONE;
        }

        state->phase                    = V24_TX_PHASE_SENDING;
        state->frame_cts_baseline       = cts_seq;
        state->frame_cts_baseline_valid = true;
        return V24_TX_ACTION_ALLOW_WRITE;
    }

    if (state->phase == V24_TX_PHASE_SENDING)
    {
        v24_tx_action_t action = v24_tx_state_check_cts(state, now_us, cts_seq);
        return action == V24_TX_ACTION_NONE ? V24_TX_ACTION_ALLOW_WRITE : action;
    }

    return V24_TX_ACTION_NONE;
}

v24_tx_action_t v24_tx_state_frame_queued(v24_tx_state_t* state)
{
    if (state == NULL || state->phase != V24_TX_PHASE_SENDING)
    {
        return V24_TX_ACTION_NONE;
    }

    state->phase = V24_TX_PHASE_DRAINING;
    return V24_TX_ACTION_CLEAR_STALL;
}

v24_tx_action_t v24_tx_state_poll(v24_tx_state_t* state, uint64_t now_us, uint32_t cts_seq,
                                  bool pio_frame_complete)
{
    if (state == NULL)
    {
        return V24_TX_ACTION_NONE;
    }

    v24_tx_action_t cts_action = v24_tx_state_check_cts(state, now_us, cts_seq);
    if (cts_action != V24_TX_ACTION_NONE)
    {
        return cts_action;
    }

    if (state->phase == V24_TX_PHASE_DRAINING && pio_frame_complete)
    {
        state->phase       = V24_TX_PHASE_RTS_HOLD;
        state->deadline_us = now_us + state->holdoff_us;
    }
    else if (state->phase == V24_TX_PHASE_RTS_HOLD && now_us >= state->deadline_us)
    {
        state->phase       = V24_TX_PHASE_INTERFRAME_HOLD;
        state->deadline_us = now_us + state->holdoff_us;
        return V24_TX_ACTION_DEASSERT_RTS;
    }
    else if (state->phase == V24_TX_PHASE_INTERFRAME_HOLD && now_us >= state->deadline_us)
    {
        state->phase                    = V24_TX_PHASE_IDLE;
        state->frame_cts_baseline_valid = false;
        return V24_TX_ACTION_FRAME_COMPLETE;
    }

    return V24_TX_ACTION_NONE;
}

void v24_tx_state_set_cts_restart(v24_tx_state_t* state, bool enabled)
{
    if (state == NULL)
    {
        return;
    }

    state->cts_restart_enabled = enabled;
    if (enabled)
    {
        state->frame_cts_baseline_valid = false;
    }
}
