#!/bin/bash
# launch x64 (console, true-drive) with iecdelay-asm.prg, then drive
# the remote monitor: read C64 dbg cells, drive RAM, and dump the screen.
#
# usage: ./vdump.sh [wait_seconds] [prg_file]
#   default wait: 18
#   default prg:  iectest.prg

WAIT=${1:-18}
PRG=${2:-iecdelay-asm.prg}

pkill -9 x64 2>/dev/null
sleep 1
rm -f /tmp/vice.log /tmp/screen.bin

nohup x64 -console \
  -drive8type 1541 -drive8truedrive \
  -autostart "$PRG" \
  -remotemonitor > /tmp/vice.log 2>&1 &

disown
sleep "$WAIT"

python3 - <<'EOF'
import socket, time

s = socket.create_connection(('127.0.0.1', 6510), timeout=5)
time.sleep(0.3)
try:
    s.recv(65536)
except OSError:
    pass

def cmd(c, wait=0.8):
    s.sendall(c.encode() + b'\n')
    time.sleep(wait)
    try:
        return s.recv(65536).decode(errors='replace')
    except OSError:
        return '***OSError***'

# C64 debug cells (upload final status / last markers)
print(cmd('dev c:', 1))
print('== C64 dbg 1ba8-1bab ==')
print(cmd('m 1ba8 1bab', 1))
print('== C64 meas 1cc5-1d0b ==')
print(cmd('m 1cc5 1d0b', 1))
print('== C64 r_tab 1c0f-1c6e ==')
print(cmd('m 1c0f 1c6e', 1))
print('== C64 drv 1d0c-1d32 ==')
print(cmd('m 1d0c 1d32', 1))

# drive RAM (upload target $0500)
print(cmd('dev 8:', 1))
print('== drive 8: 0500-0526 ==')
print(cmd('m 0500 0526', 1))

# screen dump
print(cmd('dev c:', 1))
s.sendall(b'save "/tmp/screen.bin" 0 $0400 $07e7\n')
time.sleep(1)
try:
    s.recv(65536)
except OSError:
    pass
print(cmd('g', 0.3))
s.close()

data = open('/tmp/screen.bin', 'rb').read()[2:]
tbl = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\xa3]^"
def ch(c):
    c &= 0x7f
    if c == 32:
        return ' '
    if 1 <= c <= 26:
        return chr(ord('a') + c - 1)   # lowercase charset ({swlc})
    if c < 32:
        return tbl[c] if c < len(tbl) else '?'
    if c < 64:
        return " !\"#$%&'()*+,-./0123456789:;<=>?"[c - 32]
    if c < 96:
        return tbl[c - 64]             # uppercase letters in lowercase set
    return '?'

print('== screen ==')
for r in range(25):
    print(f'{r:2d} |' + ''.join(ch(data[r * 40 + k]) for k in range(40)) + '|')
EOF

pkill -9 x64 2>/dev/null
echo done
