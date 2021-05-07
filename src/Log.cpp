#include "system.h"
#include "Log.h"

#define FORMAT_BUFFER_LEN 1023


std::mutex Log::mutex;
int Log::fd= 2;
int Log::logLevel= LOG_INFO;

void Log::open(char const *file)
{
	std::lock_guard<std::mutex> lock(mutex);

	if (fd != 2) {
		close(fd);
	}

	if (strcmp(file, "stderr") == 0) {
		fd= 2;
	} else {
		fd= ::open(file,
			O_CREAT | O_APPEND | O_CLOEXEC | O_WRONLY | O_CLOEXEC,
			S_IRUSR | S_IWUSR | S_IRGRP);
		if (fd == -1) {
			fprintf(stderr, "Error opening log file %s: %d: %s\n",
				file, errno, strerror(errno));
			fd= 2;
		}
	}
}

void Log::setLogLevel(int level)
{
	std::lock_guard<std::mutex> lock(mutex);
	logLevel= level;
}

void Log::log(int level, char const *format, ...)
{
	int localLogLevel= 0;
	{
		std::lock_guard<std::mutex> lock(mutex);
		localLogLevel= logLevel;
	}

	if (level >= localLogLevel) {
		char buffer[FORMAT_BUFFER_LEN + 1];
		int bufferLen;

		struct timeval now;
		gettimeofday(&now, NULL);

		struct tm *bt= localtime(&now.tv_sec);

		sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d ",
			bt->tm_year + 1900, bt->tm_mon + 1, bt->tm_mday,
			bt->tm_hour, bt->tm_min, bt->tm_sec);

		char *end= &buffer[strlen(buffer)];

		switch (level) {
		case LOG_DEBUG:
			strcpy(end, "[DEBUG]");
			break;
		case LOG_INFO:
			strcpy(end, "[INFO]");
			break;
		case LOG_WARNING:
			strcpy(end, "[WARNING]");
			break;
		case LOG_ERROR:
			strcpy(end, "[ERROR]");
			break;
		case LOG_CRITICAL:
			strcpy(end, "[ASSERT]");
			break;

		default:
			strcpy(end, "[WTF]");
			break;
		}

		va_list args;
		va_start(args, format);

		end= &buffer[strlen(buffer)];
		*(end++)= ' ';

		bufferLen= FORMAT_BUFFER_LEN - (end - buffer) - (1 /* for NL */);
		vsnprintf(end, bufferLen, format, args);
		bufferLen= strlen(buffer);
		buffer[bufferLen++]= '\n';
		buffer[bufferLen]= '\0';
		va_end(args);

		std::lock_guard<std::mutex> lock(mutex);

		int nwrote= write(fd, buffer, bufferLen);

		if (nwrote == -1) {
			fprintf(stderr,
				"LOG FAILURE: %d: %s\n", errno, strerror(errno));
		} else if (nwrote != bufferLen) {
			fprintf(stderr,
				"LOG INCOMPLETE: %d/%d written\n", nwrote, bufferLen);
		}
	}
}

