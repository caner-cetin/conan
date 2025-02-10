#!/usr/bin/env python3
import argparse
import base64
import os
import zlib


def generate_compressed_files(input_file: str, var_name: str, header_path: str):
    # Read the input file
    with open(input_file, "rb") as f:
        content = f.read()

    # Compress with zlib at maximum compression level
    compressed_data = zlib.compress(content, level=9)

    # Convert to base64
    b64_data = base64.b64encode(compressed_data).decode("ascii")

    # Calculate sizes
    actual_compressed_size = len(compressed_data)
    original_size = len(content)

    # Extract the base filename (without extension) for use in the header guard
    base_filename = os.path.splitext(os.path.basename(header_path))[0]

    # Generate the header file
    header_guard = f"{base_filename.upper()}_H"
    header_content = f"""#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtrigraphs"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtrigraphs"    
#ifndef {header_guard}
#define {header_guard}

#include <vector>
#include <functional>
#include "conan_util.h"

namespace Resources {{
    namespace {var_name} {{
        // Base64-encoded compressed data (stored in the corresponding .cpp file)
        extern const char compressed_data[];
        extern const unsigned int base64_size;
        extern const unsigned int compressed_size;
        extern const unsigned int original_size;

        inline std::function<std::vector<unsigned char>()> decompress = decompress_base64_encoded_zlib_compressed_data(compressed_data, base64_size, compressed_size, original_size);
    }}
}}

#endif
"""

    # Generate the source file
    source_content = f"""#include "{os.path.basename(header_path)}"

namespace Resources {{
    namespace {var_name} {{
        // Base64-encoded compressed data
        const char compressed_data[] = "{b64_data}";

        // Metadata
        const unsigned int base64_size = {len(b64_data)};
        const unsigned int compressed_size = {actual_compressed_size};
        const unsigned int original_size = {original_size};
    }}
}}
// clang-format on
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
"""

    # Write the header file
    with open(header_path, "w") as f:
        f.write(header_content)

    # Write the source file (same directory as the header, with .cpp extension)
    source_path = os.path.splitext(header_path)[0] + ".cpp"
    with open(source_path, "w") as f:
        f.write(source_content)

    print(f"Generated: {header_path} and {source_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate embedded asset files.")
    parser.add_argument(
        "input_file", help="Path to the input file (e.g., assets/no_cover_art.gif)"
    )
    parser.add_argument(
        "var_name",
        help="Namespace and variable name for the asset (e.g., NoCoverArtGif)",
    )
    parser.add_argument(
        "header_path",
        help="Path to the output header file (e.g., ${INCLUDE_FOLDER}/no_cover_art.h)",
    )
    args = parser.parse_args()

    generate_compressed_files(args.input_file, args.var_name, args.header_path)
