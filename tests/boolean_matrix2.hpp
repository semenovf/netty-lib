////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2025 Vladislav Trifochkin
//
// This file is part of `netty-lib`.
//
// Changelog:
//      2025.03.11 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include <array>
#include <bitset>

template <std::size_t Rows, std::size_t Cols = Rows>
class boolean_matrix2
{
    std::array<std::bitset<Cols>, Rows> _m;

public:
    boolean_matrix2 () = default;

public:
    static constexpr std::size_t rows () noexcept
    {
        return Rows;
    }

    static constexpr std::size_t columns () noexcept
    {
        return Cols;
    }

    constexpr std::size_t row_size () const noexcept
    {
        return Rows;
    }

    constexpr std::size_t column_size () const noexcept
    {
        return Cols;
    }

    /**
     * Returns the value of the bit at the position @a row,@a col performing a bounds check.
     */
    bool test (std::size_t row, std::size_t col) const
    {
        return (row < Rows && col < Cols)
            ? _m[row][col]
            : false;
    }

    /**
     * Sets all cells to true.
     */
    boolean_matrix2 & set ()
    {
        for (auto & row: _m)
            row.set();

        return *this;
    }

    /**
     *  Sets the bit at position @a row,@a col to the value @a value.
     *
     *  @throws std::out_of_range if @a row or @a col or both does not correspond to a valid cell.
     */
    boolean_matrix2 & set (std::size_t row, std::size_t col, bool value = true)
    {
        _m.at(row).set(col, value);
        return *this;
    }

    /**
     * Sets all cells to false.
     */
    boolean_matrix2 & reset ()
    {
        for (auto & row: _m)
            row.reset();

        return *this;
    }

    /**
     *  Sets the bit at position @a row,@a col to @c false.
     *
     *  @throws std::out_of_range if @a row or @a col or both does not correspond to a valid cell.
     */
    boolean_matrix2 & reset (std::size_t row, std::size_t col)
    {
        _m.at(row).reset(col);
        return *this;
    }

    /**
     * Returns the total number of cells that are set to true.
     */
    std::size_t count () const
    {
        std::size_t result = 0;

        for (auto & row: _m)
            result += row.count();

        return result;
    }

    /**
     * Returns the number of cells in row @a row that are set to true.
     *
     * @throws std::out_of_range if @a row does not correspond to a valid row.
     */
    std::size_t count (std::size_t row) const
    {
        return _m.at(row).count();
    }
};
