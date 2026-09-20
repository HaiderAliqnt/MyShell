#!/usr/bin/env python3
"""
drives shell through a real pseudo-terminal.

Piped input cannot exercise line editing or terminal signals, so this test
checks the interactive-only behaviour:
  * the prompt is shown
  * Ctrl-C at the prompt abandons the line but keeps the shell alive
  * Ctrl-C while a foreground command runs kills the command, not the shell
  * Ctrl-C leaves background jobs running
  * Ctrl-Z cannot stop the shell
  * Ctrl-D exits the shell
Usage: CS311_A01_2024385_terminal.py /path/to/mysh
"""
import fcntl
import os
import pty
import select
import struct
import sys
import termios
import time

MYSH = sys.argv[1]
results = []


def report(name, ok, detail=""):
    results.append(ok)
    print(f"{'PASS' if ok else 'FAIL'}  {name}" + ("" if ok else f"  ({detail})"))


class Session:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            # Give the pty a size, as a real terminal window would have.
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
            os.environ["TERM"] = "xterm"
            os.environ["HOME"] = "/nonexistent-home"
            os.execv(MYSH, [MYSH])
        self.buf = b""

    def send(self, data):
        os.write(self.fd, data)

    def expect(self, text, timeout=5.0):
        """Read until `text` appears in what was received since the last expect."""
        needle = text.encode()
        end = time.time() + timeout
        while needle not in self.buf:
            left = end - time.time()
            if left <= 0:
                return False
            r, _, _ = select.select([self.fd], [], [], left)
            if r:
                try:
                    chunk = os.read(self.fd, 4096)
                except OSError:
                    return False
                if not chunk:
                    return False
                if b"\x1b[6n" in chunk:        # cursor-position query:
                    os.write(self.fd, b"\x1b[1;1R")   # answer like a terminal
                self.buf += chunk
        self.buf = self.buf.split(needle, 1)[1]
        return True

    def alive(self):
        pid, _ = os.waitpid(self.pid, os.WNOHANG)
        return pid == 0

    def exit_status(self, timeout=3.0):
        end = time.time() + timeout
        while time.time() < end:
            pid, status = os.waitpid(self.pid, os.WNOHANG)
            if pid:
                return os.waitstatus_to_exitcode(status)
            time.sleep(0.05)
        return None


s = Session()
report("prompt is displayed", s.expect("[mysh]"))

s.send(b"echo hi\r")
report("command runs on a terminal", s.expect("hi") and s.expect("[mysh]"))

s.send(b"echo half-typed")
time.sleep(0.2)
s.send(b"\x03")                                   # Ctrl-C at the prompt
s.expect("[mysh]")
s.send(b"echo alive-after-ctrl-c\r")
report("Ctrl-C at prompt keeps shell alive",
       s.expect("alive-after-ctrl-c") and s.alive())
s.expect("[mysh]")

s.send(b"sleep 30\r")
time.sleep(0.5)
t0 = time.time()
s.send(b"\x03")                                   # Ctrl-C during sleep
back = s.expect("[mysh]", timeout=3)
report("Ctrl-C kills the foreground command", back and time.time() - t0 < 2.5)
report("... and the shell survives it", s.alive())

s.send(b"sleep 31 &\r")
s.expect("[bg] pid")
s.expect("[mysh]")
s.send(b"\x03")                                   # Ctrl-C must not reach the job
s.expect("[mysh]")
s.send(b"ps\r")
report("Ctrl-C leaves background jobs running", s.expect("sleep 31"))
s.expect("[mysh]")

s.send(b"sleep 30\r")
time.sleep(0.4)
s.send(b"\x1a")                                   # Ctrl-Z is ignored
time.sleep(0.3)
s.send(b"\x03")
report("Ctrl-Z does not wedge the shell", s.expect("[mysh]", timeout=3) and s.alive())

s.send(b"\x04")                                   # Ctrl-D on an empty line
code = s.exit_status()
# Like bash, EOF exits with the status of the last command; the last one here
# was a sleep interrupted by Ctrl-C (128 + SIGINT = 130).
report("Ctrl-D exits the shell", code == 130, f"exit status {code}")

os.system("pkill -f 'sleep 31' 2>/dev/null")
sys.exit(0 if all(results) else 1)
