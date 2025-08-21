/**
 * @file axidma_simple_transfer.c
 * @author Based on axidma_benchmark.c by Brandon Perez and Jared Choi
 *
 * This program sends a single 32-bit hexadecimal value through an AXI DMA
 * loopback system and prints the value received.
 *
 * It takes one command-line argument: the 32-bit hex value to send.
 *
 * Example Usage:
 * ./axidma_simple_transfer 0x12345678
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include "libaxidma.h"

int main(int argc, char **argv)
{
    uint32_t tx_val;
    uint32_t *tx_buf;
    uint32_t *rx_buf;
    int rc;
    axidma_dev_t axidma_dev;
    const array_t *tx_chans, *rx_chans;
    int tx_channel, rx_channel;

    // --- 1. Argument Parsing ---
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <32-bit hex value>\n", argv[0]);
        fprintf(stderr, "Example: %s 0xDEADBEEF\n", argv[0]);
        return 1;
    }

    // Convert the command-line string to an unsigned 32-bit integer
    tx_val = (uint32_t)strtoul(argv[1], NULL, 0);

    // --- 2. AXI DMA Initialization ---
    axidma_dev = axidma_init();
    if (axidma_dev == NULL) {
        fprintf(stderr, "Error: Failed to initialize the AXI DMA device.\n");
        return 1;
    }

    // --- 3. Buffer Allocation ---
    // Allocate a 4-byte (32-bit) buffer for transmit and receive
    tx_buf = axidma_malloc(axidma_dev, sizeof(uint32_t));
    if (tx_buf == NULL) {
        perror("Error: Unable to allocate transmit buffer from AXI DMA device");
        rc = -1;
        goto destroy_axidma;
    }

    rx_buf = axidma_malloc(axidma_dev, sizeof(uint32_t));
    if (rx_buf == NULL) {
        perror("Error: Unable to allocate receive buffer from AXI DMA device");
        rc = -1;
        goto free_tx_buf;
    }

    // --- 4. Channel Setup ---
    // Get the first available transmit and receive channels
    tx_chans = axidma_get_dma_tx(axidma_dev);
    if (tx_chans->len < 1) {
        fprintf(stderr, "Error: No transmit channels were found.\n");
        rc = -ENODEV;
        goto free_rx_buf;
    }

    rx_chans = axidma_get_dma_rx(axidma_dev);
    if (rx_chans->len < 1) {
        fprintf(stderr, "Error: No receive channels were found.\n");
        rc = -ENODEV;
        goto free_rx_buf;
    }

    // Use the first channel of each type
    tx_channel = tx_chans->data[0];
    rx_channel = rx_chans->data[0];

    // --- 5. Data Preparation and Transfer ---
    // Place the value from the command line into the transmit buffer
    *tx_buf = tx_val;
    
    // Initialize receive buffer to a known value to ensure it gets overwritten
    *rx_buf = 0;

    printf("Sending:  0x%08x\n", *tx_buf);

    // Perform the two-way transfer
    rc = axidma_twoway_transfer(axidma_dev, tx_channel, rx_buf, sizeof(uint32_t), NULL,
                                rx_channel, tx_buf, sizeof(uint32_t), NULL, true);
    if (rc < 0) {
        fprintf(stderr, "Error: DMA transfer failed.\n");
        goto free_rx_buf;
    }

    // --- 6. Print Result ---
    printf("Received: 0x%08x\n", *tx_buf);

    // --- 7. Cleanup ---
free_rx_buf:
    axidma_free(axidma_dev, rx_buf, sizeof(uint32_t));
free_tx_buf:
    axidma_free(axidma_dev, tx_buf, sizeof(uint32_t));
destroy_axidma:
    axidma_destroy(axidma_dev);

    return (rc == 0) ? 0 : 1;
}
