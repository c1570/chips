#!/usr/bin/env python3
"""
Convert C64 emulator trace files to a canonical format.

Handles two formats:
- RP2 format: .C:addr  opcodes  mnemonic  A:xx X:xx Y:xx SP:xx flags  cycles
- VICE format: .addr raster cycle_in_line  abs_cycles  opcodes  mnemonic  packed_regs
"""

import argparse
import re
import sys
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class Instruction:
    """Normalized instruction representation from either trace format."""
    addr: str           # Uppercase hex address (e.g., "ED23")
    opcodes: str        # Raw opcode bytes (e.g., "78" or "20 97 EE")
    mnemonic: str       # Instruction mnemonic (e.g., "SEI", "JSR $EE97")
    a: int              # Accumulator (0-255)
    x: int              # X register (0-255)
    y: int              # Y register (0-255)
    sp: int             # Stack pointer (0-255)
    cycle: int          # Cycle count


def parse_rp2_line(line: str) -> Optional[Instruction]:
    """
    Parse RP2 trace format:
    .C:ed23  78          SEI            A:28 X:01 Y:01 SP:f3 ..-....C       199045
    .C:ed24  20 97 EE    JSR $EE97      A:28 X:01 Y:01 SP:f3 ..-..I.C       199047
    .C:ee97  AD 00 DD    LDA $DD00      A:28 X:01 Y:01 SP:f1 ..-..I.C       199053
    """
    if not line.startswith('.C:') and not line.startswith('.8:'):
        return None

    # Find A: to split the line
    a_pos = line.find(' A:')
    if a_pos == -1:
        return None

    # Parse the part before A: (after .C:)
    prefix = line[3:a_pos].strip()
    suffix = line[a_pos + 3:]  # Skip " A:"

    # Split prefix into parts
    parts = prefix.split()
    if len(parts) < 2:
        return None

    addr = parts[0].upper()

    # Collect hex bytes as opcodes, rest is instruction
    opcodes = []
    instruction_parts = []
    in_instruction = False

    for part in parts[1:]:
        if not in_instruction:
            if re.match(r'^[0-9a-fA-F]{2}$', part):
                opcodes.append(part)
            else:
                in_instruction = True
                instruction_parts.append(part)
        else:
            instruction_parts.append(part)

    opcodes_str = ' '.join(opcodes)
    instruction = ' '.join(instruction_parts) if instruction_parts else ""

    # Parse suffix: xx X:xx Y:xx SP:xx flags cycle
    suffix_parts = suffix.split()
    if len(suffix_parts) < 5:
        return None

    try:
        a = int(suffix_parts[0], 16)
    except ValueError:
        return None

    x = y = sp = 0

    for part in suffix_parts[1:]:
        if part.startswith('X:'):
            x = int(part[2:], 16)
        elif part.startswith('Y:'):
            y = int(part[2:], 16)
        elif part.startswith('SP:'):
            sp = int(part[3:], 16)
        elif re.match(r'^[\.\-NVCBIDZ]{8}$', part):
            pass  # Flags ignored
        elif part.isdigit():
            cycle = int(part)

    return Instruction(
        addr=addr,
        opcodes=opcodes_str,
        mnemonic=instruction,
        a=a, x=x, y=y, sp=sp, cycle=cycle
    )


def parse_vice_line(line: str) -> Optional[Instruction]:
    """
    Parse VICE trace format:
    .ED23 191 027    3137364  78          SEI        280101f3
    .ED24 191 029    3137366  20 97 EE    JSR $EE97  280101f3
    .EE97 191 035    3137372  AD 00 DD    LDA $DD00  280101f1
    """
    if line.startswith('Drive  8:'):
        line = f"{line[9:15]} 0 0 {line[16:]}"
    if not line.startswith('.'):
        return None

    # Remove . prefix
    line = line[1:]

    # Work backwards from the end
    # Last 8 chars are packed registers
    parts = line.rsplit(' ', 1)
    if len(parts) < 2:
        return None

    packed = parts[-1]
    if len(packed) != 8:
        return None

    try:
        a = int(packed[0:2], 16)
        x = int(packed[2:4], 16)
        y = int(packed[4:6], 16)
        sp = int(packed[6:8], 16)
    except ValueError:
        return None

    # Everything before the packed regs
    rest = parts[0].strip()
    rest_parts = rest.split()

    if len(rest_parts) < 4:
        return None

    addr = rest_parts[0].upper()
    # rest_parts[1] = raster line (ignored)
    # rest_parts[2] = cycle in line (ignored)
    cycle = rest_parts[3]

    # Remaining parts: opcodes and instruction
    remaining = rest_parts[4:]

    if not remaining:
        return None

    # Collect hex bytes as opcodes, rest is instruction
    opcodes = []
    instruction_parts = []
    in_instruction = False

    for part in remaining:
        if not in_instruction:
            if re.match(r'^[0-9a-fA-F]{2}$', part):
                opcodes.append(part)
            else:
                in_instruction = True
                instruction_parts.append(part)
        else:
            instruction_parts.append(part)

    opcodes_str = ' '.join(opcodes)
    instruction = ' '.join(instruction_parts) if instruction_parts else ""

    return Instruction(
        addr=addr,
        opcodes=opcodes_str,
        mnemonic=instruction,
        a=a,
        x=x,
        y=y,
        sp=sp,
        cycle=cycle
    )


