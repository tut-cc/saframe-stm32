#!/usr/bin/env python3
"""Read the ST-LINK VCP and write raw bytes to a file.

The baud rate is set on the same file descriptor that reads, because macOS
discards termios settings when the last descriptor closes: a separate
`stty -f` followed by `cat` reopens the port at 9600 bps, and the ST-LINK
then forwards that line coding to its UART and drops every byte.
Usage: vcp_read.py /dev/cu.usbmodemXXXX SECONDS OUTFILE
"""
import fcntl
import os
import struct
import sys
import termios
import time

dev, secs, out = sys.argv[1], float(sys.argv[2]), sys.argv[3]
fd = os.open(dev, os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
iflag, oflag, cflag, lflag, _, _, cc = termios.tcgetattr(fd)
cflag = (cflag & ~(termios.CSIZE | termios.PARENB | termios.CSTOPB | termios.CRTSCTS))
cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL
cc[termios.VMIN] = 0
cc[termios.VTIME] = 1
termios.tcsetattr(fd, termios.TCSANOW, [0, 0, cflag, 0, termios.B115200, termios.B115200, cc])
TIOCMBIS, TIOCM_DTR, TIOCM_RTS = 0x8004746C, 0x002, 0x004
try:
    fcntl.ioctl(fd, TIOCMBIS, struct.pack("I", TIOCM_DTR | TIOCM_RTS))
except OSError as e:
    print("ioctl DTR/RTS:", e, file=sys.stderr)
fcntl.fcntl(fd, fcntl.F_SETFL, fcntl.fcntl(fd, fcntl.F_GETFL) & ~os.O_NONBLOCK)
termios.tcflush(fd, termios.TCIFLUSH)
end = time.time() + secs
total = 0
with open(out, "wb") as f:
    while time.time() < end:
        try:
            data = os.read(fd, 4096)
        except BlockingIOError:
            data = b""
        if data:
            f.write(data)
            f.flush()
            total += len(data)
        else:
            time.sleep(0.05)
os.close(fd)
print("bytes:", total)
