/************************************************************
* \author Ryan Skelton
* \date 18/09/2023
* \file ring_buffer.h
* \brief Header including implementation of rung_buffer data
*        structure which can be used by other translation units
*        to efficient store data in a ring-based buffer.
************************************************************/

#pragma once

#include <array>
#include <memory>

namespace graphquery::database::utils
{
    template<typename ElemType, size_t BufferSize>
    class CRingBuffer final
    {
    public:
        struct SData
        {
            std::size_t head;
            std::size_t current_capacity;
            std::array<ElemType, BufferSize> * data;
        };

        CRingBuffer();
        void Clear() noexcept;
        void Add(const ElemType & elem) noexcept;
        ElemType operator[](std::size_t i) const noexcept;
        std::size_t Get_Head() const noexcept;
        std::size_t Get_Current_Capacity() const noexcept;
        std::size_t Get_BufferSize() const noexcept;

    private:
        std::size_t m_tail = 0;
        std::size_t m_curr_capacity = 0;
        std::unique_ptr<std::array<ElemType, BufferSize>> m_data;
    };


    //~ public:
    template<typename ElemType, size_t BufferSize>
    CRingBuffer<ElemType, BufferSize>::CRingBuffer()
    {
        m_data = std::make_unique<std::array<ElemType, BufferSize>>();
    }

    template<typename ElemType, size_t BufferSize>
    void CRingBuffer<ElemType, BufferSize>::Clear() noexcept
    {
        m_curr_capacity = 0;
    }

    template<typename ElemType, size_t BufferSize>
    void CRingBuffer<ElemType, BufferSize>::Add(const ElemType & elem) noexcept
    {
        m_data->operator[](m_tail) = elem;
        m_tail = (m_tail + 1) % BufferSize;
        m_curr_capacity = (m_curr_capacity < BufferSize) ? m_curr_capacity + 1 : BufferSize;
    }

    template<typename ElemType, size_t BufferSize>
    ElemType
    CRingBuffer<ElemType, BufferSize>::operator[](std::size_t i) const noexcept
    {
        assert(0 <= i && BufferSize >= i && "Accessed element is out of range");
        return m_data->operator[](i);
    }

    template<typename ElemType, size_t BufferSize>
    std::size_t
    CRingBuffer<ElemType, BufferSize>::Get_Current_Capacity() const noexcept
    {
        return this->m_curr_capacity;
    }

    template<typename ElemType, size_t BufferSize>
    std::size_t
    CRingBuffer<ElemType, BufferSize>::Get_Head() const noexcept
    {
        return ((this->m_tail - 1) - this->m_curr_capacity) % BufferSize;
    }

    template<typename ElemType, size_t BufferSize>
    std::size_t
    CRingBuffer<ElemType, BufferSize>::Get_BufferSize() const noexcept
    {
        return BufferSize;
    }
}