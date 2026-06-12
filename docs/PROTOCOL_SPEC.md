# Custom Binary Protocol — Specification v1.0

## Overview

A lightweight, reliable, connection-oriented binary protocol layered over TCP.
Provides ordered delivery, error detection via CRC-32, and explicit connection
lifecycle management through a three-way handshake and graceful teardown.

## Wire Format

Every message begins with a fixed 20-byte header:

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                     Magic (0xDEADBEEF)                        |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |    Version    |     Type      |            Flags              |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      Sequence Number                          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                      Payload Length                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                         CRC-32                                |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                   Payload (variable)                          |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

All multi-byte fields are big-endian (network byte order).

| Field          | Size    | Description                                           |
|----------------|---------|-------------------------------------------------------|
| Magic          | 4 bytes | Always 0xDEADBEEF. Frames without this are dropped.   |
| Version        | 1 byte  | Protocol version. Current: 0x01.                      |
| Type           | 1 byte  | Frame type (see below).                               |
| Flags          | 2 bytes | Reserved for future use. Set to 0x0000.               |
| Sequence       | 4 bytes | Monotonically increasing per-connection counter.      |
| Payload Length | 4 bytes | Byte length of payload. Zero for control frames.      |
| CRC-32         | 4 bytes | CRC-32 over entire frame with checksum field zeroed.  |
| Payload        | Varies  | Application data. Max 65535 bytes.                    |

## Frame Types

| Value | Name           | Direction       | Description                           |
|-------|----------------|-----------------|---------------------------------------|
| 0x01  | HANDSHAKE      | Client -> Server| Initiates connection, carries ISN.    |
| 0x02  | HANDSHAKE_ACK  | Server -> Client| Acknowledges SYN, carries server ISN. |
| 0x03  | DATA           | Both            | Application payload.                  |
| 0x04  | ACK            | Both            | Acknowledges DATA or handshake step.  |
| 0x05  | NACK           | Both            | Negative ack, triggers retransmit.    |
| 0x06  | KEEPALIVE      | Both            | Keeps idle connection alive.          |
| 0x07  | FIN            | Both            | Initiates graceful connection close.  |
| 0x08  | FIN_ACK        | Both            | Acknowledges FIN.                     |

## Connection State Machine

    Client                          Server
      |                               |
      |--- HANDSHAKE (ISN_c) -------->|  CLOSED -> SYN_RECV
      |                               |
      |<-- HANDSHAKE_ACK (ISN_s) -----|  SYN_SENT -> waiting
      |                               |
      |--- ACK ---------------------->|  Both -> ESTABLISHED
      |                               |
      |<=== DATA / ACK / NACK =======>|  Bidirectional reliable exchange
      |                               |
      |--- FIN ---------------------->|  Client -> FIN_WAIT
      |                               |
      |<-- FIN_ACK -------------------|  Server -> FIN_WAIT
      |                               |
      |--- ACK ---------------------->|  Both -> TIME_WAIT -> CLOSED

### States

| State       | Description                                               |
|-------------|-----------------------------------------------------------|
| CLOSED      | No active connection.                                     |
| SYN_SENT    | Client sent HANDSHAKE, awaiting HANDSHAKE_ACK.            |
| SYN_RECV    | Server received HANDSHAKE, sent HANDSHAKE_ACK.            |
| ESTABLISHED | Three-way handshake complete. Data exchange active.       |
| FIN_WAIT    | FIN sent, awaiting FIN_ACK.                               |
| CLOSE_WAIT  | FIN received, local close pending.                        |
| TIME_WAIT   | Waiting for stale packets before full close.              |

## Reliability

Data frames use a stop-and-wait ARQ scheme:

1. Sender transmits a DATA frame with a sequence number.
2. Receiver validates CRC-32. On failure, sends NACK.
3. Receiver validates sequence number. Out-of-order frames trigger NACK.
4. On success, receiver sends ACK with matching sequence number.
5. Sender retransmits up to MAX_RETRIES (5) times on NACK or timeout.

## Error Codes

| Code | Name               | Meaning                                  |
|------|--------------------|------------------------------------------|
|  0   | PROTO_OK           | Success.                                 |
| -1   | PROTO_ERR_MAGIC    | Magic bytes invalid.                     |
| -2   | PROTO_ERR_CRC      | Checksum mismatch.                       |
| -3   | PROTO_ERR_LEN      | Payload length out of range.             |
| -4   | PROTO_ERR_SEQ      | Sequence number unexpected.              |
| -5   | PROTO_ERR_STATE    | Operation invalid in current state.      |
| -6   | PROTO_ERR_IO       | Underlying I/O failure.                  |
| -7   | PROTO_ERR_TIMEOUT  | Peer unresponsive within timeout window. |
