// MIT License
//
// Copyright (c) 2024 Rovshan Rustamov
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "testbase.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
    using BinLut = std::array<const char *, 256>;

    const BinLut &get_hex_to_bin_lut() {
        static const BinLut lut = [] {
            BinLut t{};
            t.fill(nullptr);

            t[static_cast<unsigned char>('0')] = "0000";
            t[static_cast<unsigned char>('1')] = "0001";
            t[static_cast<unsigned char>('2')] = "0010";
            t[static_cast<unsigned char>('3')] = "0011";
            t[static_cast<unsigned char>('4')] = "0100";
            t[static_cast<unsigned char>('5')] = "0101";
            t[static_cast<unsigned char>('6')] = "0110";
            t[static_cast<unsigned char>('7')] = "0111";
            t[static_cast<unsigned char>('8')] = "1000";
            t[static_cast<unsigned char>('9')] = "1001";
            t[static_cast<unsigned char>('A')] = "1010";
            t[static_cast<unsigned char>('a')] = "1010";
            t[static_cast<unsigned char>('B')] = "1011";
            t[static_cast<unsigned char>('b')] = "1011";
            t[static_cast<unsigned char>('C')] = "1100";
            t[static_cast<unsigned char>('c')] = "1100";
            t[static_cast<unsigned char>('D')] = "1101";
            t[static_cast<unsigned char>('d')] = "1101";
            t[static_cast<unsigned char>('E')] = "1110";
            t[static_cast<unsigned char>('e')] = "1110";
            t[static_cast<unsigned char>('F')] = "1111";
            t[static_cast<unsigned char>('f')] = "1111";
            t[static_cast<unsigned char>('X')] = "xxxx";
            t[static_cast<unsigned char>('x')] = "xxxx";
            t[static_cast<unsigned char>('Z')] = "zzzz";
            t[static_cast<unsigned char>('z')] = "zzzz";

            return t;
        }();

        return lut;
    }

    constexpr std::array<char, 16> kNibbleToHex = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
    };

    [[nodiscard]] bool is_all_char(const char *q, const char value) noexcept {
        return q[0] == value && q[1] == value && q[2] == value && q[3] == value;
    }

    [[nodiscard]] char quartet_to_hex_char(const char *q) {
        bool has_x = false;
        bool has_z = false;
        std::uint8_t nibble = 0;

        for (int i = 0; i < 4; ++i) {
            nibble <<= 1;
            switch (q[i]) {
                case '0':
                    break;
                case '1':
                    nibble |= 1u;
                    break;
                case 'x':
                case 'X':
                    has_x = true;
                    break;
                case 'z':
                case 'Z':
                    has_z = true;
                    break;
                default:
                    throw std::invalid_argument("[WARNING]\tInvalid binary quartet");
            }
        }

        if (!has_x && !has_z) {
            return kNibbleToHex[nibble];
        }

        if (is_all_char(q, 'x') || is_all_char(q, 'X')) {
            return 'X';
        }

        if (is_all_char(q, 'z') || is_all_char(q, 'Z')) {
            return 'Z';
        }

        // Mixed-state nibble policy: when a single hex digit cannot exactly preserve
        // the 4-state composition of the 4-bit quartet, collapse to X rather than
        // throwing. This extends behavior while preserving exact handling for xxxx/zzzz.
        return 'X';
    }

    [[nodiscard]] std::string bin_to_hex_impl(std::string_view bin,
                                              const bool trim_leading_zeros) {
        if (bin.empty()) {
            return "0";
        }

        const std::size_t total_quartets = (bin.size() + 3u) / 4u;
        std::string hex;
        hex.reserve(total_quartets);

        std::size_t idx = 0;
        const std::size_t remainder = bin.size() % 4u;
        if (remainder != 0u) {
            char q[4] = {'0', '0', '0', '0'};
            const std::size_t start = 4u - remainder;
            for (std::size_t i = 0; i < remainder; ++i) {
                q[start + i] = bin[i];
            }
            hex.push_back(quartet_to_hex_char(q));
            idx = remainder;
        }

        for (; idx < bin.size(); idx += 4u) {
            char q[4] = {bin[idx], bin[idx + 1u], bin[idx + 2u], bin[idx + 3u]};
            hex.push_back(quartet_to_hex_char(q));
        }

        if (!trim_leading_zeros) {
            return hex;
        }

        const std::size_t first_non_zero = hex.find_first_not_of('0');
        if (first_non_zero == std::string::npos) {
            return "0";
        }

        if (first_non_zero == 0u) {
            return hex;
        }

        return hex.substr(first_non_zero);
    }

    // Ready for a future awaitwrite.cpp fast path where hex can be packed straight
    // into s_vpi_vecval without first materializing a binary string.
    [[maybe_unused]] [[nodiscard]] std::pair<std::uint8_t, std::uint8_t>
    hex_char_to_aval_bval_nibble(const unsigned char c) {
        switch (c) {
            case '0': return {0x0u, 0x0u};
            case '1': return {0x1u, 0x0u};
            case '2': return {0x2u, 0x0u};
            case '3': return {0x3u, 0x0u};
            case '4': return {0x4u, 0x0u};
            case '5': return {0x5u, 0x0u};
            case '6': return {0x6u, 0x0u};
            case '7': return {0x7u, 0x0u};
            case '8': return {0x8u, 0x0u};
            case '9': return {0x9u, 0x0u};
            case 'A':
            case 'a': return {0xAu, 0x0u};
            case 'B':
            case 'b': return {0xBu, 0x0u};
            case 'C':
            case 'c': return {0xCu, 0x0u};
            case 'D':
            case 'd': return {0xDu, 0x0u};
            case 'E':
            case 'e': return {0xEu, 0x0u};
            case 'F':
            case 'f': return {0xFu, 0x0u};
            case 'X':
            case 'x': return {0xFu, 0xFu};
            case 'Z':
            case 'z': return {0x0u, 0xFu};
            default:
                throw std::invalid_argument("Invalid hex character");
        }
    }
} // namespace

