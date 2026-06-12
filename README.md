# nexus-protocol 🔗

A production-grade custom binary network protocol written from scratch in C. No libraries, no abstractions — just raw sockets, a handcrafted wire format, and a full connection lifecycle built the way it should be.

Built this to understand what actually happens under the hood when two machines talk to each other. Spoiler: its a lot of careful byte manipulation.

---

## What it does

- 🧱 **Binary framing** — every message is prefixed with a 20-byte header containing magic bytes, version, type, flags, sequence number, payload length, and a CRC-32 checksum
- 🤝 **Three-way handshake** — client and server exchange initial sequence numbers before any data flows, similar to how TCP establishes connections
- 🔁 **Stop-and-wait ARQ** — every data frame is acknowledged before the next one is sent; NACK triggers retransmission up to 5 times
- 🧮 **CRC-32 integrity** — every frame is verified on arrival; corrupted frames are rejected and NACKed
- 📡 **Full state machine** — CLOSED → SYN_SENT → ESTABLISHED → FIN_WAIT → CLOSED, with every transition logged
- 🔌 **Graceful teardown** — FIN/FIN_ACK exchange ensures both sides close cleanly

---

## Project structure

    nexus-protocol/
    ├── include/
    │   └── protocol.h          # types, constants, full API
    ├── src/
    │   ├── frame.c             # wire format encode/decode + CRC-32 engine
    │   ├── reliable.c          # sequence numbers, ACK/NACK, retransmission
    │   ├── transport.c         # TCP socket abstraction
    │   ├── conn.c              # connection state machine + handshake
    │   ├── server.c            # echo server
    │   └── client.c            # interactive echo client
    ├── docs/
    │   └── PROTOCOL_SPEC.md    # full wire format + state machine spec
    └── Makefile

---

## Build

    make

Requires gcc and a POSIX-compliant system. Tested on macOS and Linux.

---

## Run

Start the server in one terminal:

    ./bin/server

Connect with the client in another:

    ./bin/client

Type any message and hit Enter — it travels through the full protocol stack, gets framed, checksummed, transmitted, verified, acknowledged, and echoed back. Press Ctrl+D to close the connection gracefully and watch the FIN handshake play out in the logs.

---

## Wire format

20-byte header, all fields in network byte order (big-endian):

    +---------+---------+-------+----------+--------------+---------+
    |  magic  | version | type  |  flags   |     seq      | plen    |
    | 4 bytes | 1 byte  | 1 byte| 2 bytes  |   4 bytes    | 4 bytes |
    +---------+---------+-------+----------+--------------+---------+
    |  crc32  |              payload (variable)                      |
    | 4 bytes |                                                       |
    +---------+-------------------------------------------------------+

Full specification with frame types, state machine diagrams, and error codes in [docs/PROTOCOL_SPEC.md](docs/PROTOCOL_SPEC.md).

---

## Why

Most networking code you see online uses high-level libraries that hide everything interesting. This is what it looks like when you do it yourself — every byte intentional, every state transition explicit.
