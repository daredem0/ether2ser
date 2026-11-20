#include <stdint.h>
#include <string.h>

#include "drivers/mac_address.h"
#include "unity.h"

void test_mac_address_from_ipv4_encodes_unique_local_unicast_address(void)
{
    const uint8_t ipv4[IPV4_ADDRESS_LENGTH] = {192U, 168U, 29U, 20U};
    const uint8_t expected[MAC_ADDRESS_LENGTH] = {0x02U, 0xE2U, 192U, 168U, 29U, 20U};
    uint8_t mac[MAC_ADDRESS_LENGTH] = {0};

    mac_address_from_ipv4(ipv4, mac);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, mac, MAC_ADDRESS_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(0U, mac[0] & 0x01U);
    TEST_ASSERT_EQUAL_UINT8(0x02U, mac[0] & 0x02U);
}

void test_mac_address_from_ipv4_distinguishes_configured_board_ips(void)
{
    const uint8_t pico_a_ip[IPV4_ADDRESS_LENGTH] = {192U, 168U, 29U, 20U};
    const uint8_t pico_b_ip[IPV4_ADDRESS_LENGTH] = {192U, 168U, 30U, 20U};
    uint8_t pico_a_mac[MAC_ADDRESS_LENGTH] = {0};
    uint8_t pico_b_mac[MAC_ADDRESS_LENGTH] = {0};

    mac_address_from_ipv4(pico_a_ip, pico_a_mac);
    mac_address_from_ipv4(pico_b_ip, pico_b_mac);

    TEST_ASSERT_NOT_EQUAL(0, memcmp(pico_a_mac, pico_b_mac, MAC_ADDRESS_LENGTH));
}
