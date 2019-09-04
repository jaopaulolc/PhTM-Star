#ifndef ARCH_H
#define ARCH_H

#ifdef __x86_64__
#include "config/x86_64/target.h"
#else
#error "unsupported architecture! no config/arch/target.h defined."
#endif

#endif /* ARCH_H */
