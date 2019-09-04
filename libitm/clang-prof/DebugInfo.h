#pragma once

#include <bfd.h>

#undef bfd_init
extern "C" unsigned int bfd_init (void);

struct DebugInfo {
  bfd_vma pc;
  const char *filename;
  const char *functionname;
  unsigned int line;
  bfd_boolean found;
  DebugInfo(bfd_vma pc);
};

void initDebugInfo();
void termDebugInfo();
void getDebugInfo(DebugInfo* DI);
