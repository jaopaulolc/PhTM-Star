#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#define MSR_RAPL_POWER_UNIT		0x606
#define MSR_PKG_ENERGY_STATUS		0x611
#define MSR_PP0_ENERGY_STATUS		0x639

int __msrOpen(int core);
long long __msrRead(int fd, int which);

int __msrFileDescriptor;
double __energyUnit;

void msrInit(){
  
	long long result;
  
	// open file descriptor for only one socket
	// TODO: handle file descriptors for more sockets
	__msrFileDescriptor = __msrOpen(0);
  
  // read power and energy units
	result = __msrRead(__msrFileDescriptor,MSR_RAPL_POWER_UNIT);
  
  //power_units=pow(0.5,(double)(result&0xf));
  __energyUnit = pow(0.5,(double)((result>>8)&0x1f));
  //time_units=pow(0.5,(double)((result>>16)&0xf));
}

double msrDiffCounter(unsigned int before,unsigned int after){
	if(before > after){
		return (after + (UINT_MAX - before))*__energyUnit;
	} else {
		return (after - before)*__energyUnit;
	}
}

unsigned int msrGetCounter(){

	long long counter;

  counter = __msrRead(__msrFileDescriptor, MSR_PKG_ENERGY_STATUS);
  return (unsigned int)counter;
} 

int __msrOpen(int core) {

  char msr_filename[BUFSIZ];
  int fd;

  sprintf(msr_filename, "/dev/cpu/%d/msr", core);
  fd = open(msr_filename, O_RDONLY);
  if ( fd < 0 ) {
    if ( errno == ENXIO ) {
      fprintf(stderr, "rdmsr: No CPU %d\n", core);
      exit(2);
    } else if ( errno == EIO ) {
      fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
      exit(3);
    } else {
      perror("rdmsr:open");
      fprintf(stderr,"Trying to open %s\n",msr_filename);
      exit(127);
    }
  }

  return fd;
}

long long __msrRead(int fd, int which){

  uint64_t data;

  if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
    perror("rdmsr:pread");
    exit(127);
  }

  return (long long)data;
}
