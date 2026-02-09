#include "unity.h"
#include <stddef.h>
#include <stdint.h>
#include "protocol/hdlc_sync.h"
#include "system/error.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>


void shift_array_right(const uint8_t *in, uint8_t *out, size_t len, size_t bits_to_shift)
{
    if (!in || !out || len == 0) {
        return;
    }
    uint8_t carryover = 0;
    memset(out, 0, len+1);
    for(size_t i = 0; i < len; i++){
        out[i] |= (in[i] >> bits_to_shift) | (carryover<<(8-bits_to_shift));
        // carryover = in[i] & (0xFF >> bits_to_shift);
        carryover = in[i] & (0xFF >> (8 - bits_to_shift));
    }
    out[len] = carryover << (8 - bits_to_shift);
}



void shift_array_left(const uint8_t *in, uint8_t *shifted, size_t num_bits, size_t len)
{
    if (!in || !shifted || len == 0) return;

    size_t byte_shift = num_bits / 8;
    size_t bit_shift  = num_bits % 8;

    memset(shifted, 0, len);

    if (byte_shift >= len) {
        return; // everything shifted out
    }

    for (size_t i = 0; i < len - byte_shift; i++) {
        uint8_t src = in[i];
        size_t out_idx = i + byte_shift;

        shifted[out_idx] |= (uint8_t)(src << bit_shift);

        if (bit_shift != 0 && out_idx + 1 < len) {
            shifted[out_idx + 1] |= (uint8_t)(src >> (8 - bit_shift));
        }
    }
}
void test_multiple_bits_shifted_hdlc_frame(void){
  // Frame: 7E 11 7D 5E 22 7D 5D 33 CF BB 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x11, HDLC_ESCAPE_BYTE,
        0x5E, 0x22, HDLC_ESCAPE_BYTE, 0x5D, 0x33, 0xCF, 0xBB, HDLC_FLAG_BYTE};
    for (size_t i = 0; i < 8; i++) {
        uint8_t shifted_buffer[sizeof(frame_buffer)+1];
        shift_array_right(frame_buffer, shifted_buffer, sizeof(frame_buffer), i);
        // printf("Shifted frame: ");
        // for(size_t b = 0; b < sizeof(shifted_buffer); b++){
        //     printf("%02x ", shifted_buffer[b]);
        // }
        // printf("\n");
        HDLC_SYNC_ACCUMULATOR_T accumulator;
        hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
        for(size_t i = 0; i < sizeof(shifted_buffer); i++){
            hdlc_sync_acc_process_byte(&accumulator, shifted_buffer[i]);
        }

        uint8_t reconstructed_frame_buffer[128];
        HDLC_FRAME_T reconstructed_frame = {
            .payload = reconstructed_frame_buffer,
            .length = 0,
            .capacity = sizeof(frame_buffer)
        };
        TEST_ASSERT_EQUAL(
            hdlc_sync_acc_poll(&accumulator, &reconstructed_frame),
            E2S_ERR_HDLC_ACC_FRAME_READY
        );
        TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
        TEST_ASSERT_EQUAL(i, accumulator.bit_offset);

        TEST_ASSERT_EQUAL( sizeof(frame_buffer), reconstructed_frame.length);
        for(size_t i = 0; i < sizeof(frame_buffer); i++){
            TEST_ASSERT_EQUAL_HEX8(frame_buffer[i], reconstructed_frame.payload[i]);
        }
    }

}

