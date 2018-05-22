#ifndef ABM_LOGGER_H
#define ABM_LOGGER_H

#include <stdarg.h>

/**
 * UGLY but works
 * TODO: maybe implement a machine local log queue so at GVT
 * a coherent flow of logs can be printed instead of a rollbacked mess
 */

typedef enum _log_level_t {
	ABM_LOG_TRACE = 0,//!< ABM_LOG_TRACE
	ABM_LOG_DEBUG = 1,//!< ABM_LOG_DEBUG
	ABM_LOG_INFO = 2, //!< ABM_LOG_INFO
	ABM_LOG_WARN = 3, //!< ABM_LOG_WARN
	ABM_LOG_ERROR = 4,//!< ABM_LOG_ERROR
	ABM_LOG_FATAL = 5 //!< ABM_LOG_FATAL
} log_level_t;

// this way the software can programmatically change the logging level
extern unsigned int abm_log_level_prog;
// this is good because useless logs would be compiled away by dead code elimination
#define abm_log_level ABM_LOG_TRACE
// we wrap in a do while cycle to keep the ; syntax
#define abm_log(level, format_str, ...) do{if(level >= abm_log_level) if(level >= abm_log_level_prog) event_logger(__FILE__, __LINE__, level, format_str, ##__VA_ARGS__);}while(0)

void event_logger(const char *file, int line, log_level_t level, const char *format_str, ...);

#endif /* ABM_LOGGER_H_ */
