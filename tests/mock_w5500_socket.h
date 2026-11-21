#ifndef TESTS_MOCK_W5500_SOCKET_H
#define TESTS_MOCK_W5500_SOCKET_H

#include <stdint.h>

void mock_w5500_socket_reset(void);
void mock_w5500_socket_set_send_result(int32_t result);

#endif /* TESTS_MOCK_W5500_SOCKET_H */
