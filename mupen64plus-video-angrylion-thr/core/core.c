#include "core.h"

#include "rdp.h"
#include "vi.h"
#include "rdram.h"
#include "msg.h"
#include "plugin.h"
#include "parallel_c.hpp"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static uint32_t num_workers;
static bool parallel;
static bool parallel_tmp;

static struct core_config* config_new;
static struct core_config config;

static char filter_char(char c)
{
    if (isalnum(c) || c == '_' || c == '-' || c == '.')
        return c;
    return ' ';
}

static uint32_t get_rom_name(char* name, uint32_t name_size)
{
   int i = 0;
   uint8_t *rom_header = NULL;

   // buffer too small
   if (name_size < 21)
      return 0;

   rom_header = plugin_get_rom_header();

   // not available
   if (!rom_header)
      return 0;

   // copy game name from ROM header, which is encoded in Shift_JIS.
   // most games just use the ASCII subset, so filter out the rest.
   for (; i < 20; i++)
      name[i] = filter_char(rom_header[(32 + i) ^ BYTE_ADDR_XOR]);

   // make sure there's at least one 
   // whitespace that will terminate the string
   // below
   name[i] = ' ';

   // trim trailing whitespaces
   for (; i > 0; i--)
   {
      if (name[i] != ' ')
         break;
      name[i] = 0;
   }

   // game title is empty or invalid, use safe 
   // fallback using the four-character
   // game ID
   if (i == 0)
   {
      for (; i < 4; i++)
         name[i] = filter_char(rom_header[(59 + i) ^ BYTE_ADDR_XOR]);
      name[i] = 0;
   }

   return i;
}

void core_init(struct core_config* _config)
{
   config = *_config;

   plugin_init();
   rdram_init();

   rdp_init(&config);
   vi_init(&config);

   num_workers = config.num_workers;
   parallel    = config.parallel;

   if (config.parallel)
      parallel_alinit(num_workers);
}

void core_dp_sync(void)
{
   // update config if set
   if (config_new)
   {
      config = *config_new;
      config_new = NULL;

      // enable/disable multithreading or update number of workers
      if (config.parallel != parallel || config.num_workers != num_workers)
      {
         // destroy old threads
         parallel_close();

         // create new threads if parallel option is still enabled
         if (config.parallel)
            parallel_alinit(num_workers);

         num_workers = config.num_workers;
         parallel = config.parallel;
      }
   }

   // signal plugin to handle interrupts
   plugin_sync_dp();
}

void core_config_update(struct core_config* _config)
{
    config_new = _config;
}

void core_config_defaults(struct core_config* config)
{
    memset(config, 0, sizeof(*config));
    config->parallel = true;
}

void core_dp_update(void)
{
    rdp_update();
}

void core_vi_update(void)
{
    vi_update();
}

void core_close(void)
{
    parallel_close();
    vi_close();
    plugin_close();
}
