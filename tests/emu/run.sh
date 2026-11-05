#!/bin/sh
# Run the firmware on cortex-m-emu with host UART input and check the UART
# lines and the SSD1306 frame against tests/emu/oled_25.00.pbm.
#   tests/emu/run.sh path/to/cortex-m-emu-binary
set -eu
emu=$1
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
"$emu" -n 3000000 --uart-rx 'hello\rtemp\r' --oled-dump "$out/oled.pbm" \
    "$here/../../build/firmware.bin" > "$out/uart.txt"
tr -d '\r' < "$out/uart.txt" > "$out/lines.txt"
expect() {
    grep -qx -- "$1" "$out/lines.txt" || { echo "missing line: $1" >&2; cat "$out/lines.txt" >&2; exit 1; }
}
expect 'sensor-node boot'
expect 'temperature: 25.00'
expect 'echo: hello'
[ "$(grep -cx 'temperature: 25.00' "$out/lines.txt")" -ge 2 ] || { echo 'temp command gave no reading' >&2; exit 1; }
if grep -q 'ssd1306 not found' "$out/lines.txt"; then echo 'display not detected' >&2; exit 1; fi
cmp "$out/oled.pbm" "$here/oled_25.00.pbm" || { echo 'OLED frame differs' >&2; exit 1; }
echo "PASS emulator run: boot, temperature, RX echo, temp command, OLED frame"
