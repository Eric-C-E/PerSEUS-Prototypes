# Protocol Notes

This directory is the protocol contract surface shared between the GUI and device firmware.

Current prototype transport:

- TCP socket
- Newline-delimited JSON messages

Current message families:

- `hello`: device announces `device_id` and `device_kind`
- `heartbeat`: device keepalive
- `event`: device-to-GUI event with `event` name and optional payload
- `command`: GUI-to-device command payload

Python reference implementation lives in `shared/python/perseus_shared/protocol.py`.

The ESP-IDF firmware projects should treat this folder as the protocol source of truth as the contract evolves, even if the transport later changes away from raw TCP.
