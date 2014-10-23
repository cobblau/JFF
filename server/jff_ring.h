#ifndef _JFF_RING_H
#define _JFF_RING_H

typedef struct jff_ring_s {
    unsigned int  write_index;
    unsigned int  read_index;
    unsigned int  maximum_read_index;

    unsigned int  capacity;
    void        **buffer;
} jff_ring_t;


int jff_ring_init(jff_ring_t *r, unsigned int capacity);
int jff_ring_count_to_index(jff_ring_t *r, unsigned int count);
int jff_ring_enqueue(jff_ring_t *r, void *data);
int jff_ring_dequeue(jff_ring_t *r, void **data);

#endif
