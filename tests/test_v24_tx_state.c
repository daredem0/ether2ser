#include <stdbool.h>
#include <stdint.h>

#include "drivers/v24_tx_state.h"
#include "unity.h"

#define TEST_HOLDOFF_US 200U

static void begin_sending(v24_tx_state_t* state, uint32_t cts_seq)
{
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_ASSERT_RTS,
                             v24_tx_state_request_write(state, 1000U, cts_seq, false));
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_CLEAR_STALL | V24_TX_ACTION_ALLOW_WRITE,
                             v24_tx_state_request_write(state, 1200U, cts_seq, true));
}

void test_v24_tx_state_asserts_rts_and_waits_for_settle(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_ASSERT_RTS,
                             v24_tx_state_request_write(&state, 1000U, 7U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_RTS_SETTLE, state.phase);
    TEST_ASSERT_EQUAL_UINT64(1200U, state.deadline_us);
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_request_write(&state, 1199U, 8U, true));
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_CLEAR_STALL | V24_TX_ACTION_ALLOW_WRITE,
                             v24_tx_state_request_write(&state, 1200U, 8U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_SENDING, state.phase);
    TEST_ASSERT_EQUAL_UINT32(8U, state.frame_cts_baseline);
    TEST_ASSERT_TRUE(state.frame_cts_baseline_valid);
}

void test_v24_tx_state_ignores_cts_edges_before_first_byte(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);

    (void)v24_tx_state_request_write(&state, 1000U, 3U, false);
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_poll(&state, 1100U, 9U, true));
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_CLEAR_STALL | V24_TX_ACTION_ALLOW_WRITE,
                             v24_tx_state_request_write(&state, 1200U, 9U, true));
    TEST_ASSERT_EQUAL_UINT32(9U, state.frame_cts_baseline);
}

void test_v24_tx_state_restarts_on_cts_edge_while_sending(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 4U);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_RESET_PIO | V24_TX_ACTION_CLEAR_STALL |
                                 V24_TX_ACTION_RESTART_FRAME,
                             v24_tx_state_poll(&state, 1250U, 5U, false));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_RTS_SETTLE, state.phase);
    TEST_ASSERT_EQUAL_UINT64(1450U, state.deadline_us);
    TEST_ASSERT_FALSE(state.frame_cts_baseline_valid);
    TEST_ASSERT_EQUAL(V24_TX_RESTART_CTS, state.restart_reason);
}

void test_v24_tx_state_restarts_on_cts_edge_while_draining(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 4U);
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_CLEAR_STALL,
                             v24_tx_state_frame_queued(&state));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_DRAINING, state.phase);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_RESET_PIO | V24_TX_ACTION_CLEAR_STALL |
                                 V24_TX_ACTION_RESTART_FRAME,
                             v24_tx_state_poll(&state, 1300U, 6U, false));
    TEST_ASSERT_EQUAL(V24_TX_RESTART_CTS, state.restart_reason);
}

void test_v24_tx_state_restarts_on_fifo_underrun_during_poll(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 4U);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_RESET_PIO | V24_TX_ACTION_CLEAR_STALL |
                                 V24_TX_ACTION_RESTART_FRAME,
                             v24_tx_state_poll(&state, 1250U, 4U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_RTS_SETTLE, state.phase);
    TEST_ASSERT_EQUAL_UINT64(1450U, state.deadline_us);
    TEST_ASSERT_EQUAL(V24_TX_RESTART_UNDERRUN, state.restart_reason);
}

void test_v24_tx_state_restarts_on_fifo_underrun_before_write(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 4U);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_RESET_PIO | V24_TX_ACTION_CLEAR_STALL |
                                 V24_TX_ACTION_RESTART_FRAME,
                             v24_tx_state_request_write(&state, 1250U, 4U, true));
    TEST_ASSERT_EQUAL(V24_TX_RESTART_UNDERRUN, state.restart_reason);
}

void test_v24_tx_state_ignores_cts_restart_when_disabled(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, false);
    begin_sending(&state, 4U);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_poll(&state, 1250U, 5U, false));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_SENDING, state.phase);
}

void test_v24_tx_state_completes_after_rts_and_interframe_holds(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 4U);
    (void)v24_tx_state_frame_queued(&state);

    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_poll(&state, 1300U, 4U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_RTS_HOLD, state.phase);
    TEST_ASSERT_EQUAL_UINT64(1500U, state.deadline_us);
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_poll(&state, 1499U, 4U, true));
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_DEASSERT_RTS,
                             v24_tx_state_poll(&state, 1500U, 4U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_INTERFRAME_HOLD, state.phase);
    TEST_ASSERT_EQUAL_UINT64(1700U, state.deadline_us);
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_NONE,
                             v24_tx_state_poll(&state, 1699U, 4U, true));
    TEST_ASSERT_EQUAL_UINT32(V24_TX_ACTION_FRAME_COMPLETE,
                             v24_tx_state_poll(&state, 1700U, 4U, true));
    TEST_ASSERT_EQUAL(V24_TX_PHASE_IDLE, state.phase);
    TEST_ASSERT_EQUAL(V24_TX_RESTART_NONE, state.restart_reason);
}

void test_v24_tx_state_reconfigure_resets_lifecycle_and_preserves_restart(void)
{
    v24_tx_state_t state;
    v24_tx_state_init(&state, TEST_HOLDOFF_US, true);
    begin_sending(&state, 2U);

    v24_tx_state_reconfigure(&state, 500U);

    TEST_ASSERT_EQUAL(V24_TX_PHASE_IDLE, state.phase);
    TEST_ASSERT_EQUAL_UINT32(500U, state.holdoff_us);
    TEST_ASSERT_EQUAL_UINT64(0U, state.deadline_us);
    TEST_ASSERT_FALSE(state.frame_cts_baseline_valid);
    TEST_ASSERT_TRUE(state.cts_restart_enabled);
}
