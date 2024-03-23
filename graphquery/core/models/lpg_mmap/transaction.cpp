#include "transaction.h"

#include "lpg_mmap.h"

graphquery::database::storage::CTransaction::CTransaction(const std::filesystem::path & local_path, ILPGModel * lpg, const std::shared_ptr<logger::CLogSystem> & logsys, const bool & sync_state):
    m_lpg(lpg), _sync_state_(sync_state), m_log_system(logsys)
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
    // m_transaction_file.resize(CDiskDriver::DEFAULT_FILE_SIZE);
    // define_transaction_header();
    // (void) m_transaction_file.sync();
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
    CDiskDriver::create_file(m_transaction_file.get_path(), TRANSACTION_FILE_NAME);
    m_transaction_file.open(TRANSACTION_FILE_NAME);
    store_transaction_header();
}

void
graphquery::database::storage::CTransaction::load()
{
    m_transaction_file.open(TRANSACTION_FILE_NAME);
}

void
graphquery::database::storage::CTransaction::store_transaction_header()
{
    auto header_ptr = read_transaction_header();
    utils::atomic_store(&header_ptr->transaction_c, 0ULL);
    utils::atomic_store(&header_ptr->transactions_start_addr, TRANSACTIONS_START_ADDR);
    utils::atomic_store(&header_ptr->rollback_entries_start_addr, ROLLBACK_ENTRIES_START_ADDR);
    utils::atomic_store(&header_ptr->eof_addr, TRANSACTIONS_START_ADDR);
    utils::atomic_store(&header_ptr->valid_eof_addr, TRANSACTIONS_START_ADDR);
    utils::atomic_store(&header_ptr->rollback_entry_c, static_cast<uint8_t>(0));
}

void
graphquery::database::storage::CTransaction::store_rollback_entry(const std::string_view name) noexcept
{
    std::stringstream ss;
    const auto curr_time   = std::chrono::system_clock::now();
    const auto curr_time_c = std::chrono::system_clock::to_time_t(curr_time);

    ss << std::put_time(std::localtime(&curr_time_c), "%Y-%m-%d %X");

    const uint8_t entry          = read_transaction_header()->rollback_entry_c++ % ROLLBACK_MAX_AMOUNT;
    SRef_t<SRollbackEntry> r_ptr = read_rollback_entry(entry);

    const auto curr_eof_addr = utils::atomic_load(&read_transaction_header()->eof_addr);

    strncpy(&r_ptr->name[0], fmt::format("{} ({})", ss.str(), name).c_str(), CFG_GRAPH_ROLLBACK_NAME_LENGTH - 1);
    utils::atomic_store(&r_ptr->eor_addr, curr_eof_addr);
    m_log_system->info(fmt::format("Rollback ({}) has been created", name));
}

std::vector<std::string>
graphquery::database::storage::CTransaction::fetch_rollback_table() noexcept
{
    const uint8_t rollback_c = utils::atomic_load(&read_transaction_header()->rollback_entry_c);
    std::vector<std::string> rollback_table(rollback_c);
    rollback_table.resize(rollback_c);

    auto gbl_r_ptr = read_rollback_entry(0);
    for (uint8_t i = 0; i < rollback_c; i++)
    {
        rollback_table[i] = (gbl_r_ptr + i)->name;
    }
    return rollback_table;
}

