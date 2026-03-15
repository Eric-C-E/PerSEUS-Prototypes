from protocol import (
    build_flower_raw_command,
    build_heartbeat,
    build_hello,
    build_set_state_command,
    build_vibration_command,
    encode_message,
    parse_json_line,
)


def main() -> None:
    messages = [
        build_hello("flower_01", "flower"),
        build_heartbeat(),
        build_set_state_command("neutral"),
        build_vibration_command(True),
        build_flower_raw_command(run=True, speed=0.5, amplitude=0.75),
    ]

    for message in messages:
        encoded = encode_message(message)
        decoded = parse_json_line(encoded.decode("utf-8"))
        print("encoded:", encoded)
        print("decoded:", decoded)
        print("---")


if __name__ == "__main__":
    main()
