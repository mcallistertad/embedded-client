/*! \file libelg/crc32.c
 *  \brief from http://www.hackersdelight.org/hdcodetxt/crc.c.txt
 */

/* This is the basic CRC-32 calculation with some optimization but no
 * table lookup. The the byte reversal is avoided by shifting the crc reg
 * right instead of left and by using a reversed 32-bit word to represent
 * the polynomial.
 * When compiled to Cyclops with GCC, this function executes in 8 + 72n
 * instructions, where n is the number of bytes in the input message. It
 * should be doable in 4 + 61n instructions.
 * If the inner loop is strung out (approx. 5*8 = 40 instructions),
 * it would take about 6 + 46n instructions.
 */

/*! \brief basic CRC-32 calculation
 *  @param message pointer to bytes over which to calculate crc
 *  @param msgsize number of bytes
 *
 *  @returns crc32
 */
unsigned int sky_crc32(unsigned char *message, unsigned msgsize)
{
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    while (i < msgsize) {
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--) { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return ~crc;
}
