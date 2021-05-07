#include "system.h"

#include "Log.h"
#include "Entry.h"
#include "Scheduler.h"
#include "Listener.h"
#include "UdpListener.h"
#include "SimpleListener.h"

#define RECV_BUFFER_SIZE 2047

SimpleListener::SimpleListener(
	SchedulerRef scheduler,
	int family,
	int port,
	bool isMulticast) :
	UdpListener(family, port, isMulticast)
{
	this->scheduler= scheduler;
}

SimpleListener::~SimpleListener()
{
}

void SimpleListener::handlePacket(
	char const *data,
	socklen_t dataLen,
	char const *address)
{
	bool validName= true;
	for (socklen_t i= 0; validName && (i < dataLen); i++) {
		if (!isalnum(data[i]) && (data[i] != '.') && (data[i] != ':')) {
			Log::log(LOG_WARNING,
				"Invalid ASCII in packet at position %d", i);

			validName= false;
		}
	}

	if (validName) {
		std::string key(data, dataLen);
		int timeout= 30;

		size_t offset= key.find(':');
		if (offset != std::string::npos) {
			std::string timeoutString= key.substr(offset + 1);
			key= key.substr(0, offset);
			timeout= atoi(timeoutString.c_str());
		}

		scheduler->receive(key.c_str(), timeout, address);
	}
}

