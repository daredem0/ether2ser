/*
 * ether2ser - Ethernet <-> synchronous V.24 (RS-232/V.28) bridge
 *
 * File:    src/system/persistent_config.c
 * Purpose: Flash-backed persistent configuration and memory usage reporting.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (c) 2026 Florian <f.leuze@outlook.de>
 */

// Related headers
#include "persistent_config.h"

// Standard library headers
#include <assert.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Library Headers
#include "boards/pico.h"
#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "pico/flash.h"

// Project Headers
#include "system/common.h"

// Generated headers

// Reserve last 4KB sector for config
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

#define CONFIG_MAGIC 0xCAFEBABE
#define CONFIG_VERSION_CURRENT 1U
#define CONFIG_VERSION_WITH_OPERATING_MODE 1U
#define BYTES_PER_KIBIBYTE 1024U
#define RAM_TOTAL_KIBIBYTES 264U
#define RAM_BASE_ADDRESS 0x20000000U
#define W55RP20_FLASH_TOTAL_BYTES (2U * BYTES_PER_KIBIBYTE * BYTES_PER_KIBIBYTE)
#define FLASH_SAFE_EXECUTE_TIMEOUT_MS 1000U

void memory_status_snapshot(management_memory_snapshot_t* snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }
    extern char bss_end_sym asm("__bss_end__");
    extern char flash_binary_start_sym asm("__flash_binary_start");
    extern char flash_binary_end_sym asm("__flash_binary_end");

    snapshot->ram_total = RAM_TOTAL_KIBIBYTES * BYTES_PER_KIBIBYTE;
    snapshot->ram_static_used = (uint32_t)&bss_end_sym - RAM_BASE_ADDRESS;
    const struct mallinfo current_mallinfo = mallinfo();
    snapshot->heap_allocated = (uint32_t)current_mallinfo.uordblks;
    snapshot->ram_free =
        snapshot->ram_total - snapshot->ram_static_used - snapshot->heap_allocated;

    const uintptr_t flash_start = (uintptr_t)&flash_binary_start_sym;
    const uintptr_t flash_end = (uintptr_t)&flash_binary_end_sym;
    snapshot->flash_used = (uint32_t)(flash_end - flash_start);
    snapshot->flash_total = W55RP20_FLASH_TOTAL_BYTES;
    snapshot->flash_free = snapshot->flash_total - snapshot->flash_used;
}

void print_memory_usage(void)
{
    management_memory_snapshot_t snapshot;
    memory_status_snapshot(&snapshot);
    LOG_PLAIN("=== Memory Usage ===\n");
    LOG_PLAIN("Static RAM (data+bss): %" PRIu32 " bytes (%.1f KB)\n",
              snapshot.ram_static_used,
              (double)snapshot.ram_static_used / (double)BYTES_PER_KIBIBYTE);
    LOG_PLAIN("Heap allocated: %" PRIu32 " bytes (%.1f KB)\n",
              snapshot.heap_allocated,
              (double)snapshot.heap_allocated / (double)BYTES_PER_KIBIBYTE);
    LOG_PLAIN("Total RAM: %" PRIu32 " bytes (%.1f KB)\n", snapshot.ram_total,
              (double)snapshot.ram_total / (double)BYTES_PER_KIBIBYTE);
    LOG_PLAIN("Approx free: %" PRIu32 " bytes (%.1f KB)\n", snapshot.ram_free,
              (double)snapshot.ram_free / (double)BYTES_PER_KIBIBYTE);
}

void print_flash_usage(void)
{
    management_memory_snapshot_t snapshot;
    memory_status_snapshot(&snapshot);
    LOG_PLAIN("=== Flash Usage ===\n");
    LOG_PLAIN("Flash used: %" PRIu32 " bytes (%.1f KB)\n", snapshot.flash_used,
              (double)snapshot.flash_used / (double)BYTES_PER_KIBIBYTE);
    LOG_PLAIN("Total flash: %" PRIu32 " bytes\n", snapshot.flash_total);
    LOG_PLAIN("Flash free: %" PRIu32 " bytes\n", snapshot.flash_free);
}

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
    if (cfg->version < CONFIG_VERSION_WITH_OPERATING_MODE)
    {
        cfg->operating_mode = OPERATING_MODE_DEFAULT;
    }
    return true;
}

static void config_program_page(void* param)
{
    const uint8_t* page = (const uint8_t*)param;
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(FLASH_TARGET_OFFSET, page, FLASH_PAGE_SIZE);
}

static bool config_commit_page(const uint8_t* page)
{
    return flash_safe_execute(config_program_page, (void*)page,
                              FLASH_SAFE_EXECUTE_TIMEOUT_MS) == PICO_OK;
}

bool config_wipe(void)
{
    uint8_t buf[FLASH_PAGE_SIZE] = {0};
    return config_commit_page(buf);
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
    LOG_INFO("  mode=%s\r\n", operating_mode_name(cfg->operating_mode));
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
bool config_write(const config_t* cfg)
{
    assert(FLASH_PAGE_SIZE > sizeof(config_t));
    uint8_t  buf[FLASH_PAGE_SIZE] = {0};
    config_t stored              = *cfg;
    stored.magic                 = CONFIG_MAGIC;
    stored.version               = CONFIG_VERSION_CURRENT;
    memcpy(buf, &stored, sizeof(stored));

    return config_commit_page(buf);
}
