/**
 * Sender
 *
 * Generic interface for something that sends notifications.
 */
class Sender {
public:
	Sender();
	virtual ~Sender();

	virtual bool start() = 0;
	virtual void stop() = 0;
};

typedef std::shared_ptr<Sender> SenderRef;

