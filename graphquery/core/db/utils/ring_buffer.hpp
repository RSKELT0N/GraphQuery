/************************************************************
 * \author Ryan Skelton
 * \date 18/09/2023
 * \file ring_buffer.hpp
 * \brief Header including implementation of ring_buffer data
 *        structure which can be used by other translation units
 *        to efficient store data in a ring-based buffer.
 ************************************************************/

#pragma once

#include <array>
#include <cassert>

namespace graphquery::database::utils
{
    template<typename T, size_t BufferSize>
    class CRingBuffer final
    {
      public:
        struct SData
        {
            size_t head;
            size_t current_capacity;
            ::std::array<T, BufferSize> * data;
        };

        inline constexpr CRingBuffer();

        void clear() noexcept;
        void add(const T & elem) noexcept;
        T operator[](size_t i) const noexcept;
        [[nodiscard]] size_t get_head() const noexcept;
        [[nodiscard]] size_t get_current_capacity() const noexcept;
        [[nodiscard]] size_t get_buffer_size() const noexcept;

      private:
        size_t m_tail          = 0;
        size_t m_curr_capacity = 0;
        ::std::array<T, BufferSize> m_data;
    };

    //~ public:
    template<typename T, size_t BufferSize>
    inline constexpr CRingBuffer<T, BufferSize>::CRingBuffer()
    {
        m_data = ::std::array<T, BufferSize>();
    }

    template<typename T, size_t BufferSize>
    void CRingBuffer<T, BufferSize>::clear() noexcept
    {
        m_curr_capacity = 0;
    }

    template<typename T, size_t BufferSize>
    void CRingBuffer<T, BufferSize>::add(const T & elem) noexcept
    {
        m_data[m_tail]  = elem;
        m_tail          = (m_tail + 1) % BufferSize;
        m_curr_capacity = (m_curr_capacity < BufferSize) ? m_curr_capacity + 1 : BufferSize;
    }

    template<typename T, size_t BufferSize>
    T CRingBuffer<T, BufferSize>::operator[](size_t i) const noexcept
    {
        assert(BufferSize > i && "Accessed element is out of range");
        return m_data.operator[](i);
    }

    template<typename T, size_t BufferSize>
    size_t CRingBuffer<T, BufferSize>::get_current_capacity() const noexcept
    {
        return this->m_curr_capacity;
    }

    template<typename T, size_t BufferSize>
    size_t CRingBuffer<T, BufferSize>::get_head() const noexcept
    {
        return ((this->m_tail - 1) - this->m_curr_capacity) % BufferSize;
    }

    template<typename T, size_t BufferSize>
    size_t CRingBuffer<T, BufferSize>::get_buffer_size() const noexcept
    {
        return BufferSize;
    }
} // namespace graphquery::database::utils
