#include "transaction.h"

#include "lpg_mmap.h"

#include <sys/mman.h>
#include <fcntl.h>

graphquery::database::storage::CTransaction::
CTransaction(const std::filesystem::path & local_path, ILPGModel * lpg): m_lpg(lpg), m_transaction_file(O_RDWR, PROT_READ | PROT_WRITE, MAP_SHARED)
{
    m_transaction_file.set_path(local_path);
}

void
graphquery::database::storage::CTransaction::close() noexcept
{
    m_transaction_file.close();
}

void
graphquery::database::storage::CTransaction::reset() noexcept
{
    m_transaction_file.resize(INITIAL_TRANSACTION_FILE_SIZE);
    define_transaction_header();
    (void) m_transaction_file.sync();
}

void
graphquery::database::storage::CTransaction::init() noexcept
{
    if (!CDiskDriver::check_if_file_exists((m_transaction_file.get_path() / TRANSACTION_FILE_NAME).string()))
        set_up();
    else
        load();
}

void
graphquery::database::storage::CTransaction::set_up()
{
    CDiskDriver::create_file(m_transaction_file.get_path(), TRANSACTION_FILE_NAME, INITIAL_TRANSACTION_FILE_SIZE);
    m_transaction_file.open(TRANSACTION_FILE_NAME);
    define_transaction_header();
}

void
graphquery::database::storage::CTransaction::load()
{
    m_transaction_file.open(TRANSACTION_FILE_NAME);
}

void
graphquery::database::storage::CTransaction::define_transaction_header()
{
    auto header_ptr = read_transaction_header();
    utils::atomic_store(&header_ptr->transaction_c, 0ULL);
    utils::atomic_store(&header_ptr->transactions_start_addr, TRANSACTIONS_START_ADDR);
    utils::atomic_store(&header_ptr->eof_addr, TRANSACTIONS_START_ADDR);
}

template<typename T>
graphquery::database::storage::SRef_t<T>
graphquery::database::storage::CTransaction::read_transaction(const int64_t seek)
{
    return m_transaction_file.ref<T>(seek);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CTransaction::SHeaderBlock>
graphquery::database::storage::CTransaction::read_transaction_header()
{
    return m_transaction_file.ref<SHeaderBlock>(TRANSACTION_HEADER_START_ADDR);
}

void
graphquery::database::storage::CTransaction::commit_rm_vertex(ILPGModel::SNodeID src) noexcept
{
    auto transaction_hdr   = read_transaction_header();
    const auto commit_addr = utils::atomic_fetch_add(&transaction_hdr->eof_addr, static_cast<uint32_t>(sizeof(SVertexTransaction)));
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();

    auto transaction_ptr = read_transaction<SVertexTransaction>(commit_addr);

    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = src;
    transaction_ptr->commit.remove      = 1;
    transaction_ptr->commit.property_c  = 0;
    transaction_ptr->commit.label_c     = 0;
}

void
graphquery::database::storage::CTransaction::commit_rm_edge(ILPGModel::SNodeID src, ILPGModel::SNodeID dst, const std::string_view edge_label) noexcept
{
    auto transaction_hdr   = read_transaction_header();
    const auto commit_addr = utils::atomic_fetch_add(&transaction_hdr->eof_addr, static_cast<uint32_t>(sizeof(SEdgeTransaction)));
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();

    auto transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->type               = ETransactionType::edge;
    transaction_ptr->commit.src         = src;
    transaction_ptr->commit.dst         = dst;
    transaction_ptr->commit.remove      = 1;
    transaction_ptr->commit.property_c  = 0;
    strncpy(&transaction_ptr->commit.edge_label[0], edge_label.data(), CFG_LPG_LABEL_LENGTH - 1);
}

void
graphquery::database::storage::CTransaction::commit_vertex(const std::vector<std::string_view> & labels, const std::vector<ILPGModel::SProperty_t> & props, const ILPGModel::SNodeID optional_id) noexcept
{
    auto transaction_hdr   = read_transaction_header();
    const auto commit_addr = utils::atomic_fetch_add(&transaction_hdr->eof_addr,
                                                     static_cast<uint32_t>(sizeof(SVertexTransaction) + props.size() * sizeof(ILPGModel::SProperty_t) + CFG_LPG_LABEL_LENGTH * labels.size()));
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();

    SRef_t<SVertexTransaction> transaction_ptr = read_transaction<SVertexTransaction>(commit_addr);

    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = optional_id;
    transaction_ptr->commit.remove      = 0;
    transaction_ptr->commit.property_c  = props.size();
    transaction_ptr->commit.label_c     = labels.size();

    transaction_ptr.~SRef_t<SVertexTransaction>(); //~ Remove reference to transaction file.

    uint32_t curr_addr = commit_addr + sizeof(SVertexTransaction);

    for (const auto & label : labels)
    {
        auto label_ptr = m_transaction_file.ref<SLabel>(curr_addr);
        strncpy(&label_ptr->label[0], label.data(), CFG_LPG_LABEL_LENGTH - 1);
        curr_addr += CFG_LPG_LABEL_LENGTH;
    }

    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(curr_addr);
        strncpy(&prop->key[0], key, CFG_LPG_PROPERTY_KEY_LENGTH - 1);
        strncpy(&prop->value[0], value, CFG_LPG_PROPERTY_VALUE_LENGTH - 1);
        curr_addr += sizeof(ILPGModel::SProperty_t);
    }
}

