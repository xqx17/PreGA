#ifndef EBF_H
#define EBF_H

#include <stddef.h>
#include <stdint.h>

typedef uint8_t fingerprint_t;

/**
 * @brief eBF (Enhanced Bloom Filter) structure based on the paper
 * "EBF: An enhanced Bloom filter for intrusion detection in IoT" (Journal of Big Data, 2023).
 */
typedef struct {
    uint32_t rows;          // Number of rows in the 2D filter
    uint32_t cols;          // Number of columns in the 2D filter
    uint32_t num_hashes;    // Number of hash functions (k)
    uint32_t max_probes;    // Maximum number of probes on collision
    uint64_t elements;      // Count of inserted elements
    fingerprint_t* filter_data; // Pointer to the contiguous 2D filter data
} ebf_t;

/**
 * @brief Creates and initializes a new eBF.
 *
 * @param rows Number of rows (m).
 * @param cols Number of columns (n).
 * @param num_hashes Number of hash functions to use (k).
 * @param max_probes The maximum number of slots to check for an empty cell on collision.
 * @return A pointer to the newly created ebf_t, or NULL on failure.
 */
ebf_t* ebf_create(uint32_t rows, uint32_t cols, uint32_t num_hashes, uint32_t max_probes);

/**
 * @brief Destroys the eBF and frees its memory.
 *
 * @param filter Pointer to the eBF to be destroyed.
 */
void ebf_destroy(ebf_t* filter);

/**
 * @brief Adds an element to the eBF. Handles collisions by probing.
 *
 * @param filter Pointer to the eBF.
 * @param data Pointer to the data to be added.
 * @param len Length of the data in bytes.
 * @return 0 on success, -1 if the filter is too full to find a slot for all hashes.
 */
int ebf_add(ebf_t* filter, const void* data, size_t len);

/**
 * @brief Checks if an element is in the eBF. Follows the same probing sequence as add.
 *
 * @param filter Pointer to the eBF.
 * @param data Pointer to the data to check.
 * @param len Length of the data in bytes.
 * @return 1 if the element is probably in the set, 0 if it is definitely not.
 */
int ebf_check(const ebf_t* filter, const void* data, size_t len);

#endif // EBF_H
