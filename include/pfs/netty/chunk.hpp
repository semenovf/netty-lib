////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.09.08 Initial version.
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "namespace.hpp"
#include <stdexcept>
#include <vector>

NETTY__NAMESPACE_BEGIN

/**
 * The main purpose of this class is to implement a lightweight erase method.
 */
template <typename ArchiveTraits>
class chunk
{
    using archive_traits = ArchiveTraits;
    using archive_type = typename archive_traits::archive_type;

public:
    using iterator = typename archive_traits::iterator;
    using const_iterator = typename archive_traits::const_iterator;

private:
    archive_type _data;
    std::size_t _offset {0};

public:
    chunk () = default;
    chunk (chunk const & other) = default;
    chunk (chunk && other) = default;

    chunk (archive_type data)
        : _data(std::move(data))
    {}

    // chunk (char const * data, std::size_t size)
    //     : _data(archive_traits::make_archive(data, size))
    // {}
    //
    // chunk (iterator first, iterator last)
    //     : _data(archive_traits::make_archive(first, last))
    // {}
    //
    // chunk (const_iterator first, const_iterator last)
    //     : _data(archive_traits::make_archive(first, last))
    // {}

    chunk & operator = (chunk const & other)
    {
        if (this != & other) {
            archive_traits::assign(_data, other._data);
            _offset = other._offset;
        }

        return *this;
    }

    chunk & operator = (chunk && other) noexcept
    {
        if (this != & other) {
            archive_traits::assign(_data, std::move(other._data));
            _offset = other._offset;
        }

        return *this;
    }

public:
    bool empty () const noexcept
    {
        return size() == 0;
    }

    std::size_t size () const noexcept
    {
        return archive_traits::size(_data) - _offset;
    }

// FIXME
    // char const * data () const noexcept
    // {
    //     return size() == 0 ? nullptr : _data.data() + _offset;
    // }

    // iterator begin ()
    // {
    //     return size() == 0 ? _data.end() : _data.begin() + _offset;
    // }

    // std::vector<char>::const_iterator begin () const noexcept
    // {
    //     return size() == 0 ? _data.end() : _data.begin() + _offset;
    // }
    //
    // std::vector<char>::const_iterator cbegin () const noexcept
    // {
    //     return size() == 0 ? _data.cend() : _data.cbegin() + _offset;
    // }

    // std::vector<char>::iterator end ()
    // {
    //     return _data.end();
    // }

    // std::vector<char>::const_iterator end () const noexcept
    // {
    //     return _data.end();
    // }
    //
    // std::vector<char>::const_iterator cend () const noexcept
    // {
    //     return _data.cend();
    // }
    //
    // iterator erase (const_iterator first, const_iterator last)
    // {
    //     if (first < last) {
    //         if (first < _data.cbegin() + _offset)
    //             throw std::range_error {"bad left bound of the range"};
    //
    //         if (last > _data.cend())
    //             throw std::range_error {"bad right bound of the range"};
    //
    //         _offset += std::distance(first, last);
    //
    //         if (size() == 0) {
    //             _data.clear();
    //             _offset = 0;
    //             return end();
    //         }
    //     }
    //
    //     return _data.begin() + _offset;
    // }
    //
    // void clear ()
    // {
    //     _data.clear();
    //     _offset = 0;
    // }
};

NETTY__NAMESPACE_END
