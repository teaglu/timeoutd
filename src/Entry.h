/**
 * Entry
 *
 * An entry is an entry in the database of known keys.  It carries the key,
 * the expire time as an absolute timeval, and the last IP an update was
 * received from.
 *
 * The entry implements notification as well.
 *
 */
class Entry {
public:
	Entry(char const *key, struct timeval& expires, char const *address);
	virtual ~Entry();

	char const *getKey() {
		return key.c_str();
	}
	struct timeval *getExpires() {
		return &expires;
	}

	void receive(struct timeval& expires, char const *address) {
		this->expires= expires;
		this->lastAddress= address;
	}

	static bool CompareTimeout(
		const std::shared_ptr<Entry>,
		const std::shared_ptr<Entry>);

	void notify();

	static void setNotifyScript(char const *file) {
		notifyScript= file;
	}

private:
	static std::string notifyScript;

	std::string key;
	struct timeval expires;

	std::string lastAddress;
};

typedef std::shared_ptr<Entry> EntryRef;

struct EntryCompareTimeout {
	bool operator()(EntryRef a, EntryRef b) {
		return Entry::CompareTimeout(a, b);
	}
};

