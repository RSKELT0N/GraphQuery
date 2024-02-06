/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file bitset.hpp
 * \brief Header including implementation of bitset data
 *        structure which can be used by other translation units
 *        to efficient store a bitmap in a thread-safe context.
 ************************************************************/

#pragma once

#include "lib.h"
#include "atomic_intrinsics.h"

#include <complex>

namespace graphquery::database::utils
{
    template<typename T>
        requires std::is_integral_v<T>
    class CBitset final
    {
      public:
        typedef T WordType;
        explicit CBitset(size_t bit_c);

        ~CBitset() { delete m_start; }

        CBitset(const CBitset &)                 = delete;
        CBitset(CBitset &&) noexcept             = delete;
        CBitset & operator=(const CBitset &)     = delete;
        CBitset & operator=(CBitset &&) noexcept = delete;

        void set(size_t pos) noexcept;
        void flip(size_t pos) const noexcept;
        [[nodiscard]] bool any() const noexcept;
        [[nodiscard]] bool all() const noexcept;
        [[nodiscard]] bool get(size_t pos) const noexcept;

      private:
        static size_t calc_word_amount(size_t bits) noexcept;

        WordType * m_start;
        WordType * m_end;
        size_t m_bit_c;
        size_t m_word_c;
        static constexpr size_t BITS_PER_WORD = sizeof(T) * CHAR_BIT;
    };

    //~ public:
    template<typename T>
        requires std::is_integral_v<T>
    CBitset<T>::CBitset(const size_t bit_c)
    {
        m_bit_c  = bit_c;
        m_word_c = calc_word_amount(m_bit_c);
        m_start  = new WordType[m_word_c];
        m_end    = m_start + m_word_c;
        memset(m_start, 0, m_word_c);
    }

    template<typename T>
        requires std::is_integral_v<T>
    void CBitset<T>::set(const size_t pos) noexcept
    {
        WordType oldval, newval;
        do
        {
            oldval = m_start[pos / BITS_PER_WORD];
            newval = oldval | (1L << (pos & BITS_PER_WORD - 1));
        } while (!atomic_cas(m_start[pos / BITS_PER_WORD], oldval, newval));
    }

    template<typename T>
        requires std::is_integral_v<T>
    bool CBitset<T>::get(const size_t pos) const noexcept
    {
        return (m_start[pos / BITS_PER_WORD] >> (pos & (BITS_PER_WORD - 1))) & 1L;
    }

    template<typename T>
        requires std::is_integral_v<T>
    void CBitset<T>::flip(const size_t pos) const noexcept
    {
        WordType oldval, newval;
        do
        {
            oldval = m_start[pos / BITS_PER_WORD];
            newval = oldval ^ (1L << (pos & BITS_PER_WORD - 1));
        } while (!atomic_cas(m_start[pos / BITS_PER_WORD], oldval, newval));
    }

    template<typename T>
        requires std::is_integral_v<T>
    bool CBitset<T>::any() const noexcept
    {
        for (size_t i = 0; i < m_word_c; i++)
        {
            if (m_start[i] > 0)
                return true;
        }
        return false;
    }

    template<typename T>
        requires std::is_integral_v<T>
    bool CBitset<T>::all() const noexcept
    {
        for (size_t i = 0; i < m_word_c; i++)
        {
            if (m_start[i] < static_cast<WordType>(std::pow(2, BITS_PER_WORD)))
                return false;
        }
        return true;
    }

    //~ private
    template<typename T>
        requires std::is_integral_v<T>
    size_t CBitset<T>::calc_word_amount(const size_t bits) noexcept
    {
        return ceilaferdiv(bits, BITS_PER_WORD);
    }

} // namespace graphquery::database::utils
