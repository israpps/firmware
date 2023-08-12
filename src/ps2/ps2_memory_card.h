#pragma once

#include <stdbool.h>

void ps2_memory_card_main(void);
void ps2_memory_card_enter(void);
void ps2_memory_card_enter_flash(void);
void ps2_memory_card_exit(void);

#define SD2PSXMAN_PROTOCOL_VER 0x001 /// TODO: Discuss if this will be an arbitrary number. or if we should use the SD2PSXMAN IRX Version number as protocol Version too
#define SD2PSXMAN_PRODUCTS_VER 0x001
#define IRX_VER(major, minor)	((((major) & 0xff) << 8) + ((minor) & 0xff))