void test_one_bit_shifted_hdlc_frame(void){
    // Frame: 7E FF FF 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0xFF, 0xFF, HDLC_FLAG_BYTE};
    uint8_t shifted_buffer[sizeof(frame_buffer)+1];
    shift_array_right(frame_buffer, shifted_buffer, sizeof(frame_buffer), 1);

    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
    for(size_t i = 0; i < sizeof(shifted_buffer); i++){
        hdlc_sync_acc_process_byte(&accumulator, shifted_buffer[i]);
    }

    uint8_t reconstructed_frame_buffer[128];
    HDLC_FRAME_T reconstructed_frame = {
        .payload = reconstructed_frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    TEST_ASSERT_EQUAL(
        hdlc_sync_acc_poll(&accumulator, &reconstructed_frame),
        E2S_ERR_HDLC_ACC_FRAME_READY
    );
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
    TEST_ASSERT_EQUAL(1, accumulator.bit_offset);

    TEST_ASSERT_EQUAL( sizeof(frame_buffer), reconstructed_frame.length);
    for(size_t i = 0; i < sizeof(frame_buffer); i++){
        TEST_ASSERT_EQUAL_HEX8(frame_buffer[i], reconstructed_frame.payload[i]);
    }
}


void test_byte_aligned_hdlc_frame(void){
    // Frame: 7E FF FF 7E
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0xFF, 0xFF, HDLC_FLAG_BYTE};
    const size_t hunting_limit = 10;
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);
    for(size_t i = 0; i < sizeof(frame_buffer); i++){
        hdlc_sync_acc_process_byte(&accumulator, frame_buffer[i]);
    }

    uint8_t reconstructed_frame_buffer[128];
    HDLC_FRAME_T reconstructed_frame = {
        .payload = reconstructed_frame_buffer,
        .length = 0,
        .capacity = sizeof(frame_buffer)
    };
    TEST_ASSERT_EQUAL(
        E2S_ERR_HDLC_ACC_FRAME_READY,
        hdlc_sync_acc_poll(&accumulator, &reconstructed_frame)
    );
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state );
    TEST_ASSERT_EQUAL(0, accumulator.bit_offset);

    TEST_ASSERT_EQUAL( sizeof(frame_buffer), reconstructed_frame.length);
    for(size_t i = 0; i < sizeof(frame_buffer); i++){
        TEST_ASSERT_EQUAL_HEX8(frame_buffer[i], reconstructed_frame.payload[i]);
    }
}

void test_hdlc_sync_incomplete_frame_not_ready(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    uint8_t bytes[] = {0x11, 0x22, 0x33};
    for (size_t i = 0; i < sizeof(bytes); i++)
    {
        TEST_ASSERT_TRUE(hdlc_sync_acc_process_byte(&accumulator, bytes[i]));
    }

    uint8_t reconstructed_frame_buffer[16];
    HDLC_FRAME_T reconstructed_frame = {
        .payload  = reconstructed_frame_buffer,
        .length   = 0,
        .capacity = sizeof(reconstructed_frame_buffer),
    };

    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, &reconstructed_frame));
    TEST_ASSERT_EQUAL(0, reconstructed_frame.length);
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
}

void test_hdlc_sync_noise_then_frame(void)
{
    uint8_t noise[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t frame_buffer[] = {HDLC_FLAG_BYTE, 0x01, 0x02, 0x03, HDLC_FLAG_BYTE};

    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    for (size_t i = 0; i < sizeof(noise); i++)
    {
        TEST_ASSERT_TRUE(hdlc_sync_acc_process_byte(&accumulator, noise[i]));
    }
    for (size_t i = 0; i < sizeof(frame_buffer); i++)
    {
        TEST_ASSERT_TRUE(hdlc_sync_acc_process_byte(&accumulator, frame_buffer[i]));
    }

    uint8_t reconstructed_frame_buffer[32];
    HDLC_FRAME_T reconstructed_frame = {
        .payload  = reconstructed_frame_buffer,
        .length   = 0,
        .capacity = sizeof(reconstructed_frame_buffer),
    };

    TEST_ASSERT_EQUAL(E2S_ERR_HDLC_ACC_FRAME_READY,
                      hdlc_sync_acc_poll(&accumulator, &reconstructed_frame));
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
    TEST_ASSERT_EQUAL(0, accumulator.bit_offset);
    TEST_ASSERT_EQUAL(sizeof(frame_buffer), reconstructed_frame.length);
    for (size_t i = 0; i < sizeof(frame_buffer); i++)
    {
        TEST_ASSERT_EQUAL_HEX8(frame_buffer[i], reconstructed_frame.payload[i]);
    }
}

void test_hdlc_sync_accumulator_overflow_rejected(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    for (size_t i = 0; i < RX_HDLC_SYNC_MAX_BUFFER_SIZE; i++)
    {
        TEST_ASSERT_TRUE(hdlc_sync_acc_process_byte(&accumulator, (uint8_t)(i & 0xFF)));
    }
    TEST_ASSERT_FALSE(hdlc_sync_acc_process_byte(&accumulator, 0x00));
}

void test_hdlc_sync_poll_invalid_args_returns_ok(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    uint8_t frame_buf[8] = {0};
    HDLC_FRAME_T frame = {
        .payload = frame_buf,
        .length = 0,
        .capacity = sizeof(frame_buf),
    };

    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(NULL, &frame));
    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, NULL));

    frame.payload = NULL;
    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, &frame));

    frame.payload = frame_buf;
    frame.capacity = 0;
    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, &frame));
}

