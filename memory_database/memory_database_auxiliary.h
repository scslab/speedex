



namespace speedex
{

struct AccountLMDB : public LMDBInstance {

	constexpr static auto DB_NAME = "account_lmdb";

	AccountLMDB() : LMDBInstance() {}

	void open_env() {
		LMDBInstance::open_env(std::string(ROOT_DB_DIRECTORY) + std::string(ACCOUNT_DB));
	}

	void create_db() {
		LMDBInstance::create_db(DB_NAME);
	}

	void open_db() {
		LMDBInstance::open_db(DB_NAME);
	}

	using LMDBInstance::sync;
};

class MemoryDatabase;

struct ThunkKVPair {
	AccountID key;
	xdr::opaque_vec<> msg;

	ThunkKVPair() = default;
};

struct KVAssignment {
	ThunkKVPair& kv;
	const MemoryDatabase& db;
	void operator=(const AccountModificationLog::LogValueT& log);
};

struct DBPersistenceThunk {
	using thunk_list_t = std::vector<ThunkKVPair>;

	std::unique_ptr<thunk_list_t> kvs;
	MemoryDatabase* db;
	uint64_t current_block_number;

	DBPersistenceThunk(MemoryDatabase& db, uint64_t current_block_number)
		: kvs(std::make_unique<thunk_list_t>())
		, db(&db)
		, current_block_number(current_block_number) {}

	KVAssignment operator[](size_t idx) {
		if (idx >= kvs->size()) {
			std::printf("invalid kvs access: %lu (size: %lu)\n", idx, kvs->size());
			throw std::runtime_error("invalid kvs access");
		}

		return KVAssignment{kvs->at(idx), *db};
	}

	void detached_clear() {
		
		
		std::thread(
			[] (thunk_list_t* ptr) {
				delete ptr;
			},
		kvs.release()).detach();
	}

	void resize(size_t sz) {
		kvs->resize(sz);
	}

	void reserve(size_t sz) {
		kvs->reserve(sz);
	}
	
	void push_back(const AccountModificationLog::LogValueT& log) {
		kvs->emplace_back();
		KVAssignment{kvs->back(), *db} = log; 
	}

	size_t size() const {
		return kvs->size();
	}
};

struct AccountCreationThunk {
	uint64_t current_block_number;
	uint64_t num_accounts_created;
};

} /* speedex */