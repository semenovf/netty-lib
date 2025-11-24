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
#include <pfs/i18n.hpp>
#include <cstdint>
#include <stdexcept>
#include <vector>

NETTY__NAMESPACE_BEGIN

/**
 * @details
 * Container requirements:
 *      - must satisfy the requirements of ContiguousContainer
 *      - default constructable
 *      - move constructable
 *
 * This class implemented a lightweight erase from front method.
 */
template <typename Container = std::vector<char>>
class archive
{
public:
    using container_type = Container;

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
    {}

    archive & operator = (archive && other) noexcept
    {
        if (this != & other) {
            _c = std::move(other._c);
            _offset = other._offset;
        }

        return *this;
    }

    archive (archive const & other)
        : archive(other.data(), other.size())
    {}

    archive & operator = (archive const &) = delete;

public:
    container_type && container () &
    {
        return std::move(_c);
    }

    /**
     * @return @c nullptr on empty.
     */
    char const * data () const noexcept
    {
        return size() == 0 ? nullptr : data(_c) + _offset;
    }

    bool empty () const noexcept
    {
        return size() == 0;
    }

    std::size_t size () const noexcept
    {
        return size(_c) - _offset;
    }

    void append (archive const & ar)
    {
        append(_c, ar.data(), ar.size());
    }

    void append (container_type const & c)
    {
        append(_c, data(c), size(c));
    }

    void append (char const * data, std::size_t n)
    {
        append(_c, data, n);
    }

    void append (char ch)
    {
        append(_c, & ch, 1);
    }

    void clear ()
    {
        clear(_c);
        _offset = 0;
    }

    void erase (std::size_t pos, std::size_t n)
    {
        if (n == 0)
            return;

        if (pos == 0) {
            erase_front(n);
        } else {
            if ((pos + n) > size()) {
                throw std::range_error {
                      tr::f_("range to erase is out of bounds: "
                        "begin position: {}, number of elements to erase: {}, container size: {}"
                        , pos, n, size())
                };
            }

            erase(_c, pos + _offset, n);
        }
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

private:
    static char const * data (container_type const & c);
    static std::size_t size (container_type const & c);
    static void append (container_type & c, char const * data, std::size_t n);
    static void clear (container_type & c);
    static void erase (container_type & c, std::size_t pos, std::size_t n);
};

template <>
inline char const * archive<std::vector<char>>::data (container_type const & c)
{
    return c.data();
}

template <>
inline std::size_t archive<std::vector<char>>::size (container_type const & c)
{
    return c.size();
}

template <>
inline void archive<std::vector<char>>::append (container_type & c, char const * data, std::size_t n)
{
    c.insert(c.end(), data, data + n);
}

template <>
inline void archive<std::vector<char>>::clear (container_type & c)
{
    c.clear();
}

template <>
inline void archive<std::vector<char>>::erase (container_type & c, std::size_t pos, std::size_t n)
{
    c.erase(c.begin() + pos, c.begin() + pos + n);
}

NETTY__NAMESPACE_END
