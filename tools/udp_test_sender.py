#!/usr/bin/env python3
# V4 - Live UDP Capture test sender
# Sends sample UDP payloads to the port your app is bound to.
# Each payload is the UDP PAYLOAD only - the OS adds Ethernet/IP/UDP headers.
#
# Usage:
#   python3 udp_test_sender.py
#   python3 udp_test_sender.py 127.0.0.1 5005
#
# Stop with Ctrl+C.

import socket
import sys
import time

def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"

    try:
        port = int(sys.argv[2]) if len(sys.argv) > 2 else 5005
    except ValueError:
        print("Error: port must be an integer.", file=sys.stderr)
        return 2

    if not (1 <= port <= 65535):
        print("Error: port must be in range 1..65535.", file=sys.stderr)
        return 2

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (host, port)

    # Sample payloads (hex). First 1-2 bytes act as a header prefix
    # so you can test header-filter matching in live mode.
    samples = [
        bytes.fromhex("AA550102030405060708"),  # header AA55
        bytes.fromhex("AA550A0B0C0D0E0F"),       # header AA55
        bytes.fromhex("BB6601020304"),           # header BB66
        bytes.fromhex("AA55"),                   # short packet test
    ]

    print("Sending to {}:{} - Ctrl+C to stop".format(host, port))
    try:
        while True:
            for payload in samples:
                sock.sendto(payload, target)
                print("sent", payload.hex())
                time.sleep(0.5)
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        sock.close()

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