void test_hdlc_sync_poll_syncing_waits_for_lookahead(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    accumulator.state = HDLC_SYNC_STATE_SYNCING;
    accumulator.bit_offset = 1;
    accumulator.position = 2;
    accumulator.processed = 1; // enter syncing with i == position - 1 => needs lookahead
    accumulator.buffer[0] = 0x12;
    accumulator.buffer[1] = 0x34;

    uint8_t frame_buf[8] = {0};
    HDLC_FRAME_T frame = {
        .payload = frame_buf,
        .length = 0,
        .capacity = sizeof(frame_buf),
    };

    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, &frame));
    TEST_ASSERT_EQUAL_UINT32(1, accumulator.lookahead_wait_syncing);
}

void test_hdlc_sync_poll_synced_waits_for_lookahead(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    accumulator.state = HDLC_SYNC_STATE_SYNCED;
    accumulator.bit_offset = 1;
    accumulator.position = 2;
    accumulator.processed = 1; // enter synced with i == position - 1 => needs lookahead
    accumulator.buffer[0] = 0x56;
    accumulator.buffer[1] = 0x78;

    uint8_t frame_buf[8] = {0};
    HDLC_FRAME_T frame = {
        .payload = frame_buf,
        .length = 0,
        .capacity = sizeof(frame_buf),
    };

    TEST_ASSERT_EQUAL(E2S_OK, hdlc_sync_acc_poll(&accumulator, &frame));
    TEST_ASSERT_EQUAL_UINT32(1, accumulator.lookahead_wait_synced);
}

void test_hdlc_sync_poll_overflow_resets_state(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    accumulator.state = HDLC_SYNC_STATE_SYNCING;
    accumulator.bit_offset = 0;
    accumulator.position = 1;
    accumulator.processed = 0;
    accumulator.buffer[0] = 0xAB;

    uint8_t frame_buf[1] = {0};
    HDLC_FRAME_T frame = {
        .payload = frame_buf,
        .length = 1, // already full
        .capacity = sizeof(frame_buf),
    };

    TEST_ASSERT_EQUAL(E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG,
                      hdlc_sync_acc_poll(&accumulator, &frame));
    TEST_ASSERT_EQUAL_UINT(0, frame.length);
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
    TEST_ASSERT_EQUAL_UINT8(0, accumulator.bit_offset);
}

void test_hdlc_sync_consume_candidate_drops_prefix_and_resets(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    accumulator.position = 6;
    for (size_t i = 0; i < accumulator.position; i++)
    {
        accumulator.buffer[i] = (uint8_t)(0xA0 + i);
    }
    accumulator.candidate_valid = true;
    accumulator.candidate_end = 3;
    accumulator.state = HDLC_SYNC_STATE_SYNCED;
    accumulator.bit_offset = 2;
    accumulator.resume_pending = true;

    hdlc_sync_acc_consume_candidate(&accumulator, true);

    TEST_ASSERT_EQUAL_UINT(3, accumulator.position);
    TEST_ASSERT_EQUAL_HEX8(0xA3, accumulator.buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xA4, accumulator.buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xA5, accumulator.buffer[2]);
    TEST_ASSERT_EQUAL(HDLC_SYNC_STATE_HUNTING, accumulator.state);
    TEST_ASSERT_FALSE(accumulator.candidate_valid);
    TEST_ASSERT_EQUAL_UINT8(0, accumulator.bit_offset);
    TEST_ASSERT_FALSE(accumulator.resume_pending);
    TEST_ASSERT_EQUAL_UINT32(1, accumulator.consume_count);
}

void test_hdlc_sync_consume_candidate_applies_hardcap(void)
{
    HDLC_SYNC_ACCUMULATOR_T accumulator;
    hdlc_sync_acc_init(&accumulator, HDLC_FLAG_BYTE);

    accumulator.position = RX_HDLC_SYNC_MAX_BUFFER_SIZE - 1;
    for (size_t i = 0; i < accumulator.position; i++)
    {
        accumulator.buffer[i] = (uint8_t)i;
    }
    accumulator.candidate_valid = true;
    accumulator.candidate_end = 0;

    hdlc_sync_acc_consume_candidate(&accumulator, true);

    TEST_ASSERT_EQUAL_UINT(16, accumulator.position);
    TEST_ASSERT_EQUAL_UINT32(1, accumulator.hardcap_drop_events);
    TEST_ASSERT_TRUE(accumulator.hardcap_drop_bytes > 0);
}