void
graphquery::database::storage::CTransaction::commit_edge(const ILPGModel::SNodeID src,
                                                         const ILPGModel::SNodeID dst,
                                                         const std::string_view edge_label,
                                                         const std::vector<ILPGModel::SProperty_t> & props,
                                                         const bool undirected) noexcept
{
    auto transaction_hdr = read_transaction_header();
    const auto commit_addr =
        utils::atomic_fetch_add(&transaction_hdr->eof_addr,
                                static_cast<uint32_t>(sizeof(SEdgeTransaction) + props.size() * sizeof(ILPGModel::SProperty_t)));
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();

    SRef_t<SEdgeTransaction> transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->type              = ETransactionType::edge;
    transaction_ptr->commit.src        = src;
    transaction_ptr->commit.dst        = dst;
    transaction_ptr->commit.remove     = 0;
    transaction_ptr->commit.property_c = props.size();
    transaction_ptr->commit.undirected = undirected;
    strncpy(&transaction_ptr->commit.edge_label[0], edge_label.data(), CFG_LPG_LABEL_LENGTH - 1);

    transaction_ptr.~SRef_t<SEdgeTransaction>(); //~ Remove reference to transaction file.

    uint32_t curr_addr = commit_addr + sizeof(SEdgeTransaction);

    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(curr_addr);
        strncpy(&prop->key[0], key, CFG_LPG_PROPERTY_KEY_LENGTH - 1);
        strncpy(&prop->value[0], value, CFG_LPG_PROPERTY_VALUE_LENGTH - 1);
        curr_addr += sizeof(ILPGModel::SProperty_t);
    }
}

void
graphquery::database::storage::CTransaction::handle_transactions() noexcept
{
    m_transaction_file.seek(TRANSACTIONS_START_ADDR);
    const uint32_t transaction_c = utils::atomic_load(&read_transaction_header()->transaction_c);

    SRef_t<ETransactionType> type;
    SRef_t<SVertexTransaction> v_transc = SRef_t<SVertexTransaction>();
    SRef_t<SEdgeTransaction> e_transc   = SRef_t<SEdgeTransaction>();

    std::vector<std::string_view> labels  = {};
    std::vector<ILPGModel::SProperty_t> props = {};

    for (uint32_t i = 0; i < transaction_c; i++)
    {
        type = m_transaction_file.ref_update<ETransactionType>();
        switch (*type)
        {
        case ETransactionType::vertex:
        {
            v_transc = m_transaction_file.ref_update<SVertexTransaction>();

            if (v_transc->commit.label_c > 0)
            {
                labels.resize(v_transc->commit.label_c);
                m_transaction_file.read(&labels[0], CFG_LPG_LABEL_LENGTH, v_transc->commit.label_c);
            }

            if (v_transc->commit.property_c > 0)
            {
                props.resize(v_transc->commit.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, v_transc->commit.property_c);
            }
            process_vertex_transaction(v_transc, labels, props);
            break;
        }
        case ETransactionType::edge:
        {
            e_transc = m_transaction_file.ref_update<SEdgeTransaction>();

            if (e_transc->commit.property_c > 0)
            {
                props.resize(e_transc->commit.property_c);
                m_transaction_file.read(&props[0], CFG_LPG_PROPERTY_KEY_LENGTH + CFG_LPG_PROPERTY_VALUE_LENGTH, e_transc->commit.property_c);
            }

            process_edge_transaction(e_transc, props);
            break;
        }
        }
        labels.clear();
        props.clear();

        v_transc.~SRef_t();
        e_transc.~SRef_t();
    }
}

void
graphquery::database::storage::CTransaction::process_vertex_transaction(SRef_t<SVertexTransaction> & transaction,
                                                                        const std::vector<std::string_view> & src_labels,
                                                                        const std::vector<ILPGModel::SProperty_t> & props) const noexcept
{
    if (transaction->commit.remove == 0)
        if (transaction->commit.optional_id != UINT_MAX)
            (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_vertex_entry(transaction->commit.optional_id, src_labels, props);
        else
            (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_vertex_entry(src_labels, props);
    else
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->rm_vertex_entry(transaction->commit.optional_id);
}

void
graphquery::database::storage::CTransaction::process_edge_transaction(SRef_t<SEdgeTransaction> & transaction,
                                                                      const std::vector<ILPGModel::SProperty_t> & props) const noexcept
{
    if (transaction->commit.remove == 0)
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_edge_entry(transaction->commit.src,
                                                                          transaction->commit.dst,
                                                                          transaction->commit.edge_label,
                                                                          props,
                                                                          transaction->commit.undirected);
    else
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->rm_edge_entry(transaction->commit.src, transaction->commit.dst);
}
