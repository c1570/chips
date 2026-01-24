#!/usr/bin/env python3
"""
Create a "gist" from trace logs.

Processes canonical trace output and produces simplified event logs
triggered by specific addresses, with support for deferred logging
via ENTER/LEAVE stashes.

Purpose is to gist two differing traces, then text diff the gist
to find the source/reason of the difference more easily.

Easiest to backtrack then is to use a diff tool that can ignore
the C++ style comment lines with cycle count this tools outputs
when using command line option "--annotate":
* kdiff3 --cs "IgnoreComments=1"
* meld (Preferences/Text Filters/C++ style comments)

Otherwise, take note of the line number in the gist, then recreate
the gist using "-l" to find the line number in the original trace.
"""

import argparse
import re
import sys
from typing import List, Optional, Tuple


# Address → event mappings
# Direct events: create log entry immediately
# Stash entries: (ENTER_event, LEAVE_event) - defer until LEAVE
EVENTS = {
    # Direct events
    '0718': 'TRANSWARP_READ_1800_1_ACCU',
    '071E': 'TRANSWARP_READ_1800_2_ACCU',
    '0724': 'TRANSWARP_READ_1800_3_ACCU',
    '072B': 'TRANSWARP_READ_1800_4_ACCU',
    '0754': 'TRANSWARP_READ_1C01_1_YREG',
    '0756': 'TRANSWARP_WRITE_TO_DRIVEMEM_1_ACCU',
    '0768': 'TRANSWARP_READ_1C01_2_YREG',
    '075E': 'TRANSWARP_WRITE_TO_DRIVEMEM_2_ACCU',
    '077C': 'TRANSWARP_READ_1C01_3_ACCU',
    '0770': 'TRANSWARP_WRITE_TO_DRIVEMEM_3_ACCU',
    '0776': 'TRANSWARP_WRITE_TO_DRIVEMEM_4_ACCU',
    '078B': 'TRANSWARP_READ_1C01_4_ACCU',
    '0785': 'TRANSWARP_WRITE_TO_DRIVEMEM_5_ACCU',
    '0798': 'TRANSWARP_READ_1C01_5_ACCU',
    '078C': 'TRANSWARP_WRITE_TO_DRIVEMEM_6_ACCU',
    '0792': 'TRANSWARP_WRITE_TO_DRIVEMEM_7_ACCU',
    '079A': 'TRANSWARP_WRITE_TO_DRIVEMEM_8_ACCU',
    'F363': 'EXECUTE_JOB_ACCU',
    'F502': 'CHECKSUM_FAIL',
    'F505': 'CHECKSUM_OK',
    'F54D': 'TRACK_AND_SECTOR_FOUND_FOR_READ',
    'F553': 'ERROR',
    'F556': 'WAIT_FOR_SYNC',
    'F6FB': 'DECODE_GCR_BYTE_1_ACCU',
    'F722': 'DECODE_GCR_BYTE_2_ACCU',
    'F766': 'DECODE_GCR_BYTE_3_ACCU',
    'F779': 'DECODE_GCR_BYTE_4_ACCU',
    'FE73': 'NOTE_ATNRQ_IN_IRQ',
    'FE7C': 'DISK_CTRL_TIMER_FIRED',
    'EBE7': 'IDLE',
    'EC04': 'IDLE_TO_ATNSRV',
    'EBFC': 'IDLE_TO_PARSECMD',
    'EC1D': 'IDLE_HAVE_ACTIVE_FILE',
    'EA2D': 'GOT_IEC_BYTE_ACPTR_ACCU',
    'EA2E': 'LISTEN',
    'D09B': 'STRRD_START_READING_AHEAD',
    'D0AE': 'STRRD_FINISHED_READING_AHEAD',
    'D585': 'JOB_SET_UP',
    'D586': 'DOREAD_JOB',
    'D58A': 'DOWRITE_JOB',
    'D5A5': 'WAITJOB_DONE',
    'D155': 'GETBYT_FROM_BUFFER_DONE_ACCU',
    'C160': 'OPEN_SEC_ADDR_15',
    'C191': 'JUMP_INDIR_TO_COMMAND',
    'CB1D': 'MEMORY_EXECUTE_COMMAND',
    'CB20': 'MEMORY_READ_COMMAND',
    'CB50': 'MEMORY_WRITE_COMMAND',
    'DC46': 'OPNRCH_OPEN_READ_CHANNEL',
    'DCB5': 'DONE_OPNRCH_OPEN_READ_CHANNEL',
    'D7CB': 'LOADLASTPROGRAM',
    'D7F7': 'LOADDIR',
    'D819': 'OPENBLK',
    'D82B': 'UNKNOWN_TODOXXX',
    'E85B': 'SERVICE_ATN',

    # Stash entries: (event_at_this_addr, matching_leave_event)
    'FE67': ('ENTER_SYSIRQ', 'LEAVE_SYSIRQ'),
    'FE84': ('LEAVE_SYSIRQ', None),
    'E95A': ('ENTER_START_SEND_IEC_BYTE', 'LEAVE_START_SEND_IEC_BYTE_ACCU'),
    'E968': ('LEAVE_START_SEND_IEC_BYTE_ACCU', None),
}