template<typename T, bool write>
graphquery::database::storage::SRef_t<T, write>
graphquery::database::storage::CTransaction::read_transaction(const uint64_t seek)
{
    return m_transaction_file.ref<T, write>(seek);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CTransaction::SHeaderBlock>
graphquery::database::storage::CTransaction::read_transaction_header()
{
    return m_transaction_file.ref<SHeaderBlock>(TRANSACTION_HEADER_START_ADDR);
}

graphquery::database::storage::SRef_t<graphquery::database::storage::CTransaction::SRollbackEntry>
graphquery::database::storage::CTransaction::read_rollback_entry(const uint8_t i) noexcept
{
    return m_transaction_file.ref<SRollbackEntry>(ROLLBACK_ENTRIES_START_ADDR + static_cast<int64_t>(i) * sizeof(SRollbackEntry));
}

void
graphquery::database::storage::CTransaction::storage_persist() const noexcept
{
    if (_sync_state_)
        (void) m_transaction_file.sync();
}

uint64_t
graphquery::database::storage::CTransaction::log_rm_vertex(const Id_t src) noexcept
{
    auto transaction_hdr           = read_transaction_header();
    static constexpr uint64_t size = sizeof(SVertexTransaction);
    const auto commit_addr         = utils::atomic_fetch_add(&transaction_hdr->eof_addr, size);
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();
    transaction_hdr = {};

    auto transaction_ptr = read_transaction<SVertexTransaction, true>(commit_addr);

    transaction_ptr->size               = size;
    transaction_ptr->committed          = 0;
    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = src;
    transaction_ptr->commit.remove      = 1;
    transaction_ptr->commit.property_c  = 0;
    transaction_ptr->commit.label_c     = 0;

    return commit_addr;
}

uint64_t
graphquery::database::storage::CTransaction::log_rm_edge(const Id_t src, const Id_t dst, const std::string_view edge_label) noexcept
{
    auto transaction_hdr           = read_transaction_header();
    static constexpr uint64_t size = sizeof(SEdgeTransaction);
    const auto commit_addr         = utils::atomic_fetch_add(&transaction_hdr->eof_addr, size);
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();
    transaction_hdr = {};

    auto transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->size              = size;
    transaction_ptr->committed         = 0;
    transaction_ptr->type              = ETransactionType::edge;
    transaction_ptr->commit.src        = src;
    transaction_ptr->commit.dst        = dst;
    transaction_ptr->commit.remove     = 1;
    transaction_ptr->commit.property_c = 0;
    strcpy(&transaction_ptr->commit.edge_label[0], edge_label.data());

    return commit_addr;
}

uint64_t
graphquery::database::storage::CTransaction::log_vertex(const std::vector<std::string_view> & labels, const std::vector<ILPGModel::SProperty_t> & props, const Id_t optional_id) noexcept
{
    auto transaction_hdr   = read_transaction_header();
    const uint64_t size    = sizeof(SVertexTransaction) + props.size() * sizeof(ILPGModel::SProperty_t) + CFG_LPG_LABEL_LENGTH * labels.size();
    const auto commit_addr = utils::atomic_fetch_add(&transaction_hdr->eof_addr, size);
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();
    transaction_hdr = {};

    SRef_t<SVertexTransaction, true> transaction_ptr = read_transaction<SVertexTransaction, true>(commit_addr);

    transaction_ptr->size               = size;
    transaction_ptr->committed          = 0;
    transaction_ptr->type               = ETransactionType::vertex;
    transaction_ptr->commit.optional_id = optional_id;
    transaction_ptr->commit.remove      = 0;
    transaction_ptr->commit.property_c  = props.size();
    transaction_ptr->commit.label_c     = labels.size();

    transaction_ptr.~SRef_t(); //~ Remove reference to transaction file.
    transaction_ptr = {};

    auto curr_addr = commit_addr + sizeof(SVertexTransaction);

    for (const auto & label : labels)
    {
        auto label_ptr = m_transaction_file.ref<ILPGModel::SLabel>(static_cast<int64_t>(curr_addr));
        strcpy(&label_ptr->label[0], label.data());
        curr_addr += CFG_LPG_LABEL_LENGTH;
    }

    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(static_cast<int64_t>(curr_addr));
        strcpy(&prop->key[0], key);
        strcpy(&prop->value[0], value);
        curr_addr += sizeof(ILPGModel::SProperty_t);
    }

    return commit_addr;
}

uint64_t
graphquery::database::storage::CTransaction::log_edge(const Id_t src,
                                                      const Id_t dst,
                                                      const std::string_view edge_label,
                                                      const std::vector<ILPGModel::SProperty_t> & props,
                                                      const bool undirected) noexcept
{
    auto transaction_hdr   = read_transaction_header();
    const uint64_t size    = sizeof(SEdgeTransaction) + props.size() * sizeof(ILPGModel::SProperty_t);
    const auto commit_addr = utils::atomic_fetch_add(&transaction_hdr->eof_addr, size);
    utils::atomic_fetch_inc(&transaction_hdr->transaction_c);

    //~ Lose reference to transaction_hdr
    transaction_hdr.~SRef_t();
    transaction_hdr = {};

    SRef_t<SEdgeTransaction> transaction_ptr = read_transaction<SEdgeTransaction>(commit_addr);

    transaction_ptr->size              = size;
    transaction_ptr->committed         = 0;
    transaction_ptr->type              = ETransactionType::edge;
    transaction_ptr->commit.src        = src;
    transaction_ptr->commit.dst        = dst;
    transaction_ptr->commit.remove     = 0;
    transaction_ptr->commit.property_c = props.size();
    transaction_ptr->commit.undirected = undirected;
    strcpy(&transaction_ptr->commit.edge_label[0], edge_label.data());

    transaction_ptr.~SRef_t<SEdgeTransaction>(); //~ Remove reference to transaction file.
    transaction_ptr = {};

    auto curr_addr = commit_addr + sizeof(SEdgeTransaction);

    for (const auto & [key, value] : props)
    {
        auto prop = m_transaction_file.ref<ILPGModel::SProperty_t>(static_cast<int64_t>(curr_addr));
        strcpy(&prop->key[0], key);
        strcpy(&prop->value[0], value);
        curr_addr += sizeof(ILPGModel::SProperty_t);
    }

    return commit_addr;
}

