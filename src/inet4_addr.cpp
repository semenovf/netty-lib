////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2021 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// References:
//
// Changelog:
//      2017.07.03 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "pfs/integer.hpp"
#include "pfs/netty/inet4_addr.hpp"
#include <cassert>
#include <regex>
#include <utility>

namespace netty {

static std::string const DEFAULT_FORMAT = "%a.%b.%c.%d";

static void append_number_prefix (std::string & r, std::string const & a, int base)
{
    if (base == 16) {
        size_t len = a.length();
        r.append("0x");

        if (len == 1)
            r.push_back('0');
    } else if (base == 8) {
        size_t len = a.length();
        r.append("0");

        if (len < 3)
            r.push_back('0');

        if (len < 2)
            r.push_back('0');
    }
}

void append_number_prefix (std::string & r, int base)
{
    if (base == 16)
        r.append("0x");
    else if (base == 8)
        r.append("0");
}

static std::string uintmax_to_string (std::uintmax_t num, int radix, bool uppercase)
{
    using char_type = std::string::value_type;

    static const char_type digits_lower[] = {
          '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
        , 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'
        , 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't'
        , 'u', 'v', 'w', 'x', 'y', 'z'
    };

    static const char_type digits_upper[] = {
          '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
        , 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'
        , 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'
        , 'U', 'V', 'W', 'X', 'Y', 'Z'
    };

    assert(radix >= 2 && radix <= 36);

    char_type const * digits = uppercase ? digits_upper : digits_lower;
    std::string result;

    if (num) {
        while (num > 0) {
            result.insert(result.begin(), digits[num % radix]);
            num /= radix;
        }
    } else { /* num == 0 */
        result = "0";
    }

    return result;
}

/**
* @brief Converts IPv4 address to string.
*
* @details The format specification is a null-terminated string and may
*      contain special character sequences called conversion specifications,
*      each of which is introduced by a '%' character and terminated by
*      some other character known as a conversion specifier character.
*      All other character sequences are ordinary character sequences.
*
*      The characters of ordinary character sequences (including the null byte)
*      are copied verbatim from format to resulting string. However,
*      the characters of conversion specifications are replaced as follows:
*
*      @li %a First byte of IPv4 address.
*      @li %A The full 32-bit representation of IPv4 address.
*      @li %b Second byte of IPv4 address.
*      @li %B 24-bit representation of last part of IPv4 address
*      @li %c Third byte of IPv4 address.
*      @li %C 16-bit representation of last part of IPv4 address
*      @li %d Fourth byte of IPv4 address.
*      @li %% '%' character.
*
* @param addr Source address for conversion.
* @param format Conversion format string. If @a format is empty the default format
*      will be used: "%a.%b.%c.%d".
* @param base Specifies the radix (8, 10 or 16) for output address parts.
* @return String representation of IPv4 address. If @a addr is not valid
*      result will be an empty string.
*
* @note 192.0.2.235           decimal
*       0xC0.0x00.0x02.0xEB   hex
*       0300.0000.0002.0353   octal
*       0xC00002EB            decimal
*       3221226219            hex
*       030000001353          octal
*
*
*/
std::string to_string (inet4_addr const & addr, std::string const & format, int base)
{
    using char_type = std::string::value_type;

    std::string r;
    std::string const * f = nullptr;

    if (format.empty()) {
        f = & DEFAULT_FORMAT;
    } else {
        f = & format;
    }

    if (!(base == 10 || base == 8 || base == 16))
        base = 10;

    std::string::const_iterator it = f->cbegin();
    std::string::const_iterator it_end = f->cend();

    bool uppercase = true;
    auto value = static_cast<std::uint32_t>(addr);

    while (it != it_end) {
        if (*it == char_type('%')) {
            ++it;

            if (it == it_end) {
                r.push_back(char_type('%'));
                break;
            }

            if (*it == char_type('a')) {
                auto a = uintmax_to_string(static_cast<uint8_t>(0x000000FF & (value >> 24)), base, uppercase);
                append_number_prefix(r, a, base);
                r.append(a);
            } else if (*it == char_type('b')) {
                auto b = uintmax_to_string(static_cast<uint8_t>(0x000000FF & (value >> 16)), base, uppercase);
                append_number_prefix(r, b, base);
                r.append(b);
            } else if (*it == char_type('c')) {
                auto c = uintmax_to_string(static_cast<uint8_t>(0x000000FF & (value >> 8)), base, uppercase);
                append_number_prefix(r, c, base);
                r.append(c);
            } else if (*it == char_type('d')) {
                auto d = uintmax_to_string(static_cast<uint8_t>(0x000000FF & value), base, uppercase);
                append_number_prefix(r, d, base);
                r.append(d);
            } else if (*it == char_type('A')) {
                auto A = uintmax_to_string(value, base, uppercase);
                append_number_prefix(r, base);
                r.append(A);
            } else if (*it == char_type('B')) {
                auto B = uintmax_to_string(0x00FFFFFF & value, base, uppercase);
                append_number_prefix(r, base);
                r.append(B);
            } else if (*it == char_type('C')) {
                auto C = uintmax_to_string(0x0000FFFF & value, base, uppercase);
                append_number_prefix(r, base);
                r.append(C);
            } else {
                r.push_back(*it);
            }
        } else {
            r.push_back(*it);
        }

        ++it;
    }

    return r;
}

std::pair<bool, inet4_addr> inet4_addr::parse (char const * s, std::size_t n)
{
    std::regex rx(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
    std::cmatch m;

    if (!std::regex_match(s, s + n, m, rx))
        return std::make_pair(false, inet4_addr{});

    auto count = m.size();

    if (count != 5)
        return std::make_pair(false, inet4_addr{});

    std::error_code ec;

    auto a = pfs::to_integer(m[1].first, m[1].second, std::uint8_t{0}, std::uint8_t{255}, ec);

    if (ec)
        return std::make_pair(false, inet4_addr{});

    auto b = pfs::to_integer(m[2].first, m[2].second, std::uint8_t{0}, std::uint8_t{255}, ec);

    if (ec)
        return std::make_pair(false, inet4_addr{});

    auto c = pfs::to_integer(m[3].first, m[3].second, std::uint8_t{0}, std::uint8_t{255}, ec);

    if (ec)
        return std::make_pair(false, inet4_addr{});

    auto d = pfs::to_integer(m[4].first, m[4].second, std::uint8_t{0}, std::uint8_t{255}, ec);

    if (ec)
        return std::make_pair(false, inet4_addr{});

    return std::make_pair(true, inet4_addr{a, b, c, d});
}

std::pair<bool, inet4_addr> inet4_addr::parse (char const * s)
{
    return parse(s, std::char_traits<char>::length(s));
}

std::pair<bool, inet4_addr> inet4_addr::parse (std::string const & s)
{
    return parse(s.c_str(), s.size());
}

} // namespace netty
