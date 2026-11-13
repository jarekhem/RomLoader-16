#!/usr/bin/env python3
"""
Host uploader tool for RomLoader-16 (8086 / V50 Serial Bootloader).
Supports both:
  - Bochs emulated COM1 over TCP (--tcp localhost:4444)
  - Physical UART over pyserial (--port COM3 --baud 115200)
"""

import argparse
import os
import select
import socket
import struct
import subprocess
import sys
import time


MAGIC = b"\x55\xAA" + b"RL"
CMD_PING = 0x01
CMD_LOAD = 0x02
CMD_EXEC = 0x03
CMD_LOAD_AND_EXEC = 0x04

ACK = 0x06
NAK = 0x15


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def run_bridge_worker() -> int:
    """
    Persistent background TCP bridge that keeps Bochs's single-shot
    com1 socket-server connection alive across multiple send_payload.py runs.
    """
    hdr = sys.stdin.buffer.read(8)
    if len(hdr) != 8:
        return 1
    raw_len, srv_len = struct.unpack("<II", hdr)
    raw_share = sys.stdin.buffer.read(raw_len)
    srv_share = sys.stdin.buffer.read(srv_len)

    bochs_sock = socket.fromshare(raw_share)
    srv_sock = socket.fromshare(srv_share)
    bochs_sock.setblocking(False)
    srv_sock.setblocking(False)

    sys.stdout.buffer.write(b"OK")
    sys.stdout.buffer.flush()

    client_sock = None
    try:
        while True:
            rlist = [bochs_sock, srv_sock]
            if client_sock is not None:
                rlist.append(client_sock)

            readable, _, _ = select.select(rlist, [], [], 1.0)

            if bochs_sock in readable:
                try:
                    data = bochs_sock.recv(4096)
                except OSError:
                    break
                if not data:
                    break  # Bochs closed COM1 socket (emulator exited)
                if client_sock is not None:
                    try:
                        client_sock.sendall(data)
                    except OSError:
                        client_sock.close()
                        client_sock = None

            if srv_sock in readable:
                try:
                    new_client, _ = srv_sock.accept()
                    new_client.setblocking(False)
                    if client_sock is not None:
                        try:
                            client_sock.close()
                        except OSError:
                            pass
                    client_sock = new_client
                except OSError:
                    pass

            if client_sock is not None and client_sock in readable:
                try:
                    data = client_sock.recv(4096)
                except OSError:
                    data = b""
                if not data:
                    try:
                        client_sock.close()
                    except OSError:
                        pass
                    client_sock = None
                else:
                    try:
                        bochs_sock.sendall(data)
                    except OSError:
                        break
    finally:
        if client_sock is not None:
            try:
                client_sock.close()
            except OSError:
                pass
        try:
            srv_sock.close()
        except OSError:
            pass
        try:
            bochs_sock.close()
        except OSError:
            pass
    return 0


