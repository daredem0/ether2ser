/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/error.h
 * Purpose: Common error codes for ether2ser.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

#ifndef ERROR_H
#define ERROR_H

// Related headers

// Standard library headers

// Project Headers

// Generated headers

/**
 * @brief Common error codes returned by ether2ser modules.
 */
typedef enum
{
    E2S_OK = 0,
    E2S_ERR_GENERIC_ERROR,

    /* CLI parsing/commands */
    E2S_ERR_CLI_EMPTY_LINE,
    E2S_ERR_CLI_UNKNOWN_COMMAND,
    E2S_ERR_CLI_USAGE_SET,
    E2S_ERR_CLI_USAGE_GET,
    E2S_ERR_CLI_USAGE_PININFO,
    E2S_ERR_CLI_UNKNOWN_PIN,
    E2S_ERR_CLI_PIN_INPUT_ONLY,
    E2S_ERR_CLI_LINE_FORMAT,
    E2S_ERR_CLI_LINE_TRUNCATED,

    /* Event queue */
    E2S_ERR_EVENT_QUEUE_FULL,
    E2S_ERR_EVENT_QUEUE_EMPTY,

    /* PIO TX/RX driver */
    E2S_ERR_PIO_UNAVAILABLE,
    E2S_ERR_PIO_RX_EMPTY,
    E2S_ERR_PIO_TX_FULL,
    E2S_V24_RUNTIME_NOT_INITIALIZED,

    /* HDLC encoder */
    E2S_ERR_HDLC_ENCODE_FAILED,
    E2S_ERR_HDLC_ENCODE_INVALID_ARGS,
    E2S_ERR_HDLC_ENCODE_FRAME_TOO_SMALL,
    E2S_ERR_HDLC_ENCODE_FRAME_NOT_EMPTY,
    E2S_ERR_HDLC_ENCODE_OVERFLOW,

    /* HDLC decoder */
    E2S_ERR_HDLC_DECODE_INVALID_ARGS,
    E2S_ERR_HDLC_DECODE_INVALID_FRAME,
    E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_LONG,
    E2S_ERR_HDLC_DECODE_PAYLOAD_TOO_SHORT,
    E2S_ERR_HDLC_DECODE_CRC_MISMATCH,

    /* Frame Acccumulator */
    E2S_ERR_HDLC_ACC_FRAME_READY,

    /* W5500 */
    E2S_ERR_W5500_INIT_FAILED,
    E2S_ERR_W5500_SOCKET_OPEN_FAILED,
    E2S_ERR_W5500_SEND_FAILED,
    E2S_ERR_W5500_RECV_FAILED,

    /* TX Queue */
    E2S_ERR_TX_QUEUE_FULL,
    E2S_ERR_TX_QUEUE_NOT_INITIALIZED
} e2s_error_t;

/**
 * @brief Print error message and panic.
 * @param reason Error code.
 */
void fatal_panic(e2s_error_t reason);

#endif /* ERROR_H */
