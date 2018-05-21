#ifndef CONFIG_H_
#define CONFIG_H_

#include "agent.h"

// Configuration file
#define CONFIG_FILE "config.json"
// Buffer to store in static memory the JSON configuration file.
// Preferably a power of 2.
#define CONFIG_FILE_MAX_LENGTH	8192

typedef enum _config_error_t {

	// Success, no errors encountered.
	// You are a lucky boy!
	CONFIG_SUCCESS = 0,

	// Can't open the config file. Possible reasons:
	// non existing file on the machine, no read permission for the file, input error
	CONFIG_FILE_CANT_OPEN = -1,

	// The config file is too large to be processed in the static memory allocated:
	// either use a smaller config file or recompile with a bigger static allocation :(
	CONFIG_FILE_TOO_LARGE = -2,

	// The config file is not a valid JSON file
	// In particular, the initial parsing has failed
	CONFIG_FILE_BAD_FORMAT = -3,

	// The topology specification is either missing or malformed
	CONFIG_BAD_TOPOLOGY = -4,

	// The obstacles specification is either missing or malformed
	CONFIG_BAD_OBSTACLES = -5,

	// A region specification is either missing or malformed
	CONFIG_BAD_REGION = -6,

	// An agent specification is either missing or malformed
	CONFIG_BAD_AGENT = -7,

	// An action specification is either missing or malformed
	CONFIG_BAD_ACTIONS = -8,

	// A user custom data specification for a region is either missing or malformed
	CONFIG_BAD_USER_REGION = -9,

	// A user custom data specification for an agent is either missing or malformed
	CONFIG_BAD_USER_AGENT = -10,

	// No idea on what's happening.
	// (currently only thrown if jsmn parser goes mad)
	CONFIG_UNKNOWN_ERROR = -11,

	CONFIG_ERROR_COUNT = 1 +(-CONFIG_UNKNOWN_ERROR)

} config_error_t;

/**
 * This function reads the config file and initializes the topology and the obstacles.
 * It is thread safe and it is guaranteed that initialization occurs only once per machine.
 * @return a proper error code, as explained by the config_error_t enum.
 * It is also guaranteed that on a single machine this function would always yield the same return code.
 */
config_error_t init_config(void);

/**
 * This function reads the config file loaded by init_config and instantiates a region with his agents
 * for the LP with id me.
 * Each lp can call this function with this id as argument even concurrently without errors, no more guarantees are given.
 * @param me the id of the LP calling this function
 * @param[out] result this pointer will point to a valid instantiated region_t, or will be set to NULL in case of failures
 * @return a proper error code, as explained by the config_error_t enum
 * Failure happens mostly if the json config file misses some fields.
 */
config_error_t get_region_config(unsigned int me, region_t **result);

/**
 * This function returns a printable null terminated string giving a brief textual description of the error code.
 * @param cfg_err the error code we are interested in.
 * @return a null terminated string, NULL in case of failure.
 * Failure happens if cfg_err is an invalid error code.
 */
const char* error_msg_config(config_error_t cfg_err);

#endif /* CONFIG_H_ */
