#include <gtest/gtest.h>

#include "fmt/include/fmt/format.h"
#include "db/system.h"

GTEST_TEST(GraphQuery_core, test_initialisation)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
}

GTEST_TEST(GraphQuery_core, test_is_db_storage_valid)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_TRUE(graphquery::database::_db_storage != nullptr);
}

GTEST_TEST(GraphQuery_core, test_is_db_analytic_valid)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_TRUE(graphquery::database::_db_analytic != nullptr);
}

GTEST_TEST(GraphQuery_core, test_is_db_query_valid)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_TRUE(graphquery::database::_db_query != nullptr);
}

GTEST_TEST(GraphQuery_core, test_is_db_graph_valid)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    ASSERT_TRUE(*graphquery::database::_db_graph.get() == nullptr);
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

GTEST_TEST(GraphQuery_core, test_create_db)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    EXPECT_NO_THROW();
    std::remove("test");
}

GTEST_TEST(GraphQuery_core, test_graph_table_empty)
{
    std::filesystem::remove_all("table_empty");
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "table_empty");
    fmt::print("{}\n", std::filesystem::current_path().string());
    ASSERT_TRUE(graphquery::database::_db_storage->get_graph_table().empty());
}

GTEST_TEST(GraphQuery_core, test_db_string)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    ASSERT_EQ(graphquery::database::_db_storage->get_db_info().empty(), false);
    std::filesystem::remove_all("test");
}

GTEST_TEST(GraphQuery_core, test_check_graph_exists)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    ASSERT_EQ(graphquery::database::_db_storage->check_if_graph_exists("null"), false);
    std::remove("test");
}

GTEST_TEST(GraphQuery_core, test_add_graph)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ(graphquery::database::_db_storage->check_if_graph_exists("Graph"), true);
}

GTEST_TEST(GraphQuery_lpg, check_vertex_count)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ((*graphquery::database::_db_graph)->get_num_vertices(), 0);
}

GTEST_TEST(GraphQuery_lpg, check_edge_count)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ((*graphquery::database::_db_graph)->get_num_edges(), 0);
}

GTEST_TEST(GraphQuery_lpg, check_vertex_label_count)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ((*graphquery::database::_db_graph)->get_num_vertex_labels(), 0);
    std::remove("test");
}

GTEST_TEST(GraphQuery_lpg, check_edge_label_count)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ((*graphquery::database::_db_graph)->get_num_edge_labels(), 0);
    std::remove("test");
}

GTEST_TEST(GraphQuery_lpg, check_if_vertex_exists)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    ASSERT_EQ((*graphquery::database::_db_graph)->get_vertex(0), std::nullopt);
    std::remove("test");
}

GTEST_TEST(GraphQuery_lpg, add_vertex)
{
    ASSERT_TRUE(graphquery::database::initialise(false) == graphquery::database::EStatus::valid);
    graphquery::database::_db_storage->init(std::filesystem::current_path(), "test");
    graphquery::database::_db_storage->create_graph("Graph", "lpg_mmap");
    (*graphquery::database::_db_graph)->add_vertex(0, {}, {});
    ASSERT_EQ((*graphquery::database::_db_graph)->get_vertex(0).has_value(), true);

    std::remove("test");
}


// GTEST_TEST(GraphQuery_core, test_initialisation)
// {
//     ASSERT_TRUE(graphquery::database::initialise() == graphquery::database::EStatus::valid);
// }
//
// GTEST_TEST(GraphQuery_core, test_initialisation)
// {
//     ASSERT_TRUE(graphquery::database::initialise() == graphquery::database::EStatus::valid);
// }