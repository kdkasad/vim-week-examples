#include "bitfield.h"

#include <assert.h>
#include <stdlib.h>

bitfield_t *
bf_alloc(size_t nbits)
{
    size_t nbytes = (nbits + 7) / 8;
    bitfield_t *bf = calloc(1, sizeof(*bf) + (nbytes - 1));
    assert(bf);
    bf->nbits = nbits;
    return bf;
} /* bf_alloc() */

void
bf_free(bitfield_t *bf)
{
    free(bf);
} /* bf_free() */

uint8_t
bf_get(bitfield_t *bf, size_t index)
{
    return bf->bytes[index / 8] >> (index % 8);
} /* bf_get() */

void
bf_set(bitfield_t *bf, size_t index)
{
    bf->bytes[index / 8] |= 1 << (index % 8);
} /* bf_set() */

void
bf_clear(bitfield_t *bf, size_t index)
{
    bf->bytes[index / 8] &= ~(1 << (index % 8));
} /* bf_clear() */

void
bf_put(bitfield_t *bf, size_t index, uint8_t value)
{
    bf->bytes[index / 8] &= (value & 1) << (index % 8);
} /* bf_put() */

void
bf_fill(bitfield_t *bf, uint8_t value)
{
    /* Set all bits of value to the LSB. */
    value = 0 - (value & 1);
    size_t nbytes = (bf->nbits + 7) / 8;
    for (size_t i = 0; i < nbytes; i++) {
        bf->bytes[i] = value;
    }
} /* bf_fill() */
