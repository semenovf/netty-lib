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
#pragma once
#include "exports.hpp"
#include <functional>
#include <string>
#include <utility>

namespace netty {

/**
 * @brief Satisfies Concepts:
 *        @li CopyConstructible
 *        @li CopyAssignable
 */
class inet4_addr
{
public:
    static constexpr std::uint32_t broadcast_addr_value = 0xFFFFFFFF;
    static constexpr std::uint32_t any_addr_value       = 0x00000000;

private:
    std::uint32_t _addr {0};

public:
    /**
     * @brief Constructs invalid @c inet4_addr instance.
     */
    inet4_addr () = default;

    /**
     * @brief Copy constructor.
     */
    inet4_addr (inet4_addr const & x) = default;

    /**
     * @brief Move constructor.
     */
    inet4_addr (inet4_addr && x) = default;

    /**
     * @brief Copy assignment operator.
     */
    inet4_addr & operator = (inet4_addr const & x) = default;

    /**
     * @brief Move assignment operator.
     */
    inet4_addr & operator = (inet4_addr && x) = default;

    /**
     * @brief Constructs inet4_addr from four numeric parts.
     *
     * @details Each of the four numeric parts specifies a byte of the address;
     *          the bytes are assigned in left-to-right order to produce the binary address.
     *
     * @param a First numeric part.
     * @param b Second numeric part.
     * @param c Third numeric part.
     * @param d Fourth numeric part.
     */
    inet4_addr (std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d)
        : _addr(0)
    {
        _addr |= (static_cast<uint32_t>(a) << 24);
        _addr |= (static_cast<uint32_t>(b) << 16);
        _addr |= (static_cast<uint32_t>(c) << 8);
        _addr |= static_cast<uint32_t>(d);
    }

    /**
     * @brief Constructs inet4_addr from three numeric parts.
     *
     * @details Parts @a a and @a b specify the first two bytes of the binary address.
     *          Part @a c is interpreted as a 16-bit value that defines the rightmost
     *          two bytes of the binary address. This notation is suitable
     *          for specifying (outmoded) Class B network addresses.
     *
     * @param a First numeric part
     * @param b Second numeric part
     * @param c Third numeric parts.
     */
    inet4_addr (std::uint8_t a, std::uint8_t b, std::uint16_t c)
        : _addr(0)
    {
        _addr |= (static_cast<uint32_t>(a) << 24);
        _addr |= (static_cast<uint32_t>(b) << 16);
        _addr |= static_cast<uint32_t>(c);
    }

    /**
     * @brief Constructs inet4_addr from two numeric parts.
     *
     * @details Part @a a specifies the first byte of the binary address.
     *          Part @a b is interpreted as a 24-bit value that defines
     *          the rightmost three bytes of the binary address.
     *          This notation is suitable for specifying (outmoded)
     *          Class C network addresses.
     *
     * @param a First numeric part
     * @param b Second numeric part
     */
    inet4_addr (std::uint8_t a, std::uint32_t b)
        : _addr(0)
    {
        _addr |= (static_cast<uint32_t>(a) << 24);
        _addr |= b;
    }

    /**
     * @brief Constructs inet4_addr from one numeric part.
     *
     * @param a Numeric part.
     */
    inet4_addr (std::uint32_t a) : _addr(a)
    {}

    inet4_addr & operator = (std::uint32_t a)
    {
        _addr = a;
        return *this;
    }

    explicit operator std::uint32_t () const noexcept
    {
        return _addr;
    }

public: // static
    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, inet4_addr> parse (char const * s, std::size_t n);

    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, inet4_addr> parse (char const * s);

    /**
     * Parses IPv4 address from string.
     */
    static std::pair<bool, inet4_addr> parse (std::string const & s);
};

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
NETTY__EXPORT std::string to_string (inet4_addr const & addr
    , std::string const & format, int base);

inline bool operator == (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) == static_cast<std::uint32_t>(b);
}

inline bool operator != (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) != static_cast<std::uint32_t>(b);
}

inline bool operator < (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) < static_cast<std::uint32_t>(b);
}

inline bool operator > (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) > static_cast<std::uint32_t>(b);
}

inline bool operator <= (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) <= static_cast<std::uint32_t>(b);
}

inline bool operator >= (inet4_addr const & a, inet4_addr const & b)
{
    return static_cast<std::uint32_t>(a) >= static_cast<std::uint32_t>(b);
}

/**
 * Checks if @a addr is loopback.
 */
inline bool is_loopback (inet4_addr const & addr)
{
    return addr == inet4_addr{127, 0, 0, 1};
}

// https://en.wikipedia.org/wiki/Multicast_address
inline bool is_multicast (inet4_addr const & addr)
{
    return addr >= inet4_addr{224, 0, 0, 0}
        && addr <= inet4_addr{239, 255, 255, 255};
}

/**
 * Checks if @a addr is not multicast and last octet equals to @c 255.
 */
inline bool is_broadcast (inet4_addr const & addr)
{
    return !is_multicast(addr)
        && ((static_cast<std::uint32_t>(addr) & 0x000000FF) == 0x000000FF);
}

/**
 * @brief Converts IPv4 address to string with format "%a.%b.%c.%d" and base 10.
 *
 * @param addr Source address for conversion.
 * @return String representation of IPv4 address. If @a addr is not valid
 *         result will be an empty string.
 */
inline std::string to_string (inet4_addr const & addr)
{
    return to_string(addr, std::string{}, 10);
}

} // netty
