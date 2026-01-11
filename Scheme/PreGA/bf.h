#ifndef TBF_H
#define TBF_H

#include <stdint.h>
#include <stddef.h> 

// =================================================================
//                 Compile-time Configuration
// =================================================================

// Maximum number of hash functions to use.
#define HASH_COUNT 3

// Maximum size of the Bloom Filter's bit array.
#define BLOOM_SIZE 1024

// Maximum number of time windows (slices) the filter can have.
#define TIME_WINDOW_SIZE 5

// =================================================================
//                      Type Definitions
// =================================================================

// Define the counter type. uint8_t is memory-efficient but can saturate at 255.
// Change to uint16_t or uint32_t for applications with very frequent insertions.
typedef uint8_t counter_t;

// Forward declaration of the main structure
typedef struct TimeBloomFilter TimeBloomFilter;

// =================================================================
//                      Public API Functions
// =================================================================

/**
 * @brief Initializes the global Time Bloom Filter instance.
 *
 * This function must be called before any other library function. It configures
 * the filter with the specified parameters.
 *
 * @param bloom_size The size of each Bloom Filter column (should not exceed BLOOM_SIZE).
 * @param time_window_size The maximum number of time windows (should not exceed TIME_WINDOW_SIZE).
 * @param threshold_ratio The saturation ratio (0.0 to 1.0) to trigger window expansion.
 * @param time_range The number of seconds after which an inactive window is considered old and can be replaced.
 * @return A pointer to the initialized TimeBloomFilter instance.
 */
TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range);

/**
 * @brief Inserts an element into the Time Bloom Filter.
 *
 * Finds an available (non-saturated) time window and adds the element to it.
 * If all windows are saturated, it may try to expand the number of active windows.
 *
 * @param tbf Pointer to the TimeBloomFilter instance.
 * @param data Pointer to the data of the element to be inserted.
 * @param length The length of the data in bytes.
 * @return 0 on success, -1 on failure (e.g., filter is full and cannot expand).
 */
int insert_element(TimeBloomFilter* tbf, const void* data, size_t length);

/**
 * @brief Queries for the existence of an element in any of the active time windows.
 *
 * @param tbf Pointer to the TimeBloomFilter instance.
 * @param data Pointer to the data of the element to be queried.
 * @param length The length of the data in bytes.
 * @return 1 if the element is likely present, 0 if it is definitely not present.
 */
int query_element(TimeBloomFilter* tbf, const void* data, size_t length);

/**
 * @brief Removes one instance of an element from the filter.
 *
 * This function decrements the counters associated with the element in the
 * first time window where it is found.
 *
 * @param tbf Pointer to the TimeBloomFilter instance.
 * @param data Pointer to the data of the element to be removed.
 * @param length The length of the data in bytes.
 * @return 0 on success, -1 if the element was not found to be removed.
 */
int remove_element(TimeBloomFilter* tbf, const void* data, size_t length);

/**
 * @brief Manually checks the age of the time windows and slides/reduces them if necessary.
 *
 * This function is called automatically by insert, query, and remove, but can be
 * called manually in an idle loop or on a timer.
 *
 * @param tbf Pointer to the TimeBloomFilter instance.
 */
void check_and_update_time_window(TimeBloomFilter* tbf);

/**
 * @brief Prints the current status of the Time Bloom Filter.
 *
 * Depends on a backend for printf.
 *
 * @param tbf Pointer to the TimeBloomFilter instance.
 */
void print_bloom_filter_status(TimeBloomFilter* tbf);


// =================================================================
//              Internal Structures (Exposed in Header)
// =================================================================

// WCH Port: The custom time_t definition is no longer needed.
// We now use uint64_t directly to store CPU cycle counts.

typedef struct time_window
{
    // WCH Port: Changed from time_t to uint64_t to store the 64-bit CPU cycle count.
    uint64_t up_time;             // Last update timestamp (in CPU cycles).
    int size;                     // Current number of active windows
    int time_position_front;      // Index of the newest window
    int time_position_end;        // Index of the oldest window
    int current_time_position;    // Currently active window for insertions
} Time_Win;

// Main Time Bloom Filter structure
struct TimeBloomFilter {
    int time_window_size;         // Max number of windows (T)
    int is_expanded;              // Flag indicating if windows have been dynamically added
    int bloom_size;               // Size of each bloom filter (B)
    float threshold_ratio;        // Saturation threshold
    int time_range;               // Time in seconds to slide the window
    Time_Win tw;                  // Time window management struct
    counter_t filter[TIME_WINDOW_SIZE][BLOOM_SIZE + 1]; // The 2D filter array
};

#endif // TBF_H