def extract_pc_cycle(line: str) -> Tuple[Optional[str], str]:
    """
    Extract program counter address from a canonical format line.

    Returns addr or None if line doesn't match format.
    Format: ADDR  OPCODES  MNEMONIC  A:XX X:XX Y:XX SP:XX CYC:XX
    """
    if line.startswith("#"):
        return None, 0

    parts = line.split()
    if len(parts) < 3:
        return None, 0

    addr = parts[0]
    if not re.match(r'^[0-9A-F]{4}$', addr):
        return None, 0

    cycle = parts[-1][4:] if parts[-1].startswith("CYC:") else "0"

    return addr, cycle


class StashEntry:
    """Represents a stash context with collected formatted events."""
    def __init__(self, enter_event: str, start_line: int, cycle: str, matching_leave: str):
        self.enter_event = enter_event
        self.start_line = start_line
        self.cycle = cycle
        self.matching_leave = matching_leave
        self.events: List[str] = []  # List of formatted inner event strings


def format_event(event: str, addr: str, cycle: int, line_number: int, show_line: bool, show_cycle: bool, line: str = None) -> str:
    """Format an event with address and optional line count.

    For events ending in _ACCU/_YREG/_XREG, also extracts and displays the register value.
    """
    prefix = None
    if event.endswith("_ACCU"):
        prefix = "A:"
    elif event.endswith("_YREG"):
        prefix = "Y:"
    elif event.endswith("_XREG"):
        prefix = "X:"
    if not prefix:
        result = f"{event} ({addr})"
    else:
        parts = line.split()
        for part in parts:
            if part.startswith(prefix):
                reg = part[2:]
                result = f"{event}:0x{reg} ({addr})"
    if show_line:
        result += f" line:{line_number}"
    if show_cycle:
        result += f" cycle:{cycle}"
    return result


def process_trace(input_path: str, show_line: bool = False, show_cycle: bool = False, use_comments: bool = False, gap_threshold: int = None):
    """
    Process a canonical trace file and generate gist output.
    """
    # Read all lines to get line counts if needed
    with open(input_path, 'r') as f:
        lines = f.readlines()

    # Track line counts if needed
    line_counter = 0
    instructions_since_last_event = 0

    # Stash stack for nested stashes
    stash_stack: List[StashEntry] = []

    # Output lines
    output: List[str] = []

    for line_num, line in enumerate(lines):
        line = line.rstrip('\n')
        if not line:
            continue

        line_counter += 1
        addr, cycle = extract_pc_cycle(line)
        if addr is None:
            continue

        instructions_since_last_event += 1

        # Check if this address triggers an event
        if addr in EVENTS:
            instructions_since_last_event = 0  # Reset counter on event
            event_def = EVENTS[addr]
            if isinstance(event_def, tuple):
                event_str = format_event(event_def[0], addr, cycle, line_counter, show_line, show_cycle, line)
                if not event_def[1] and stash_stack and stash_stack[-1].matching_leave == event_def[0]:
                    # this is the leave event to the newest stash entry, close it
                    frame = stash_stack.pop()
                    frame.events.append(event_str)
                    if use_comments:
                        output.append(f"// cycle {frame.cycle}")
                    output.append(", ".join(frame.events))
                elif event_def[1] and (not stash_stack or stash_stack[-1].enter_event != event_def[0]):
                    # open new stash
                    frame = StashEntry(event_def[0], line_counter, cycle, event_def[1])
                    frame.events.append(event_str)
                    stash_stack.append(frame)
                continue
            else:
                # log immediately if not in a stash
                event_name = event_def
                event_str = format_event(event_name, addr, cycle, line_counter, show_line, show_cycle, line)
                if stash_stack:
                    # Inside a stash - format and collect the event
                    stash_stack[-1].events.append(event_str)
                else:
                    # Not in a stash - output directly
                    if use_comments:
                        output.append(f"// cycle {cycle}")
                    output.append(event_str)
        elif gap_threshold is not None and instructions_since_last_event >= gap_threshold:
            # Gap threshold reached - insert marker (always show line number)
            output.append(f"// ({gap_threshold} instructions with no events) line:{line_counter} cycle:{cycle}")
            instructions_since_last_event = 0

    if stash_stack:
        output.append(f"\n\n!!! Unresolved stack entries!")
        for frame in stash_stack:
            output.append(", ".join(frame.events))
    return output


def main():
    parser = argparse.ArgumentParser(
        description="Create a gist from trace logs.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s rp2_trace_cpu_canon.txt
  %(prog)s --lines rp2_trace_cpu_canon.txt
  %(prog)s gist.txt > output.txt
        """
    )

    parser.add_argument(
        "files",
        nargs='+',
        help="Canonical trace file(s) to process"
    )

    parser.add_argument(
        "--lines", "-l",
        action="store_true",
        help="Include line counts in output"
    )

    parser.add_argument(
        "--cycles", "-c",
        action="store_true",
        help="Include cycles in output"
    )

    parser.add_argument(
        "--annotate", "-a",
        action="store_true",
        help="Use C++ style comments to annotate events with cycles - configure your diff tool to ignore those lines"
    )

    parser.add_argument(
        "--gap", "-g",
        type=int,
        metavar="N",
        help="Insert marker after N instructions with no events"
    )

    args = parser.parse_args()

    for input_path in args.files:
        try:
            output_lines = process_trace(input_path, show_line=args.lines, show_cycle=args.cycles, use_comments=args.annotate, gap_threshold=args.gap)

            for line in output_lines:
                print(line)

        except FileNotFoundError:
            print(f"Error: File not found: {input_path}", file=sys.stderr)
            sys.exit(1)

if __name__ == "__main__":
    main()
