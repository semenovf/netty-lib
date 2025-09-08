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
class chunk
{
public:
    using iterator = std::vector<char>::iterator;
    using const_iterator = std::vector<char>::const_iterator;

private:
    std::vector<char> _data;
    std::size_t _offset {0};

public:
    chunk () = default;

    chunk (chunk const & other)
        : _data(other._data)
        , _offset(other._offset)
    {}

    chunk (chunk && other)
        : _data(std::move(other._data))
        , _offset(other._offset)
    {
        other._offset = 0;
    }

    chunk (std::vector<char> data)
        : _data(std::move(data))
    {}

    chunk (char const * data, std::size_t size)
        : _data(data, data + size)
    {}

    template <typename InputIt>
    chunk (InputIt first, InputIt last)
        : _data(first, last)
    {}

    chunk & operator = (chunk const & other)
    {
        if (this != & other) {
            _data = other._data;
            _offset = other._offset;
        }

        return *this;
    }

    chunk & operator = (chunk && other) noexcept
    {
        if (this != & other) {
            _data = std::move(other._data);
            _offset = other._offset;
            other._offset = 0;
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
        return _data.size() - _offset;
    }

    char const * data () const noexcept
    {
        return size() == 0 ? nullptr : _data.data() + _offset;
    }

    std::vector<char>::iterator begin ()
    {
        return size() == 0 ? _data.end() : _data.begin() + _offset;
    }

    std::vector<char>::const_iterator begin () const noexcept
    {
        return size() == 0 ? _data.end() : _data.begin() + _offset;
    }

    std::vector<char>::const_iterator cbegin () const noexcept
    {
        return size() == 0 ? _data.cend() : _data.cbegin() + _offset;
    }

    std::vector<char>::iterator end ()
    {
        return _data.end();
    }

    std::vector<char>::const_iterator end () const noexcept
    {
        return _data.end();
    }

    std::vector<char>::const_iterator cend () const noexcept
    {
        return _data.cend();
    }

    iterator erase (const_iterator first, const_iterator last)
    {
        if (first < last) {
            if (first < _data.cbegin() + _offset)
                throw std::range_error {"bad left bound of the range"};

            if (last > _data.cend())
                throw std::range_error {"bad right bound of the range"};

            _offset += std::distance(first, last);

            if (size() == 0) {
                _data.clear();
                _offset = 0;
                return end();
            }
        }

        return _data.begin() + _offset;
    }

    void clear ()
    {
        _data.clear();
        _offset = 0;
    }
};

NETTY__NAMESPACE_END
