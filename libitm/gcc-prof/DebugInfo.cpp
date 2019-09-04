#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "DebugInfo.h"

static char *ProgramName;
static bfd* abfd;
static asymbol **syms;		/* Symbol table.  */

DebugInfo::DebugInfo(bfd_vma _pc) {
  pc = _pc;
  filename = nullptr;
  functionname = nullptr;
  line = 0;
  found = 0;
}

static
char* getProgramName() {

  char* ProgramName;

  pid_t MyPid = getpid();

  char CmdLine[BUFSIZ];
  size_t ret = snprintf(CmdLine, BUFSIZ, "/proc/%u/cmdline", MyPid);
  if (ret >= BUFSIZ) {
    std::cerr << "error: CmdLine buffer smaller than '/proc/{{pid}}/cmdline' string" << std::endl;
    exit(EXIT_FAILURE);
  }

  int FD = open(CmdLine, O_RDONLY);
  if (FD < 0) {
    perror("open");
    exit(EXIT_FAILURE);
  }

  ssize_t FileSize = BUFSIZ;
  ProgramName = (char*)malloc(sizeof(char)*(FileSize+1));
  ssize_t BytesRead = read(FD, ProgramName, FileSize);
  if (BytesRead < 0) {
    perror("read");
    exit(EXIT_FAILURE);
  }
  ProgramName[FileSize] = '\0';

  return ProgramName;
}

/* Extracted form addr2line.c
 *
 * Read in the symbol table.
 */
static void
slurp_symtab (bfd *abfd) {

  long storage;
  long symcount;
  bfd_boolean dynamic = FALSE;
  bfd_error_type ErrType;
  const char* ErrMsg;

  if ((bfd_get_file_flags (abfd) & HAS_SYMS) == 0)
    return;

  storage = bfd_get_symtab_upper_bound (abfd);
  if (storage < 0) {
    ErrType = bfd_get_error();
    ErrMsg = bfd_errmsg(ErrType);
    std::cerr << ProgramName
      << ": error while calling bfd_get_symtab_upper_bound!" << std::endl;
    std::cerr << ErrMsg << std::endl;
  }

  if (storage == 0) {
    storage = bfd_get_dynamic_symtab_upper_bound (abfd);
    dynamic = TRUE;
  }

  syms = (asymbol **) malloc(storage);
  if (dynamic) {
    symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
  } else {
    symcount = bfd_canonicalize_symtab (abfd, syms);
  }

  if (symcount < 0) {
    ErrType = bfd_get_error();
    ErrMsg = bfd_errmsg(ErrType);
    std::cerr << ProgramName
      << ": error while calling bfd_canonicalize_XXX !" << std::endl;
    std::cerr << ErrMsg << std::endl;
  }

  /* If there are no symbols left after canonicalization and
     we have not tried the dynamic symbols then give them a go.  */
  if (symcount == 0
      && ! dynamic
      && (storage = bfd_get_dynamic_symtab_upper_bound (abfd)) > 0) {
    free (syms);
    syms = (asymbol **) malloc(storage);
    symcount = bfd_canonicalize_dynamic_symtab (abfd, syms);
  }

  /* PR 17512: file: 2a1d3b5b.
     Do not pretend that we have some symbols when we don't.  */
  if (symcount <= 0) {
    free (syms);
    syms = nullptr;
  }
}

/* Extracted form addr2line.c
 *
 * Look for an address in a section. This is called via bfd_map_over_sections.
 */
static void
find_address_in_section (bfd *abfd, asection *section, void *data) {

  DebugInfo* DI = (DebugInfo*)data;
  bfd_vma vma;
  bfd_size_type size;

  if (DI->found)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
    return;

  vma = bfd_get_section_vma (abfd, section);
  if (DI->pc < vma)
    return;

  size = bfd_get_section_size (section);
  if (DI->pc >= vma + size)
    return;

  DI->found = bfd_find_nearest_line(abfd, section, syms, DI->pc - vma,
      &DI->filename, &DI->functionname, &DI->line);//, &discriminator);
}

void initDebugInfo() {

  bfd_init();

  ProgramName = getProgramName();

  abfd = bfd_openr(ProgramName, /*TARGET*/nullptr);

  bfd_error_type ErrType = bfd_get_error();
  if (ErrType == bfd_error_no_memory) {
    const char* ErrMsg = bfd_errmsg(ErrType);
    std::cerr << ProgramName
      << ": fatal error: bfd_error_no_memory : " << ErrMsg << std::endl;
    exit(EXIT_FAILURE);
  } else if (ErrType == bfd_error_invalid_target) {
    const char* ErrMsg = bfd_errmsg(ErrType);
    std::cerr << ProgramName
      << ": fatal error: bfd_error_invalid_target : " << ErrMsg << std::endl;
    exit(EXIT_FAILURE);
  } else if (ErrType == bfd_error_system_call) {
    perror("bfd_openr");
    exit(EXIT_FAILURE);
  }

  /* Decompress sections.  */
  abfd->flags |= BFD_DECOMPRESS;

  if (bfd_check_format (abfd, bfd_archive)) {
    std::cerr << ProgramName <<
      ": cannot get addresses from archive" << std::endl;
  }

  char** matching;
  if (! bfd_check_format_matches (abfd, bfd_object, &matching)) {
    std::cerr << ProgramName <<
      ": binary file matches multiple formats!" << std::endl;
    /*if (bfd_get_error () == bfd_error_file_ambiguously_recognized) {
	    list_matching_formats (matching);
	    free (matching);
	  }*/
    exit(EXIT_FAILURE);
  }

  // read symtab
  slurp_symtab(abfd);
}

void termDebugInfo() {
  bfd_close(abfd);
}

void getDebugInfo (DebugInfo* DI) {
	bfd_map_over_sections (abfd, find_address_in_section, DI);
}
