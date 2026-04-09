#!/usr/bin/env python3
"""Split raw string literals in EmbeddedAssets.hpp for MSVC compatibility.

MSVC has a 16380-character limit per string literal (C2026). This script
splits large R"delim(...)delim" blocks into adjacent raw string literals
that get concatenated at compile time, keeping each piece under the limit.
"""

import re
import sys

MAX_CHARS = 12000  # conservative: well under MSVC's 16380 limit

def process(path):
    with open(path, 'r') as f:
        text = f.read()

    lines = text.split('\n')
    output = []
    in_raw = False
    delim = ''
    chars_accum = 0
    split_count = 0

    for line in lines:
        if not in_raw:
            m = re.search(r'R"(\w*)\(', line)
            if m:
                delim = m.group(1)
                close = f'){delim}"'
                after = line[m.end():]
                if close in after:
                    output.append(line)
                else:
                    in_raw = True
                    chars_accum = len(line) - m.start()
                    output.append(line)
            else:
                output.append(line)
        else:
            close = f'){delim}"'
            if close in line:
                output.append(line)
                in_raw = False
                chars_accum = 0
            else:
                chars_accum += len(line) + 1
                if chars_accum >= MAX_CHARS:
                    # Append split marker to end of this line (no extra newline)
                    output.append(line + f'){delim}" R"{delim}(')
                    chars_accum = 0
                    split_count += 1
                else:
                    output.append(line)

    result = '\n'.join(output)

    with open(path, 'w') as f:
        f.write(result)

    print(f"Processed {path}: {len(lines)} original lines, {split_count} splits inserted")

if __name__ == '__main__':
    target = sys.argv[1] if len(sys.argv) > 1 else 'src/server/EmbeddedAssets.hpp'
    process(target)
