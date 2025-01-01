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
  std::string TestBase::hex_to_bin(const std::string& hex) {
    std::string bin;
    for (const char c : hex) {
      switch (c) {
      case '0': bin += "0000";
        break;
      case '1': bin += "0001";
        break;
      case '2': bin += "0010";
        break;
      case '3': bin += "0011";
        break;
      case '4': bin += "0100";
        break;
      case '5': bin += "0101";
        break;
      case '6': bin += "0110";
        break;
      case '7': bin += "0111";
        break;
      case '8': bin += "1000";
        break;
      case '9': bin += "1001";
        break;
      case 'A':
      case 'a': bin += "1010";
        break;
      case 'B':
      case 'b': bin += "1011";
        break;
      case 'C':
      case 'c': bin += "1100";
        break;
      case 'D':
      case 'd': bin += "1101";
        break;
      case 'E':
      case 'e': bin += "1110";
        break;
      case 'F':
      case 'f': bin += "1111";
        break;
      case 'X':
      case 'x': bin += "xxxx";
        break;
      case 'Z':
      case 'z': bin += "zzzz";
        break;
      default:
        throw std::invalid_argument("Invalid hex character");
      }
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
  char TestBase::bin_to_hex_char(const std::string& bin) {
    if (bin == "0000") return '0';
    if (bin == "0001") return '1';
    if (bin == "0010") return '2';
    if (bin == "0011") return '3';
    if (bin == "0100") return '4';
    if (bin == "0101") return '5';
    if (bin == "0110") return '6';
    if (bin == "0111") return '7';
    if (bin == "1000") return '8';
    if (bin == "1001") return '9';
    if (bin == "1010") return 'A';
    if (bin == "1011") return 'B';
    if (bin == "1100") return 'C';
    if (bin == "1101") return 'D';
    if (bin == "1110") return 'E';
    if (bin == "1111") return 'F';
    if (bin == "xxxx") return 'X';
    if (bin == "zzzz") return 'Z';
    throw std::invalid_argument("[WARNING]\tInvalid binary quartet");
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
  std::string TestBase::bin_to_hex(const std::string& bin) {
    std::string padded_bin = bin;

    // Calculate needed padding
    if (const size_t remainder = bin.size() % 4; remainder != 0) {
      padded_bin.insert(0, 4 - remainder, '0');
    }

    std::string hex;
    for (size_t i = 0; i < padded_bin.size(); i += 4) {
      hex += bin_to_hex_char(padded_bin.substr(i, 4));
    }

    // Trim leading zeros in the hex string
    while (hex.size() > 1 && hex.front() == '0') {
      hex.erase(0, 1);
    }

    return hex;
  }
}
