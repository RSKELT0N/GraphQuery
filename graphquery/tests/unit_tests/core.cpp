#include <gtest/gtest.h>

#include "fmt/include/fmt/format.h"
#include "db/system.h"

GTEST_TEST(GraphQuery_core, test_initialisation)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
}

GTEST_TEST(GraphQuery_core, test_no_db_loaded)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_FALSE(graphquery::database::_db_storage->get_is_db_loaded());
}

GTEST_TEST(GraphQuery_core, test_no_graph_loaded)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_FALSE(graphquery::database::_db_storage->get_is_graph_loaded());
}

GTEST_TEST(GraphQuery_core, test_is_db_storage_valid)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_FALSE(graphquery::database::_db_storage->get_is_graph_loaded());
}

//
// GTEST_TEST(GraphQuery_core, test_initialisation)
// {
//     ASSERT_TRUE(graphquery::database::initialise() == graphquery::database::EStatus::valid);
// }
//
// GTEST_TEST(GraphQuery_core, test_initialisation)
// {
//     ASSERT_TRUE(graphquery::database::initialise() == graphquery::database::EStatus::valid);
// }