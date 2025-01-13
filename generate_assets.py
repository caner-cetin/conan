#!/usr/bin/env python3
import base64
import zlib


def generate_compressed_header(input_file: str, var_name: str):
    with open(input_file, "rb") as f:
        content = f.read()

    # Compress with zlib at maximum compression level
    compressed_data = zlib.compress(content, level=9)

    # Convert to base64
    b64_data = base64.b64encode(compressed_data).decode("ascii")

    # Calculate actual compressed data size (before base64)
    actual_compressed_size = len(compressed_data)

    # Generate header guard
    header = f"""#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtrigraphs"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtrigraphs"
// clang-format off
#ifndef {var_name.upper()}_H
#define {var_name.upper()}_H

#include "conan_util.h"

namespace Resources {{
    namespace {var_name} {{
        const char compressed_data[] = "{b64_data}";
        const unsigned int base64_size = {len(b64_data)};
        const unsigned int compressed_size = {actual_compressed_size};  // Size of data after base64 decode
        const unsigned int original_size = {len(content)};
        
        inline std::function<std::vector<unsigned char>()> decompress = decompress_base64_encoded_zlib_compressed_data(compressed_data, base64_size, compressed_size, original_size);

    }}
}}

#endif
// clang-format on
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
"""
    return header


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    parser.add_argument("classname")
    args = parser.parse_args()

    print(generate_compressed_header(args.filename, args.classname))
