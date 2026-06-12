# nexus-protocol

A custom binary network protocol implemented in C — built from scratch over TCP with a 20-byte framed wire format, CRC-32 integrity verification, three-way handshake, stop-and-wait ARQ reliability, and graceful connection teardown.

## Features

- Binary framing with magic-byte validation and CRC-32 checksums
- Three-way handshake with initial sequence number exchange
- Stop-and-wait ARQ with NACK-triggered retransmission (up to 5 retries)
- Full connection state machine: CLOSED -> SYN_SENT -> ESTABLISHED -> FIN_WAIT -> CLOSED
- Blocking TCP transport abstraction with full send/recv loop
- Echo server and client as reference implementations

## Project Structure

    include/protocol.h     -- types, constants, and full API surface
    src/frame.c            -- binary frame encode/decode and CRC-32 engine
    src/reliable.c         -- sequence numbers, ACK/NACK, retransmission
    src/transport.c        -- TCP socket abstraction
    src/conn.c             -- connection state machine and handshake
    src/server.c           -- echo server
    src/client.c           -- interactive echo client
    docs/PROTOCOL_SPEC.md  -- full wire format and state machine specification

## Build

    make

Requires gcc and POSIX sockets. Tested on macOS and Linux.

## Run

Terminal 1:

    ./bin/server

Terminal 2:

    ./bin/client

Type any message and press Enter. The server echoes it back over the protocol. Press Ctrl+D to close the connection gracefully.

## Wire Format

20-byte header, all fields big-endian:

    magic (4) | version (1) | type (1) | flags (2) | seq (4) | payload_len (4) | crc32 (4)

See docs/PROTOCOL_SPEC.md for the full specification.
