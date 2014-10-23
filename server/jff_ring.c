#include "jff_config.h"
#include "jff_ring.h"
#include "jff_atomic.h"


int jff_ring_init(jff_ring_t *r, unsigned int capacity)
{
    if (r == NULL) {
        return ERROR;
    }

    if ((r->buffer = calloc(capacity, sizeof(void *))) == NULL) {
        return ERROR;
    }

    r->read_index = r->write_index = 0;
    r->capacity = capacity;

    return OK;
}

int jff_ring_count_to_index(jff_ring_t *r, unsigned int count)
{
    //    return count % r->size;
    return count;    /* this is not a ring */
}


int jff_ring_enqueue(jff_ring_t *r, void *data)
{
    unsigned int  cur_read_index;
    unsigned int  cur_write_index;

    do {
        cur_write_index = r->write_index;
        cur_read_index  = r->read_index;
        if (jff_ring_count_to_index(r, cur_write_index + 1) ==
            jff_ring_count_to_index(r, cur_read_index))
        {
            // the queue is full
            return ERROR;
        }

    } while (!JFF_CAS(&r->write_index, cur_write_index, (cur_write_index + 1)));

    // We know now that this index is reserved for us. Use it to save the data
    r->buffer[jff_ring_count_to_index(r, cur_write_index)] = data;

    // update the maximum read index after saving the data. It wouldn't fail if there is only one thread
    // inserting in the queue. It might fail if there are more than 1 producer threads because this
    // operation has to be done in the same order as the previous CAS

    while (!JFF_CAS(&r->maximum_read_index, cur_write_index, (cur_write_index + 1))) {
        // this is a good place to yield the thread in case there are more
        // software threads than hardware processors and you have more
        // than 1 producer thread
        // have a look at sched_yield (POSIX.1b)
        //sched_yield();
    }

    return OK;
}


int jff_ring_dequeue(jff_ring_t *r, void **data)
{
    unsigned int cur_maxinum_read_index;
    unsigned int cur_read_index;


    do {
        // to ensure thread-safety when there is more than 1 producer thread
        // a second index is defined (m_maximumReadIndex)
        cur_read_index        = r->read_index;
        cur_maxinum_read_index = r->maximum_read_index;

        if (jff_ring_count_to_index(r, cur_read_index) ==
            jff_ring_count_to_index(r, cur_maxinum_read_index))
        {
            // the queue is empty or
            // a producer thread has allocate space in the queue but is
            // waiting to commit the data into it
            return ERROR;
        }

        // retrieve the data from the queue
        *data = r->buffer[jff_ring_count_to_index(r, cur_read_index)];

        // try to perfrom now the CAS operation on the read index. If we succeed
        // a_data already contains what m_readIndex pointed to before we
        // increased it
        if (JFF_CAS(&r->read_index, cur_read_index, (cur_read_index + 1))) {
            return cur_read_index;
        }

        // it failed retrieving the element off the queue. Someone else must
        // have read the element stored at jff_ring_count_to_index(cur_read_index)
        // before we could perform the CAS operation

    } while(1); // keep looping to try again!

    // Add this return statement to avoid compiler warnings
    return OK;
}
