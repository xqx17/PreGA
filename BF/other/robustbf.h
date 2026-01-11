#ifndef ROBUSTBF_H
#define ROBUSTBF_H

#include <stddef.h>
#include <stdint.h>

// For simplicity, we will use an 8-bit fingerprint.
// You can change this to uint16_t for 16-bit fingerprints if needed.
typedef uint8_t fingerprint_t;

/**
 * @brief robustBF structure based on the paper "RobustBF: A high accuracy and 
 * memory efficient 2D Bloom filter" (arXiv:2106.04365).
 */
typedef struct {
    uint32_t rows;          // Number of rows in the 2D filter
    uint32_t cols;          // Number of columns in the 2D filter
    uint32_t num_hashes;    // Number of hash functions for columns (k)
    uint64_t elements;      // Count of inserted elements
    fingerprint_t* filter_data; // Pointer to the contiguous 2D filter data
} robustbf_t;

/**
 * @brief Creates and initializes a new robustBF.
 *
 * @param rows Number of rows (m).
 * @param cols Number of columns (n).
 * @param num_hashes Number of column hashes to use (k).
 * @return A pointer to the newly created robustbf_t, or NULL on failure.
 */
robustbf_t* robustbf_create(uint32_t rows, uint32_t cols, uint32_t num_hashes);

/**
 * @brief Destroys the robustBF and frees its memory.
 *
 * @param filter Pointer to the robustBF to be destroyed.
 */
void robustbf_destroy(robustbf_t* filter);

/**
 * @brief Adds an element to the robustBF.
 *
 * @param filter Pointer to the robustBF.
 * @param data Pointer to the data to be added.
 * @param len Length of the data in bytes.
 */
void robustbf_add(robustbf_t* filter, const void* data, size_t len);

/**
 * @brief Checks if an element is in the robustBF.
 *
 * @param filter Pointer to the robustBF.
 * @param data Pointer to the data to check.
 * @param len Length of the data in bytes.
 * @return 1 if the element is probably in the set, 0 if it is definitely not.
 */
int robustbf_check(const robustbf_t* filter, const void* data, size_t len);

#endif // ROBUSTBF_H