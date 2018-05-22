#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

unsigned int abm_log_level_prog = ABM_LOG_FATAL;

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char *const level_names[] = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };

// FANCY COLOURS YAY! (who cares if this stuff is not portable)
static const char *const level_colors[] = { "\x1b[34m", "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[35m", "\x1b[31m" };

void event_logger(const char *file, int line, log_level_t level, const char *format_str, ...) {

	time_t rawtime;
	struct tm * timeinfo;
	char date_buffer[10];
	char pos_buffer[10];
	

	memcpy(pos_buffer, file, 3*sizeof(char));
	strcpy(pos_buffer + 3, "~.c: ");
	if(line >= 100)
		pos_buffer[7] = '\0';

	time(&rawtime);

	/**
	 * This function is contended, moreover various function here aren't thread safe
	 * thus this is performance killing but necessary. Oh well, logs can be disabled.
	 */
	pthread_mutex_lock(&log_mutex);

	timeinfo = localtime(&rawtime);

	strftime(date_buffer, sizeof(date_buffer), "%T", timeinfo);
	date_buffer[9] = '\0';

	fprintf(stderr, "%s %s%-5s\x1b[0m \x1b[90m%s%d:\x1b[0m \x1b[37m",
			date_buffer, level_colors[level], level_names[level], pos_buffer, line);
	va_list args;
	va_start(args, format_str);
	vfprintf(stderr, format_str, args);
	va_end(args);

	fprintf(stderr, "\x1b[0m\n");

	pthread_mutex_unlock(&log_mutex);

}