class TcpTransport:
    def __init__(self, host_port: str, connect_timeout: float = 30.0):
        host, port_s = host_port.split(":")
        port = int(port_s)
        print(f"Connecting to Bochs COM1 at {host}:{port} (waiting up to {connect_timeout:.0f}s)...")
        deadline = time.time() + connect_timeout
        last_err = None
        while time.time() < deadline:
            try:
                raw_sock = socket.create_connection((host, port), timeout=2.0)
                self.sock = self._ensure_persistent_bridge(raw_sock, host, port)
                self.sock.settimeout(0.3)
                print(f"Connected to {host}:{port}!")
                return
            except OSError as e:
                last_err = e
                time.sleep(0.25)
        raise RuntimeError(f"Could not connect to {host}:{port} (is Bochs running with com1 socket-server?): {last_err}")

    @staticmethod
    def _ensure_persistent_bridge(raw_sock: socket.socket, host: str, port: int) -> socket.socket:
        """
        Bochs's socket-server closes its listening socket immediately after the
        first accept(). If we just connected directly to Bochs, port 4444 is now
        unbound; we spawn a background bridge worker that holds the Bochs socket
        open and re-listens on port 4444 so subsequent script runs can reconnect.
        """
        if not hasattr(raw_sock, "share"):
            return raw_sock

        time.sleep(0.05)
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            srv.bind((host, port))
            srv.listen(5)
        except OSError:
            srv.close()
            return raw_sock  # Persistent bridge is already listening on (host, port)

        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0x08000000)
        proc = subprocess.Popen(
            [sys.executable, os.path.abspath(__file__), "--_bridge-worker"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            creationflags=creationflags,
        )
        raw_share = raw_sock.share(proc.pid)
        srv_share = srv.share(proc.pid)
        assert proc.stdin is not None and proc.stdout is not None
        proc.stdin.write(struct.pack("<II", len(raw_share), len(srv_share)) + raw_share + srv_share)
        proc.stdin.flush()
        proc.stdin.close()

        raw_sock.close()
        srv.close()

        ready = proc.stdout.read(2)
        proc.stdout.close()
        if ready != b"OK":
            raise RuntimeError("Failed to initialize persistent Bochs TCP bridge")

        return socket.create_connection((host, port), timeout=5.0)

    def set_timeout(self, timeout: float) -> None:
        self.sock.settimeout(timeout)

    def write(self, data: bytes) -> None:
        self.sock.sendall(data)

    def read(self, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise RuntimeError("Connection closed by peer")
            buf.extend(chunk)
        return bytes(buf)

    def read_available(self) -> bytes:
        try:
            return self.sock.recv(256)
        except socket.timeout:
            return b""

    def close(self) -> None:
        self.sock.close()


class SerialTransport:
    def __init__(self, port: str, baud: int):
        import serial  # type: ignore
        print(f"Opening serial port {port} at {baud} baud (8N1)...")
        self.ser = serial.Serial(
            port=port,
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.3,
            write_timeout=2.0,
            rtscts=False,
            dsrdtr=False,
        )
        self.ser.reset_input_buffer()

    def set_timeout(self, timeout: float) -> None:
        self.ser.timeout = timeout

    def write(self, data: bytes) -> None:
        self.ser.write(data)
        self.ser.flush()

    def read(self, n: int) -> bytes:
        data = self.ser.read(n)
        if len(data) != n:
            raise RuntimeError(f"Serial timeout (expected {n} bytes, got {len(data)})")
        return data

    def read_available(self) -> bytes:
        return self.ser.read(256)

    def close(self) -> None:
        self.ser.close()


def wait_for_loader_sync(transport, timeout: float = 30.0) -> None:
    """
    Waits until the target 8086 CPU has finished BIOS/KBC/UART init
    and responds to CMD_PING with 'OK'.
    """
    print("Waiting for RomLoader-16 target to initialize and answer PING...")
    deadline = time.time() + timeout
    rx_window = bytearray()

    while time.time() < deadline:
        transport.write(MAGIC + bytes([CMD_PING]))
        time.sleep(0.25)
        chunk = transport.read_available()
        if chunk:
            rx_window.extend(chunk)
            if b"OK" in rx_window:
                # Drain any queued responses from earlier PING retries
                time.sleep(0.15)
                while transport.read_available():
                    pass
                print("Target synchronized (PING -> OK)!")
                transport.set_timeout(5.0)
                return
            else:
                print(f"  [RX raw]: {chunk!r}")

    raise RuntimeError("Timed out waiting for RomLoader-16 PING response (if using debug_bochs, type 'c' in debugger).")


def parse_hex16(val: str) -> int:
    return int(val, 16)


def main() -> int:
    if len(sys.argv) == 2 and sys.argv[1] == "--_bridge-worker":
        return run_bridge_worker()

    parser = argparse.ArgumentParser(description="Upload and run 8086 binary payload via RomLoader-16")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--tcp", help="Bochs COM1 TCP socket (e.g. localhost:4444)")
    group.add_argument("--port", help="Physical serial port (e.g. COM3)")

    parser.add_argument("--baud", type=int, default=115200, help="Baud rate for physical serial port")
    parser.add_argument("--seg", default="1000", help="Target segment in hex (default: 1000)")
    parser.add_argument("--off", default="0000", help="Target offset in hex (default: 0000)")
    parser.add_argument("--run", action="store_true", help="Immediately execute after loading")
    parser.add_argument("--ping", action="store_true", help="Only send PING to verify link")
    parser.add_argument("binary", nargs="?", help="Path to binary payload (.bin)")

    args = parser.parse_args()

    transport = TcpTransport(args.tcp) if args.tcp else SerialTransport(args.port, args.baud)
    try:
        wait_for_loader_sync(transport)

        if args.ping:
            return 0

        if not args.binary:
            parser.error("binary path is required unless --ping is specified")

        with open(args.binary, "rb") as f:
            payload = f.read()

        if not (1 <= len(payload) <= 0xFFFF):
            raise ValueError(f"Payload size must be 1..65535 bytes (got {len(payload)})")

        seg = parse_hex16(args.seg)
        off = parse_hex16(args.off)
        crc = crc16_ccitt(payload)
        cmd = CMD_LOAD_AND_EXEC if args.run else CMD_LOAD

        cmd_prefix = MAGIC + struct.pack("<B", cmd)
        meta_hdr = struct.pack("<HHHH", seg, off, len(payload), crc)
        print(f"Sending header -> {seg:04X}:{off:04X}, size={len(payload)} bytes, CRC16=0x{crc:04X}...")
        transport.write(cmd_prefix)
        time.sleep(0.002)
        transport.write(meta_hdr)

        ack1 = transport.read(1)[0]
        if ack1 != ACK:
            raise RuntimeError(f"Header rejected by loader (byte=0x{ack1:02X})")

        print("Streaming payload...")
        transport.write(payload)

        ack2 = transport.read(1)[0]
        if ack2 != ACK:
            raise RuntimeError(f"Payload CRC check failed on target (byte=0x{ack2:02X})")

        print("Success! Payload verified in RAM." + (" Executing!" if args.run else ""))
        if args.run:
            transport.set_timeout(0.5)
            time.sleep(0.2)
            out = transport.read_available()
            if out:
                print(f"Payload UART output: {out.decode('latin1', errors='replace').strip()}")
        return 0
    finally:
        transport.close()


if __name__ == "__main__":
    sys.exit(main())
