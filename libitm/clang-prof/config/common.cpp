#include <stdint.h>
#include "common.h"

void* __attribute__((noinline))
mask_stack_bottom() {
  return (uint8_t*)__builtin_dwarf_cfa() - 256;
}
