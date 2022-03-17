/**
 *  header for received messages
 *  message_type can be f (for file transfer) or chat (for chat - not our business)
 *  message_size is the size of the next read from the socket
 *
 */


#include <stdint.h>

typedef struct
{
    char message_type;
    uint32_t message_size;
} message_header;
