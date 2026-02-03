// Related headers
#include "persistent_config.h"

// Standard library headers
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "hardware/flash.h"
#include "hardware/sync.h"

// Project Headers
#include "system/common.h"

// Generated headers

// Reserve last 4KB sector for config
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define CONFIG_MAGIC 0xCAFEBABE

// Read: just cast the flash address
static const config_t* nonsafe_config_read(void)
{
    return (const config_t*)(XIP_BASE + FLASH_TARGET_OFFSET);
}

bool config_read(config_t* cfg)
{
    const config_t* tmp_cnfg = nonsafe_config_read();
    if (tmp_cnfg->magic != CONFIG_MAGIC)
    {
        return false;
    }
    memcpy(cfg, tmp_cnfg, sizeof(config_t));
    return true;
}

bool config_is_valid(void)
{
    const config_t* cfg = nonsafe_config_read();
    return cfg->magic == CONFIG_MAGIC;
}

void dump_config(void)
{
    const config_t* cfg = nonsafe_config_read();
    if (cfg->magic != CONFIG_MAGIC)
    {
        LOG_INFO("Config: invalid magic 0x%08" PRIX32 "\r\n", cfg->magic);
        return;
    }

    LOG_INFO("Config:\r\n");
    LOG_INFO("  magic=0x%08" PRIX32 " version=%" PRIu32 "\r\n", cfg->magic, cfg->version);
    LOG_INFO("  local=%u.%u.%u.%u:%u\r\n", cfg->local_config.ip_address[0],
             cfg->local_config.ip_address[1], cfg->local_config.ip_address[2],
             cfg->local_config.ip_address[3], cfg->local_config.port);
    LOG_INFO("  remote=%u.%u.%u.%u:%u\r\n", cfg->remote_config.ip_address[0],
             cfg->remote_config.ip_address[1], cfg->remote_config.ip_address[2],
             cfg->remote_config.ip_address[3], cfg->remote_config.port);
    LOG_INFO("  net: mac=%02X:%02X:%02X:%02X:%02X:%02X ip=%u.%u.%u.%u sn=%u.%u.%u.%u "
             "gw=%u.%u.%u.%u dns=%u.%u.%u.%u dhcp=%u\r\n",
             cfg->net_config.net_info.mac[0], cfg->net_config.net_info.mac[1],
             cfg->net_config.net_info.mac[2], cfg->net_config.net_info.mac[3],
             cfg->net_config.net_info.mac[4], cfg->net_config.net_info.mac[5],
             cfg->net_config.net_info.ip[0], cfg->net_config.net_info.ip[1],
             cfg->net_config.net_info.ip[2], cfg->net_config.net_info.ip[3],
             cfg->net_config.net_info.sn[0], cfg->net_config.net_info.sn[1],
             cfg->net_config.net_info.sn[2], cfg->net_config.net_info.sn[3],
             cfg->net_config.net_info.gw[0], cfg->net_config.net_info.gw[1],
             cfg->net_config.net_info.gw[2], cfg->net_config.net_info.gw[3],
             cfg->net_config.net_info.dns[0], cfg->net_config.net_info.dns[1],
             cfg->net_config.net_info.dns[2], cfg->net_config.net_info.dns[3],
             cfg->net_config.net_info.dhcp);
    LOG_INFO("  broadcast=%u.%u.%u.%u\r\n", cfg->net_config.broadcast_address[0],
             cfg->net_config.broadcast_address[1], cfg->net_config.broadcast_address[2],
             cfg->net_config.broadcast_address[3]);
    LOG_INFO("  v24: baud=%u txd_inv=%u txc_inv=%u cts_inv=%u rts_inv=%u dtr_inv=%u rxd_inv=%u "
             "rxc_inv=%u dcd_inv=%u\r\n",
             (unsigned)cfg->v24_config.baudrate,
             cfg->v24_config.polarities.tx_polarities.txd_inverted,
             cfg->v24_config.polarities.tx_polarities.txc_inverted,
             cfg->v24_config.polarities.tx_polarities.cts_inverted,
             cfg->v24_config.polarities.tx_polarities.rts_inverted,
             cfg->v24_config.polarities.tx_polarities.dtr_inverted,
             cfg->v24_config.polarities.rx_polarities.rxd_inverted,
             cfg->v24_config.polarities.rx_polarities.rxc_inverted,
             cfg->v24_config.polarities.rx_polarities.dcd_inverted);
}

// Write: erase then program
void config_write(const config_t* cfg)
{
    uint8_t   buf[FLASH_PAGE_SIZE] = {0}; // 256 bytes, pad with zeros
    config_t* _cfg                 = (config_t*)cfg;
    _cfg->magic                    = CONFIG_MAGIC;
    memcpy(buf, _cfg, sizeof(config_t));

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}
