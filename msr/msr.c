#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

#include <msr.h>

int __msrFileDescriptor;
int __isCore0MsrFileOpen = 0;
double __energyUnit;

void msrInitialize(){
  
	uint64_t result;
  
	// open file descriptor for only one socket
	// TODO: handle file descriptors for more sockets
	__msrFileDescriptor = __msrOpen(0);
	
	if (__msrFileDescriptor == 0) {
		fprintf(stderr, "warning: msr-based measurements are disabled!\n");
		return;
	}
  
  // read power and energy units
	result = __msrRead(__msrFileDescriptor,MSR_RAPL_POWER_UNIT);
  
  //power_units=pow(0.5,(double)(result&0xf));
  __energyUnit = pow(0.5,(double)((result>>8)&0x1f));
  //time_units=pow(0.5,(double)((result>>16)&0xf));
}

void msrTerminate(){
	if (__msrFileDescriptor == 0) return; // msr-based measurements are disabled
	__msrClose(__msrFileDescriptor);
}

double msrDiffCounter(unsigned int before,unsigned int after){
	if (__msrFileDescriptor == 0) return 0; // msr-based measurements are disabled
	if(before > after){
		return (after + (UINT_MAX - before))*__energyUnit;
	} else {
		return (after - before)*__energyUnit;
	}
}

unsigned int msrGetCounter(){

	uint64_t counter;
	
	if (__msrFileDescriptor == 0) return 0; // msr-based measurements are disabled

  counter = __msrRead(__msrFileDescriptor, MSR_PKG_ENERGY_STATUS);
  return (unsigned int)counter;
} 

int __msrOpen(int core) {

	if(core == 0 && __isCore0MsrFileOpen){
	  __isCore0MsrFileOpen++;
		return __msrFileDescriptor;
	}

  char msr_filename[BUFSIZ];
  int fd;

  sprintf(msr_filename, "/dev/cpu/%d/msr", core);
  fd = open(msr_filename, O_RDWR);
  if ( fd < 0 ) {
    if ( errno == ENXIO ) {
      fprintf(stderr, "rdmsr: No CPU %d\n", core);
      //exit(2);
    } else if ( errno == EIO ) {
      fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n", core);
      //exit(3);
    } else {
      fprintf(stderr,"Trying to open %s\n",msr_filename);
      //exit(127);
    }
		return 0;
  }
	
	if(core == 0) __isCore0MsrFileOpen++;
  return fd;
}

void __msrClose(int msrFd) {

	if(msrFd == __msrFileDescriptor){
		__isCore0MsrFileOpen--;
		if(__isCore0MsrFileOpen == 0) close(msrFd);
		return;
	}

	close(msrFd);
}

uint64_t __msrRead(int fd, int which){

  uint64_t data;

  if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
		fprintf(stderr, "error: pread failed at __msrRead function!");
    exit(127);
  }

  return (uint64_t)data;
}

void __msrWrite(int fd, int which, uint64_t data){

  if ( pwrite(fd, &data, sizeof data, which) != sizeof data ) {
		fprintf(stderr, "error: pwrite failed at __msrWrite function!");
    exit(127);
  }
}
