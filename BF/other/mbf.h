#ifndef MBF_H
#define MBF_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Multidimensional Bloom Filter (mBF) structure.
 * Based on the paper: "LCMA: A Novel Lightweight Continuous Message Authentication for Cyber-Physical System"
 * This structure consists of 'v' 2-Dimensional Bloom Filters (2DBF).
 */
typedef struct {
    uint32_t v;      // Number of 2DBFs
    uint32_t a;      // Dimension 1 of each 2DBF
    uint32_t b;      // Dimension 2 of each 2DBF
    uint32_t z;      // Bit size of each cell in a 2DBF
    uint32_t p_z;    // Largest prime number not exceeding z
    uint64_t elements; // Number of elements inserted
    uint32_t* filter_data; // Pointer to the contiguous memory block for all filters
} mbf_t;

/**
 * @brief Creates and initializes a new mBF.
 *
 * @param v Number of 2-dimensional filters.
 * @param a Dimension 'a' of each filter.
 * @param b Dimension 'b' of each filter.
 * @param z Bit size of each cell (e.g., 8, 16, 32). Should be <= 32 in this implementation.
 * @return A pointer to the newly created mbf_t, or NULL on failure.
 */
mbf_t* mbf_create(uint32_t v, uint32_t a, uint32_t b, uint32_t z);

/**
 * @brief Destroys the mBF and frees its memory.
 *
 * @param filter Pointer to the mBF to be destroyed.
 */
void mbf_destroy(mbf_t* filter);

/**
 * @brief Inserts data into the mBF.
 * This function implements Algorithm 1 from the source paper.
 *
 * @param filter Pointer to the mBF.
 * @param data Pointer to the data to be inserted.
 * @param len Length of the data in bytes.
 * @param seed0 Initial random seed for the first hash operation.
 */
void mbf_insert(mbf_t* filter, const void* data, size_t len, uint32_t seed0);

/**
 * @brief Checks for the existence of data in the mBF.
 *
 * @param filter Pointer to the mBF.
 * @param data Pointer to the data to check.
 * @param len Length of the data in bytes.
 * @param seed0 The same initial random seed used for insertion.
 * @return 1 if the element is probably in the set, 0 if it is definitely not.
 */
int mbf_check(const mbf_t* filter, const void* data, size_t len, uint32_t seed0);

#endif // MBF_H