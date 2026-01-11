////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.11.19 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <pfs/assert.hpp>
#include <pfs/i18n.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

NETTY__NAMESPACE_BEGIN

template <typename Container = std::vector<char>>
struct container_traits
{
    using container_type = Container;

    static char const * data (container_type const & c);
    static std::size_t size (container_type const & c);
    static void append (container_type & c, char const * data, std::size_t n);
    static void clear (container_type & c);
    static void erase (container_type & c, std::size_t pos, std::size_t n);
    static void resize (container_type & c, std::size_t n);
    static void copy (container_type & c, char const * data, std::size_t n, std::size_t pos);
};

/**
 * @details
 * Container requirements:
 *      - must satisfy the requirements of ContiguousContainer
 *      - default constructable
 *      - move constructable
 *      - equality operator (for test purposes only)
 *
 * This class implemented a lightweight erase from front method.
 */
template <typename Container = std::vector<char>>
class archive
{
    using traits_type = container_traits<Container>;

public:
    using container_type = typename traits_type::container_type;

private:
    container_type _c;
    std::size_t _offset {0};

public:
    archive () = default;

    archive (char const * data, std::size_t n)
    {
        append(data, n);
    }

    archive (container_type && c) noexcept
        : _c(std::move(c))
    {}

    archive (archive && other) noexcept
        : _c(std::move(other._c))
        , _offset(other._offset)
    {
        other._offset = 0;
    }

    archive & operator = (archive && other) noexcept
    {
        if (this != & other) {
            _c = std::move(other._c);
            _offset = other._offset;
            other._offset = 0;
        }

        return *this;
    }

    archive (archive const & other)
        : archive(other.data(), other.size())
    {}

    archive & operator = (archive const &) = delete;

    ~archive ()  = default;

public:
    container_type && move_container () &
    {
        traits_type::erase(_c, 0, _offset);
        _offset = 0;
        return std::move(_c);
    }

    // For test purposes for now
    bool operator == (archive const & other) const noexcept
    {
        if (size() != other.size())
            return false;

        return std::memcmp(data(), other.data(), size()) == 0;
    }

    // For test purposes for now
    bool operator != (archive const & other) const noexcept
    {
        return !operator == (other);
    }

    /**
     * @return @c nullptr on empty.
     */
    char const * data () const noexcept
    {
        return traits_type::data(_c) + _offset;
    }

    bool empty () const noexcept
    {
        return size() == 0;
    }

    std::size_t size () const noexcept
    {
        return traits_type::size(_c) - _offset;
    }

    void append (archive const & ar)
    {
        traits_type::append(_c, ar.data(), ar.size());
    }

    void append (container_type const & c)
    {
        traits_type::append(_c, traits_type::data(c), traits_type::size(c));
    }

    void append (char const * data, std::size_t n)
    {
        traits_type::append(_c, data, n);
    }

    void append (char ch)
    {
        traits_type::append(_c, & ch, 1);
    }

    void clear ()
    {
        traits_type::clear(_c);
        _offset = 0;
    }

    void erase_front (std::size_t n)
    {
        if (n == 0)
            return;

        if (n > size()) {
            throw std::range_error {
                tr::f_("range to erase from front is out of bounds: "
                    "number of elements to erase: {}, container size: {}", n, size())
            };
        }

        _offset += n;

        if (size() == 0)
            clear();
    }

    void resize (std::size_t n)
    {
        traits_type::resize(_c, n + _offset);
    }

    /**
     * Copy @a data with length @a n to archive starting from position @a pos.
     */
    void copy (char const * data, std::size_t n, std::size_t pos)
    {
        traits_type::copy(_c, data, n, pos + _offset);
    }
};

template <>
inline char const * container_traits<std::vector<char>>::data (container_type const & c)
{
    return c.data();
}

template <>
inline std::size_t container_traits<std::vector<char>>::size (container_type const & c)
{
    return c.size();
}

template <>
inline void container_traits<std::vector<char>>::append (container_type & c, char const * data
    , std::size_t n)
{
    c.insert(c.end(), data, data + n);
}

template <>
inline void container_traits<std::vector<char>>::clear (container_type & c)
{
    c.clear();
}

template <>
inline void container_traits<std::vector<char>>::erase (container_type & c, std::size_t pos
    , std::size_t n)
{
    c.erase(c.begin() + pos, c.begin() + pos + n);
}

template <>
inline void container_traits<std::vector<char>>::resize (container_type & c, std::size_t n)
{
    c.resize(n);
}

template <>
inline void container_traits<std::vector<char>>::copy (container_type & c, char const * data
    , std::size_t n, std::size_t pos)
{
    std::copy(data, data + n, c.begin() + pos);
}

NETTY__NAMESPACE_END
