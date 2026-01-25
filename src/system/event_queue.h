
#ifndef SYSTEM_EVENT_QUEUE_H
#define SYSTEM_EVENT_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#define EVENT_QUEUE_CAPACITY 16

typedef enum
{
    EV_NONE = 0,
    EV_CLI_LINE,
} event_type_t;

typedef struct
{
    event_type_t type;
    const void *data; // opaque payload pointer owned by caller
    size_t data_len;  // length of payload in bytes
} event_t;

void event_queue_init(void);
bool event_queue_push(const event_t *event_entry);
bool event_queue_pop(event_t *event_out);
bool event_queue_is_empty(void);
bool event_queue_is_full(void);

#endif /* SYSTEM_EVENT_QUEUE_H */