namespace test {
    /**
     * @brief Converts a hexadecimal string to a binary string.
     *
     * This function takes a hexadecimal string as input, converts each hex digit to its
     * corresponding 4-bit binary representation, and returns the resultant binary string.
     * It also test_handles special characters 'X' and 'Z' by representing them as "xxxx" and "zzzz" respectively.
     *
     * @param hex The hexadecimal string to be converted.
     * @return The resulting binary string.
     * @throws std::invalid_argument If the input string contains invalid hexadecimal characters.
     */
    std::string TestBase::hex_to_bin(const std::string &hex) {
        const auto &lut = get_hex_to_bin_lut();

        std::string bin;
        bin.reserve(hex.size() * 4u);

        for (const unsigned char c: hex) {
            const char *mapped = lut[c];
            if (mapped == nullptr) {
                throw std::invalid_argument("Invalid hex character");
            }
            bin.append(mapped, 4u);
        }

        return bin;
    }


    /**
     * @brief Converts a binary string representing a 4-bit value to its hexadecimal character.
     *
     * This function accepts a binary string of length 4 and returns the corresponding hexadecimal
     * character. If the input string represents an invalid binary quartet, an exception is thrown.
     *
     * @param bin A string representing a 4-bit binary value.
     * @return The hexadecimal character corresponding to the given binary value.
     * @throws std::invalid_argument If the input string is not a valid binary quartet.
     */
    char TestBase::bin_to_hex_char(const std::string &bin) {
        if (bin.size() != 4u) {
            throw std::invalid_argument("[WARNING]\tInvalid binary quartet");
        }

        return quartet_to_hex_char(bin.data());
    }

    /**
     * @brief Converts a binary string to a hexadecimal string.
     *
     * This function takes a binary string as input, pads it to ensure its length
     * is a multiple of 4, and then converts each 4-bit segment to the corresponding
     * hexadecimal character. Any leading zeros in the resulting hexadecimal string
     * are trimmed.
     *
     * @param bin The binary string to be converted.
     * @return The resulting hexadecimal string.
     */
    std::string TestBase::bin_to_hex(const std::string &bin) {
        return bin_to_hex_impl(bin, true);
    }
} // namespace test