def canonicalize_mnemonic(mnemonic: str) -> str:
    """Convert mnemonic to canonical form for consistent diffing.

    Normalizes accumulator operations:
    - ASL/ASL A → ASL A
    - LSR/LSR A → LSR A
    - ROL/ROL A → ROL A
    - ROR/ROR A → ROR A
    - INC/INC A → INC A
    - DEC/DEC A → DEC A
    """
    # These are accumulator-only operations that should have " A" suffix
    acc_ops = {'ASL', 'LSR', 'ROL', 'ROR', 'INC', 'DEC', 'LDA'}

    # Extract base opcode (first word)
    parts = mnemonic.split()
    if not parts:
        return mnemonic

    base = parts[0]

    # If it's an accumulator operation with no operand, add " A"
    if base in acc_ops and len(parts) == 1:
        return f"{base} A"

    return mnemonic


def convert_to_canonical(inst: Instruction) -> str:
    """Convert an Instruction to canonical format string."""
    canon_mnemonic = canonicalize_mnemonic(inst.mnemonic)
    return f"{inst.addr}  {inst.opcodes:12}  {canon_mnemonic:13}  A:{inst.a:02X} X:{inst.x:02X} Y:{inst.y:02X} SP:{inst.sp:02X} CYC:{inst.cycle}"


def detect_format(filepath: str) -> Optional[str]:
    """Detect trace format by examining the first non-empty line."""
    with open(filepath, 'r') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            if line.startswith('.C:') or line.startswith('.8:'):
                return 'rp2'
            elif line.startswith('.') or line.startswith('Drive'):
                return 'vice'
    return None


def process_file(input_path: str, output_path: str) -> int:
    """Process a trace file and write canonical format output.

    Returns the number of instructions processed.
    """
    format_type = detect_format(input_path)
    if format_type is None:
        print(f"Error: Could not detect format of {input_path}", file=sys.stderr)
        return 0

    parser = parse_rp2_line if format_type == 'rp2' else parse_vice_line

    num_lines = 0
    with open(output_path, 'w') as fw:
        with open(input_path, 'r') as fr:
            for line in fr:
                line = line.rstrip('\n')
                inst = parser(line)
                if not inst:
                    print(f"Failed to parse line {num_lines+1}: {line}")
                    fw.write(f"# {line}\n")
                else:
                    fw.write(convert_to_canonical(inst) + "\n")
                num_lines += 1

    return num_lines


def main():
    parser = argparse.ArgumentParser(
        description="Convert C64 emulator trace files to canonical format for visual diff tools.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s rp2_trace.txt                    # Output: rp2_trace_canon.txt
  %(prog)s rp2_trace.txt vice_trace.txt     # Convert both files
  %(prog)s --output-dir /tmp/canon *.txt    # Specify output directory
        """
    )

    parser.add_argument(
        "files",
        nargs='+',
        help="Trace file(s) to convert"
    )

    parser.add_argument(
        "--output-dir", "-o",
        help="Output directory (default: same as input file)"
    )

    parser.add_argument(
        "--suffix", "-s",
        default="_canon.txt",
        help="Output suffix (default: _canon.txt)"
    )

    args = parser.parse_args()

    total_instructions = 0

    for input_path in args.files:
        # Determine output path
        if args.output_dir:
            import os
            basename = os.path.basename(input_path)
            # Remove existing extension
            if '.' in basename:
                basename = basename.rsplit('.', 1)[0]
            output_path = os.path.join(args.output_dir, basename + args.suffix)
        else:
            import os
            dirname = os.path.dirname(input_path)
            basename = os.path.basename(input_path)
            # Remove existing extension
            if '.' in basename:
                basename = basename.rsplit('.', 1)[0]
            if dirname:
                output_path = os.path.join(dirname, basename + args.suffix)
            else:
                output_path = basename + args.suffix

        count = process_file(input_path, output_path)
        total_instructions += count

        if count > 0:
            print(f"{input_path} -> {output_path} ({count} instructions)")
        else:
            print(f"{input_path} -> {output_path} (empty or error)")

    print(f"Total: {total_instructions} instructions processed")


if __name__ == "__main__":
    main()
