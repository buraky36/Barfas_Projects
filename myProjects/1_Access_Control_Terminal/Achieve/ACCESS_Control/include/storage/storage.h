#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>


/**
 * @brief Initialize SPIFFS file system
 * @return true if successful, false otherwise
 */
bool storage_init(void);

/**
 * @brief Formats the storage partition
 */
void storage_format(void);

/**
 * @brief Save a single log entry string
 * @param log_entry Formatted string to append to log file
 */
bool storage_append_log(const char *log_entry);

/**
 * @brief Check if a card UID exists in the local database
 * @param card_uid Card to search for
 * @return true if access should be granted based on local DB
 */
bool storage_db_check_card(uint32_t card_uid);

/**
 * @brief Save a string to NVS storage
 * @param key The key to save
 * @param value The null-terminated string value
 * @return true if successful
 */
bool storage_write_string(const char *key, const char *value);

#endif // STORAGE_H
