#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define LIB_EXPORT __declspec(dllexport)
#else
#define LIB_EXPORT
#endif

#include "db/storage/diskdriver/diskdriver.h"
#include "db/storage/graphmodel.h"
#include "db/storage/config.h"

namespace graphquery::database::storage
{
    class CGraphModelPropertyLabel final : public IGraphModel
    {
    private:
        struct SGraphHead_t
        {
            char graph_name[GRAPH_NAME_LENGTH] = {};
            char graph_type[GRAPH_MODEL_TYPE_LENGTH] = {};
            int64_t vertices_c = {};
            int64_t edges_c = {};
            int16_t vertex_label_c = {};
            int16_t edge_label_c = {};
        } __attribute__((packed));

    public:
        CGraphModelPropertyLabel();
        ~CGraphModelPropertyLabel() override;

        void load_graph(std::string_view graph) noexcept override;
        void create_graph(std::string_view graph) noexcept override;
        void Close() noexcept override;

    private:
        void CheckIfMainFileExists() noexcept;
        void StoreGraphHead() noexcept;
        void LoadGraphHead() noexcept;

        CDiskDriver m_graph_head;
        SGraphHead_t m_graph_header = {};
    };
}
