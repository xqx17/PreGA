/*
 * MIRACL Core Exponentiation Timing Example for G1, G2, GT
 * Curve: BN254 (Assumed compiled in MIRACL Core)
 * Platform: CH32V307VCT6
 *
 * Measures time for:
 * - G1: k * P (scalar multiplication)
 * - G2: k * Q (scalar multiplication)
 * - GT: gT^k (power operation)
 * Requires pre-compiled MIRACL Core library (with BN254) linked.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- WCH Platform Headers ---
#include "ch32v30x.h"
#include "debug.h"
#include "ch32v30x_rng.h"
#include "risc_time.h"    // For Get_counter() and systick_Init()

// --- MIRACL Core Headers (Assuming BN254) ---
#include "core.h"
#include "config_curve_BN254.h"
#include "config_field_BN254.h"
#include "ecp_BN254.h"
#include "ecp2_BN254.h"
#include "pair_BN254.h"
#include "fp12_BN254.h"
#include "randapi.h"
// --- End MIRACL Core Headers ---

#ifndef MODBYTES_BN254
#define MODBYTES_BN254 ((MODBITS_BN254+7)/8)
#endif

// --- Forward declarations ---
void seed_csprng_from_trng(csprng *prng, size_t seed_len);
void print_big(const char *label, BIG_256_28 k); // Use correct BIG type for BN254
void print_ecp(const char *label, ECP_BN254 *P);
void print_ecp2(const char *label, ECP2_BN254 *Q);
// void print_fp12(const char *label, FP12_BN254 *gT); // Printing FP12 is complex, often omitted

// Helper function to seed csprng from hardware TRNG
void seed_csprng_from_trng(csprng *prng, size_t seed_len) {
    #define MAX_SEED_LEN 128
    if (seed_len > MAX_SEED_LEN) {
        printf("ERROR: Seed length %u too large, max is %d\r\n", (unsigned)seed_len, MAX_SEED_LEN);
        seed_len = MAX_SEED_LEN;
    }
    char raw_seed[MAX_SEED_LEN];
    octet RAW_SEED = {0, seed_len, raw_seed};

    for (unsigned i = 0; i < seed_len; i += 4) {
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);
        uint32_t random_word = RNG_GetRandomNumber();
        unsigned bytes_to_copy = (seed_len - i < 4) ? (seed_len - i) : 4;
        memcpy(RAW_SEED.val + i, &random_word, bytes_to_copy);
    }
    RAW_SEED.len = seed_len;
    CREATE_CSPRNG(prng, &RAW_SEED);
}

// Helper to print big numbers (scalars) - Use correct BIG type for BN254
void print_big(const char *label, BIG_256_28 k) {
    char k_str[MODBYTES_BN254 * 2 + 1];
    char k_bytes[MODBYTES_BN254];
    octet K_OCT = {0, MODBYTES_BN254, k_bytes};
    K_OCT.len = MODBYTES_BN254;
    BIG_256_28_toBytes(K_OCT.val, k);
    OCT_toHex(&K_OCT, k_str);
    printf("%s: 0x%s\r\n", label, k_str);
}

// Helper to print ECP points (G1)
void print_ecp(const char *label, ECP_BN254 *P) {
    char p_str[1 + 2 * MODBYTES_BN254];
    octet P_OCT = {0, sizeof(p_str), p_str};
    ECP_BN254_toOctet(&P_OCT, P, false); // Uncompressed
    printf("%s: 0x", label);
    // Basic hex print function (replace with OCT_output_variable_len if available)
    for(int i=0; i<P_OCT.len; i++) printf("%02x", P_OCT.val[i]);
    printf("\r\n");
}

// Helper to print ECP2 points (G2)
void print_ecp2(const char *label, ECP2_BN254 *Q) {
    // G2 points are typically larger (e.g., 4*MODBYTES for uncompressed BN)
    char q_str[1 + 4 * MODBYTES_BN254];
    octet Q_OCT = {0, sizeof(q_str), q_str};
    ECP2_BN254_toOctet(&Q_OCT, Q, false); // Uncompressed
    printf("%s: 0x", label);
    for(int i=0; i<Q_OCT.len; i++) printf("%02x", Q_OCT.val[i]);
    printf("\r\n");
}


int main()
{
    uint32_t start_time, end_time, time_g1, time_g2, time_gt;

    // --- CH32V Initialization ---
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
    USART_Printf_Init(115200);
    systick_Init();
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
    while(RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);
    // --- End CH3V Initialization ---

    printf("\r\nMIRACL Core Exponentiation Timing (BN254)\r\n");
    printf("Platform: CH32V307VCT6\r\n");
    printf("============================================\r\n");
    printf("WARNING: Operations might be very slow!\r\n");

    // --- Initialize CSPRNG ---
    csprng rng_state;
    printf("Seeding CSPRNG...\r\n");
    seed_csprng_from_trng(&rng_state, 100);
    printf("CSPRNG Seeded.\r\n");

    // --- Declare MIRACL Core variables ---
    // Use static allocation for large structures
    static BIG_256_28 k;      // Scalar exponent (Use correct BIG type for BN254)
    static ECP_BN254 P, R_G1; // Points in G1
    static ECP2_BN254 Q, R_G2;// Points in G2
    static FP12_BN254 gT, R_GT;// Elements in GT
    static BIG_256_28 curve_order; // Curve order

    // --- Get Generator Points P (G1) and Q (G2) ---
    printf("Getting generator points...\r\n");
    if (!ECP_BN254_generator(&P)) {
         printf("ERROR: Failed to get G1 generator!\r\n"); while(1);
    }
    if (!ECP2_BN254_generator(&Q)) {
         printf("ERROR: Failed to get G2 generator!\r\n"); while(1);
    }
    print_ecp("G1 Generator P", &P);
    print_ecp2("G2 Generator Q", &Q);

    // --- Get Curve Order ---
    // Find the correct function or constant for curve order in MIRACL Core BN254
    // It might be predefined constant `CURVE_Order_BN254` or a function call
    // ECP_BN254_get_ord(curve_order); // Assuming this function exists for BN254
    // If not, you might need to find the constant:
     BIG_256_28_rcopy(curve_order, CURVE_Order_BN254);

    // --- Generate Random Scalar k ---
    printf("Generating random scalar k...\r\n");
    BIG_256_28_randomnum(k, curve_order, &rng_state);
    if (BIG_256_28_iszilch(k)) BIG_256_28_inc(k, 1); // Avoid k=0
    print_big("Scalar k", k);

    // --- 1. Time Exponentiation in G1 (Scalar Multiplication) ---
    printf("\r\nCalculating R_G1 = k * P (Exponentiation in G1)...\r\n");
    start_time = Get_counter();
    ECP_BN254_copy(&R_G1, &P);
    ECP_BN254_mul(&R_G1, k);
    //ECP_BN254_mul(&R_G1, k, &P); // Calculate k * P
    end_time = Get_counter();
    time_g1 = end_time - start_time;
    printf("G1 calculation finished.\r\n");
    print_ecp("Result R_G1", &R_G1);
    printf("Time Cost (G1): %lu us\r\n", time_g1);

    // --- 2. Time Exponentiation in G2 (Scalar Multiplication) ---
    printf("\r\nCalculating R_G2 = k * Q (Exponentiation in G2)...\r\n");
    start_time = Get_counter();
    ECP2_BN254_mul(&R_G2, k); // Calculate k * Q (G2 generator)
    end_time = Get_counter();
    time_g2 = end_time - start_time;
    printf("G2 calculation finished.\r\n");
    print_ecp2("Result R_G2", &R_G2);
    printf("Time Cost (G2): %lu us\r\n", time_g2);

    // --- 3. Time Exponentiation in GT (Power Operation) ---
    // First, generate an element gT in GT (e.g., by pairing generators)
    printf("\r\nGenerating element gT = e(P, Q) in GT...\r\n");
    start_time = Get_counter();
    PAIR_BN254_ate(&gT, &Q, &P);
    PAIR_BN254_fexp(&gT);
    end_time = Get_counter();
    printf("Generated gT in %lu us.\r\n", end_time - start_time);
    if (FP12_BN254_isunity(&gT)) printf("Warning: gT is unity!\r\n");

    printf("\r\nCalculating R_GT = gT ^ k (Exponentiation in GT)...\r\n");
    start_time = Get_counter();
    FP12_BN254_pow(&R_GT, &gT, k); // Calculate gT ^ k
    end_time = Get_counter();
    time_gt = end_time - start_time;
    printf("GT calculation finished.\r\n");
    // Printing R_GT is complex, just report time
    printf("Time Cost (GT): %lu us\r\n", time_gt);


    // --- Summary ---
    printf("\r\n--- Exponentiation Timing Summary (BN254) ---\r\n");
    printf("  G1 (k*P): %lu us\r\n", time_g1);
    printf("  G2 (k*Q): %lu us\r\n", time_g2);
    printf("  GT (gT^k): %lu us\r\n", time_gt);

    printf("\r\nTest finished.\r\n");

    while(1)
    {
        // Infinite loop
    }
}
