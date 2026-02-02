
// Related headers
#include "hdlc_sync.h"

// Standard library headers
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Library Headers

// Project Headers
#include "system/error.h"
#include "hdlc_common.h"

// Generated headers

void hdlc_sync_acc_init(HDLC_SYNC_ACCUMULATOR_T *accumulator, uint8_t sync_byte){
    accumulator->position = 0;
    accumulator->bit_offset = 0;
    accumulator->state = HDLC_SYNC_STATE_HUNTING;
    accumulator->sync_byte = sync_byte;
    accumulator->sync_accumulator = 0;
}

bool hdlc_sync_acc_process_byte(HDLC_SYNC_ACCUMULATOR_T *accumulator, uint8_t byte){
    if (!accumulator){
        return false;
    }
    if(accumulator->position >= RX_HDLC_SYNC_MAX_BUFFER_SIZE){
        return false;
    }
    accumulator->buffer[accumulator->position] = byte;
    accumulator->position++;
    return true;
}


e2s_error_t hdlc_sync_acc_poll(HDLC_SYNC_ACCUMULATOR_T *accumulator, HDLC_FRAME_T *out_frame){
    size_t out_frame_position = 0;
    for(size_t i = 0; i < accumulator->position; i++){
        switch(accumulator->state){
            case HDLC_SYNC_STATE_HUNTING:
                // accumulator->sync_accumulator = (accumulator->sync_accumulator >> 8) | ((uint16_t)accumulator->buffer[i] << 8);
                accumulator->sync_accumulator = (accumulator->sync_accumulator << 8) | accumulator->buffer[i];
                // printf("Sync Accumulator: %02X\n", accumulator->sync_accumulator);
                // printf("Sync Accumulator: ");
                for (size_t bit_pos = 0; bit_pos < 8; bit_pos++){
                    // printf("Bit %d: ", bit_pos);
                    // printf("%02X ", ((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF);
                    // printf("\n");

                    // if ((( accumulator->sync_accumulator << bit_pos) & 0xFF) == accumulator->sync_byte){
                    if ((((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF) == accumulator->sync_byte){
                        // printf("SYNCED\n");
                        // printf("Bit %d: ", bit_pos);
                        // printf("%02X ", ((accumulator->sync_accumulator << bit_pos) >> 8) & 0xFF);
                        // printf("\n");
                        // printf("Sync Accumulator: %02X\n", accumulator->sync_accumulator);


                        accumulator->state = HDLC_SYNC_STATE_SYNCED;
                        accumulator->bit_offset = bit_pos;
                        for (size_t j = 0; j < sizeof(accumulator->sync_accumulator); j++){
                            out_frame->payload[out_frame_position] = (accumulator->buffer[i-1+j] << accumulator->bit_offset) | (accumulator->buffer[i+j] >> (8 - accumulator->bit_offset));
                            // printf("Writing output byte: %02X\n", out_frame->payload[out_frame_position]);
                            ++(out_frame->length);
                            ++out_frame_position;
                        }
                        break;
                    }
                }
                // printf("\n");
                break;
            case HDLC_SYNC_STATE_SYNCED:
                uint8_t aligned;
                if (accumulator->bit_offset == 0){
                    aligned = accumulator->buffer[i];
                }
                else{
                    aligned = (accumulator->buffer[i] << accumulator->bit_offset) | (accumulator->buffer[i+1] >> (8 - accumulator->bit_offset));
                }
                out_frame->payload[out_frame_position] = aligned;
                // printf("Writing output byte: %02X\n", out_frame->payload[out_frame_position]);
                ++(out_frame->length);
                ++out_frame_position;
                if (aligned == accumulator->sync_byte) {
                    return E2S_ERR_HDLC_ACC_FRAME_READY;
                }
                break;
            default:
                return E2S_OK;
        }
    }

    return E2S_ERR_HDLC_ACC_FRAME_READY;
}

