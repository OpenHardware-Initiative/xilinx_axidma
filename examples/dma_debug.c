/**
 * @file dma_debug.c
 * A minimal, focused AXI DMA test program for debugging.
 *
 * This program performs a single, simple DMA transfer with verbose output
 * to help diagnose hardware or driver issues. It is stripped of all
 * non-essential features like command-line parsing and file I/O.
 *
 * It performs these steps:
 * 1. Allocates a transmit (Tx) and receive (Rx) buffer.
 * 2. Fills both with known, distinct patterns.
 * 3. Prints the initial state of both buffers.
 * 4. Executes a single two-way DMA transfer.
 * 5. Prints the final state of both buffers.
 * 6. Performs an automated verification to check for errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include "libaxidma.h"

// --- Configuration ---
// Set the size of the transfer in bytes. 64 is a good small number for testing.
#define TRANSFER_SIZE 64

// The pattern that we fill into the buffers for verification
#define TEST_PATTERN(i) ((int)(0x1234ACDE ^ (i)))

// --- Helper Functions from axidma_benchmark.c ---

/* Fills the transmit and receive buffers with known patterns. */
static void init_data(void *tx_buf, void *rx_buf, size_t size) {
    int *tx_buf_int = (int *)tx_buf;
    int *rx_buf_int = (int *)rx_buf;
    size_t i;

    // Fill Tx buffer with a pattern
    for (i = 0; i < size / sizeof(int); i++) {
        tx_buf_int[i] = TEST_PATTERN(i);
    }

    // Fill Rx buffer with a *different* pattern to ensure it gets overwritten
    for (i = 0; i < size / sizeof(int); i++) {
    //    rx_buf_int[i] = TEST_PATTERN(i + size);
			rx_buf_int[i] = (int)(0xAAAAAAAA);
    }
}

/* Verifies the buffer contents after a transfer. */
static int verify_data(void *tx_buf, void *rx_buf, size_t size) {
    int *tx_buf_int = (int *)tx_buf;
    int *rx_buf_int = (int *)rx_buf;
    size_t i;
    size_t rx_data_same = 0;

    // 1. Check if the transmit buffer was corrupted
    for (i = 0; i < size / sizeof(int); i++) {
        if (tx_buf_int[i] != TEST_PATTERN(i)) {
            fprintf(stderr, "Error: Transmit buffer was overwritten at word %zu!\n", i);
            fprintf(stderr, "Expected 0x%08X, found 0x%08X.\n", TEST_PATTERN(i), tx_buf_int[i]);
            return -1;
        }
    }

    // 2. Check if the receive buffer was updated
    for (i = 0; i < size / sizeof(int); i++) {
        if (rx_buf_int[i] == TEST_PATTERN(i + size)) {
            rx_data_same++;
        }
    }

    if (rx_data_same == (size / sizeof(int))) {
        fprintf(stderr, "Error: Receive buffer was not updated.\n");
        return -1;
    }

    return 0;
}

/* Helper to print the first few words of a buffer. */
void print_buf(const char *name, void *buf, size_t size) {
    int *buf_int = (int *)buf;
    size_t i;
    size_t words_to_print = (size / sizeof(int) < 16) ? (size / sizeof(int)) : 16;
    
    printf("--- %s Contents (first %zu words) ---\n", name, words_to_print);
    for (i = 0; i < words_to_print; i++) {
        printf("%s[%zu] = 0x%08X\n", name, i, buf_int[i]);
    }
    printf("--------------------------------------------\n");
}


// --- Main Program ---

int main(void) {
    int rc = 0;
    axidma_dev_t axidma_dev;
    void *tx_buf = NULL;
    void *rx_buf = NULL;
    const array_t *tx_chans, *rx_chans;
    int tx_channel, rx_channel;

    // Initialize the AXI DMA device
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Failed to initialize the AXI DMA device.\n");
        return 1;
    }

    // Get DMA channels
    tx_chans = axidma_get_dma_tx(axidma_dev);
    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (tx_chans->len < 1 || rx_chans->len < 1) {
        fprintf(stderr, "Error: Could not find DMA channels.\n");
        rc = -ENODEV;
        goto cleanup;
    }
    tx_channel = tx_chans->data[0];
    rx_channel = rx_chans->data[0];
    printf("Using Tx channel %d and Rx channel %d.\n", tx_channel, rx_channel);
    printf("Transfer size is %d bytes.\n\n", TRANSFER_SIZE);

    // Allocate DMA-capable memory
    tx_buf = axidma_malloc(axidma_dev, TRANSFER_SIZE);
    rx_buf = axidma_malloc(axidma_dev, TRANSFER_SIZE);
    if (tx_buf == NULL || rx_buf == NULL) {
        fprintf(stderr, "Failed to allocate DMA buffers.\n");
        rc = -ENOMEM;
        goto cleanup;
    }
    
    // STEP 1: Initialize buffers and print their initial state
    printf("--- BEFORE TRANSFER ---\n");
    init_data(tx_buf, rx_buf, TRANSFER_SIZE);
    print_buf("Tx Buffer", tx_buf, TRANSFER_SIZE);
    print_buf("Rx Buffer", rx_buf, TRANSFER_SIZE);
    printf("\n");

    // STEP 2: Perform the DMA transfer
    printf("--- PERFORMING DMA TRANSFER ---\n");
    rc = axidma_twoway_transfer(axidma_dev, tx_channel, tx_buf, TRANSFER_SIZE, NULL,
                                rx_channel, rx_buf, TRANSFER_SIZE, NULL, true);
    if (rc < 0) {
        fprintf(stderr, "DMA transfer failed with error code %d\n", rc);
        goto cleanup;
    }
    printf("DMA transfer completed.\n\n");
    
    // STEP 3: Print the buffer state after the transfer
    printf("--- AFTER TRANSFER ---\n");
    print_buf("Tx Buffer", tx_buf, TRANSFER_SIZE);
    print_buf("Rx Buffer", rx_buf, TRANSFER_SIZE);
    printf("\n");

    // STEP 4: Verify the results
    printf("--- VERIFYING DATA ---\n");
    if (verify_data(tx_buf, rx_buf, TRANSFER_SIZE) < 0) {
        printf("Verification FAILED.\n");
        rc = -1; // Indicate failure
    } else {
        printf("Verification PASSED.\n");
    }

cleanup:
    // Free the DMA buffers and destroy the device
    if (tx_buf) axidma_free(axidma_dev, tx_buf, TRANSFER_SIZE);
    if (rx_buf) axidma_free(axidma_dev, rx_buf, TRANSFER_SIZE);
    axidma_destroy(axidma_dev);
    
    return (rc == 0) ? 0 : 1;
}
