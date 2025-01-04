#!/usr/bin/env python3


def generate_header(input_file: str, var_name: str):
    with open(input_file, "rb") as f:
        content = f.read()

    # Generate header guard
    header = f"""#ifndef {var_name.upper()}_H
#define {var_name.upper()}_H

namespace Resources {{
    namespace {var_name}{{
        const unsigned char hex[] = {{
"""

    # Convert binary data to hex format
    hex_data = [f"0x{byte:02x}" for byte in content]

    # Format data in rows of 12 numbers
    rows = [hex_data[i : i + 12] for i in range(0, len(hex_data), 12)]
    formatted_data = ",\n        ".join(", ".join(row) for row in rows)

    # Add data and size variable
    header += f"        {formatted_data}\n    }};\n\n"
    header += f"    const unsigned int size = {len(content)};\n}}}}\n\n"
    header += "#endif\n"

    return header


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 3:
        print("Usage: ./resource_converter.py input_file var_name")
        sys.exit(1)

    print(generate_header(sys.argv[1], sys.argv[2]))