uint64_t
graphquery::database::storage::CTransaction::get_valid_eor_addr() noexcept
{
    return read_transaction_header()->valid_eof_addr;
}

std::vector<std::string_view>
graphquery::database::storage::CTransaction::slabel_to_strview_vector(const std::vector<ILPGModel::SLabel> & vec) noexcept
{
    std::vector<std::string_view> res(vec.size());

    for (size_t i = 0; i < vec.size(); i++)
        res[i] = vec[i].label;

    return res;
}

void
graphquery::database::storage::CTransaction::rollback(const uint64_t rollback_entry) noexcept
{
    m_transaction_file.seek(TRANSACTIONS_START_ADDR);
    auto curr_addr         = m_transaction_file.get_seek_offset();
    auto rollback_eor_addr = rollback_entry;

    SRef_t<ETransactionType> type;
    SRef_t<SVertexTransaction> v_transc = SRef_t<SVertexTransaction>();
    SRef_t<SEdgeTransaction> e_transc   = SRef_t<SEdgeTransaction>();

    std::vector<ILPGModel::SLabel> labels     = {};
    std::vector<ILPGModel::SProperty_t> props = {};

    while (curr_addr < rollback_eor_addr)
    {
        type = m_transaction_file.ref<ETransactionType>();
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

            if (v_transc->committed)
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

            if (e_transc->committed)
                process_edge_transaction(e_transc, props);
            break;
        }
        }

        curr_addr = m_transaction_file.get_seek_offset();

        labels.clear();
        props.clear();

        v_transc.~SRef_t();
        e_transc.~SRef_t();
    }
}

void
graphquery::database::storage::CTransaction::handle_transactions() noexcept
{
    m_transaction_file.seek(TRANSACTIONS_START_ADDR);
    const uint32_t transaction_c = read_transaction_header()->transaction_c;
    SRef_t<ETransactionType> type;
    SRef_t<SVertexTransaction> v_transc = SRef_t<SVertexTransaction>();
    SRef_t<SEdgeTransaction> e_transc   = SRef_t<SEdgeTransaction>();

    std::vector<ILPGModel::SLabel> labels     = {};
    std::vector<ILPGModel::SProperty_t> props = {};

    for (uint32_t i = 0; i < transaction_c; i++)
    {
        type = m_transaction_file.ref<ETransactionType>();
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

            if (v_transc->committed)
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

            if (e_transc->committed)
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
                                                                        const std::vector<ILPGModel::SLabel> & src_labels,
                                                                        const std::vector<ILPGModel::SProperty_t> & props) const noexcept
{
    auto labels = slabel_to_strview_vector(src_labels);
    if (transaction->commit.remove == 0)
        if (transaction->commit.optional_id != std::numeric_limits<Id_t>::max())
            (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_vertex_entry(transaction->commit.optional_id, labels, props);
        else
            (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_vertex_entry(labels, props);
    else
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->rm_vertex_entry(transaction->commit.optional_id);
}

void
graphquery::database::storage::CTransaction::process_edge_transaction(SRef_t<SEdgeTransaction> & transaction, const std::vector<ILPGModel::SProperty_t> & props) const noexcept
{
    if (transaction->commit.remove == 0)
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->add_edge_entry(transaction->commit.src, transaction->commit.dst, transaction->commit.edge_label, props, transaction->commit.undirected);
    else
        (void) dynamic_cast<CMemoryModelMMAPLPG *>(m_lpg)->rm_edge_entry(transaction->commit.src, transaction->commit.dst);
}