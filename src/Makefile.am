AM_CXXFLAGS = -g -std=c++11 -pthread -Wall -fno-strict-aliasing \
	-Wno-deprecated-declarations \
	-DSCRIPTDIR=\"$(libexecdir)\"

bin_PROGRAMS = timeoutd

timeoutd_SOURCES = \
	Entry.cpp \
	Scheduler.cpp \
	Worker.cpp \
	Multicast.cpp \
	Listener.cpp \
	UdpListener.cpp \
	SimpleListener.cpp \
	SignedListener.cpp \
	Sender.cpp \
	UdpSender.cpp \
	SimpleSender.cpp \
	SignedSender.cpp \
	Log.cpp \
	OpensslMagic.cpp \
	main.cpp

timeoutd_LDFLAGS = -pthread
timeoutd_LDADD = -lcrypto -lssl

