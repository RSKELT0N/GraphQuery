#include <gtest/gtest.h>

#include "fmt/include/fmt/format.h"
#include "db/system.h"
#include "db/utils/bitset.hpp"
#include "db/utils/ring_buffer.hpp"
#include "db/utils/sliding_queue.hpp"

static constexpr size_t bit_size = 10;

GTEST_TEST(utils_bitset, init)
{
    graphquery::database::utils::CBitset<> tmp(bit_size);
    ASSERT_TRUE(tmp.any() == false);
}

GTEST_TEST(utils_bitset, set)
{
    graphquery::database::utils::CBitset<> tmp(bit_size);
    tmp.set(0);
    ASSERT_TRUE(tmp.get(0) == true);
}

GTEST_TEST(utils_bitset, reset)
{
    graphquery::database::utils::CBitset<> tmp(bit_size);
    tmp.set(0);
    tmp.reset();
    ASSERT_TRUE(tmp.any() == false);
}

GTEST_TEST(utils_bitset, swap)
{
    graphquery::database::utils::CBitset<> tmp(bit_size);
    graphquery::database::utils::CBitset<> tmp1(bit_size);
    tmp.set(0);
    tmp.swap(tmp1);
    ASSERT_TRUE(tmp.get(0) == false);
    ASSERT_TRUE(tmp1.get(0) == true);
}

GTEST_TEST(utils_ringbuffer, init)
{
    graphquery::database::utils::CRingBuffer<int8_t, 10> tmp;
    ASSERT_TRUE(tmp.get_current_capacity() == 0);
}

GTEST_TEST(utils_ringbuffer, add_element)
{
    graphquery::database::utils::CRingBuffer<int8_t, 10> tmp;
    tmp.add(10);
    ASSERT_EQ(tmp.get_head(), 5);
}

GTEST_TEST(utils_ringbuffer, check_buffer_size)
{
    graphquery::database::utils::CRingBuffer<int8_t, 10> tmp;
    ASSERT_TRUE(tmp.get_buffer_size() == 10);
}

GTEST_TEST(utils_ringbuffer, overflow_buffer)
{
    graphquery::database::utils::CRingBuffer<int8_t, 2> tmp;
    tmp.add(1);
    tmp.add(2);
    tmp.add(3);
    ASSERT_EQ(tmp[0], 3);
}

GTEST_TEST(utils_queue, init)
{
    graphquery::database::utils::SlidingQueue<int8_t> tmp(10);
    ASSERT_EQ(tmp.empty(), true);
    ASSERT_EQ(tmp.size(), 0);
}

GTEST_TEST(utils_queue, push)
{
    graphquery::database::utils::SlidingQueue<int8_t> tmp(10);
    tmp.push_back(10);
    ASSERT_EQ(tmp.size(), 0);
    ASSERT_EQ(*tmp.begin(), 10);
}

GTEST_TEST(utils_queue, slide_window)
{
    graphquery::database::utils::SlidingQueue<int8_t> tmp(10);
    tmp.push_back(10);
    tmp.slide_window();
    ASSERT_EQ(tmp.size(), 1);
    ASSERT_EQ(*tmp.begin(), 10);
}
