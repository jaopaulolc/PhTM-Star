
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
#include <time.h>
#define INIT_MAX_TRANS 1000000
static uint64_t started __ALIGN__ = 0;
static uint64_t start_time __ALIGN__ = 0;
static uint64_t end_time __ALIGN__ = 0;
static uint64_t trans_index __ALIGN__ = 1;
static uint64_t trans_labels_size __ALIGN__ = INIT_MAX_TRANS;
#endif /* PHASE_PROFILING || TIME_MODE_PROFILING */
static uint64_t hw_sw_transitions __ALIGN__ = 0;
#if DESIGN == OPTIMIZED
static uint64_t hw_lock_transitions __ALIGN__ = 0;
#endif /* DESIGN == OPTIMIZED */
#if defined(PHASE_PROFILING) || defined(TIME_MODE_PROFILING)
typedef struct _trans_label_t {
	uint64_t timestamp;
	unsigned char mode;
	char padding[__CACHE_LINE_SIZE__ - sizeof(unsigned char) - sizeof(uint64_t)];
} trans_label_t __ALIGN__;
static trans_label_t *trans_labels __ALIGN__;
static uint64_t hw_sw_wait_time  __ALIGN__ = 0;
static uint64_t sw_hw_wait_time  __ALIGN__ = 0;

static __thread uint64_t __before__ __ALIGN__ = 0;

inline
uint64_t getTime(){
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)(t.tv_sec*1.0e9) + (uint64_t)(t.tv_nsec);
}

inline
void increaseTransLabelsSize(trans_label_t **ptr, uint64_t *oldLength, uint64_t newLength) {
	trans_label_t *newPtr = (trans_label_t*)malloc(newLength*sizeof(trans_label_t));
	if ( newPtr == NULL ) {
		perror("malloc");
		fprintf(stderr, "error: failed to increase trans_labels array!\n");
		exit(EXIT_FAILURE);
	}
	memcpy((void*)newPtr, (const void*)*ptr, (*oldLength)*sizeof(trans_label_t));
	free(*ptr);
	*ptr = newPtr;
	*oldLength = newLength;
}

inline
void updateTransitionProfilingData(uint64_t mode){
	uint64_t now = getTime();
	if(mode == SW){
		hw_sw_transitions++;
		if (__before__ != 0) {
			hw_sw_wait_time += now - __before__;
		}
	} else if (mode == HW){
		if (__before__ != 0) {
			sw_hw_wait_time += now - __before__;
		}
	} else { // GLOCK
#if DESIGN == OPTIMIZED
		hw_lock_transitions++;
#endif /* DESIGN == OPTIMIZED */
	}
	if ( unlikely(trans_index >= trans_labels_size) ) {
		increaseTransLabelsSize(&trans_labels, &trans_labels_size, 2*trans_labels_size);
	}
	trans_labels[trans_index++].timestamp = now;
	trans_labels[trans_index-1].mode = mode;
	__before__ = 0;
}

inline
void setProfilingReferenceTime(){
	__before__ = getTime();
}

inline
void phase_profiling_init(){
	trans_labels = (trans_label_t*)malloc(sizeof(trans_label_t)*INIT_MAX_TRANS);
}

inline
void phase_profiling_start(){
	if(started == 0){
		started = 1;
		start_time = getTime();
	}
}

inline
void phase_profiling_stop(){
	end_time = getTime();
}

inline
void phase_profiling_report(){
	printf("hw_sw_transitions: %lu \n", hw_sw_transitions);
#if DESIGN == OPTIMIZED	
	printf("hw_lock_transitions: %lu \n", hw_lock_transitions);
#endif /* DESIGN == OPTIMIZED */
	
#ifdef PHASE_PROFILING
	FILE *f = fopen("transitions.timestamp", "w");
	if(f == NULL){
		perror("fopen");
	}
#endif /* PHASE_PROFILING*/
	
	trans_labels[0].timestamp = start_time;
	trans_labels[0].mode = HW;

	uint64_t i, ttime = 0;
#ifdef TIME_MODE_PROFILING
	uint64_t hw_time = 0, sw_time = 0;
#if DESIGN == OPTIMIZED
	uint64_t lock_time = 0;
#endif /* DESIGN == OPTIMIZED */
#endif /* TIME_MODE_PROFILING */
	for (i=1; i < trans_index; i++){
		uint64_t dx = trans_labels[i].timestamp - trans_labels[i-1].timestamp;
		unsigned char mode = trans_labels[i-1].mode;
#ifdef PHASE_PROFILING
		fprintf(f, "%lu %d\n", dx, mode);
#else /* TIME_MODE_PROFILING */
		switch (mode) {
			case HW:
				hw_time += dx;
				break;
			case SW:
				sw_time += dx;
				break;
#if DESIGN == OPTIMIZED
			case GLOCK:
				lock_time += dx;
				break;
#endif /* DESIGN == OPTIMIZED */
			default:
				fprintf(stderr, "error: invalid mode in trans_labels array!\n");
				exit(EXIT_FAILURE);
		}
#endif /* TIME_MODE_PROFILING */
		ttime += dx;
	}
	if(ttime < end_time){
		uint64_t dx = end_time - trans_labels[i-1].timestamp;
		unsigned char mode = trans_labels[i-1].mode;
#ifdef PHASE_PROFILING
		fprintf(f, "%lu %d\n", dx, mode);
#else /* TIME_MODE_PROFILING */
		switch (mode) {
			case HW:
				hw_time += dx;
				break;
			case SW:
				sw_time += dx;
				break;
#if DESIGN == OPTIMIZED
			case GLOCK:
				lock_time += dx;
				break;
#endif /* DESIGN == OPTIMIZED */
			default:
				fprintf(stderr, "error: invalid mode in trans_labels array!\n");
				exit(EXIT_FAILURE);
		}
#endif /* TIME_MODE_PROFILING */
		ttime += dx;
	}

#ifdef PHASE_PROFILING
	fprintf(f, "\n\n");
	fclose(f);
#endif /* PHASE_PROFILING */

#ifdef TIME_MODE_PROFILING
	printf("hw:   %6.2lf\n", 100.0*((double)hw_time/(double)ttime));
	printf("sw:   %6.2lf\n", 100.0*((double)sw_time/(double)ttime));
#if DESIGN == OPTIMIZED
	printf("lock: %6.2lf\n", 100.0*((double)lock_time/(double)ttime));
#endif /* DESIGN == OPTIMIZED */
	printf("hw_sw_wtime: %lu (%6.2lf)\n", hw_sw_wait_time,100.0*((double)hw_sw_wait_time/ttime));
	printf("sw_hw_wtime: %lu (%6.2lf)\n", sw_hw_wait_time,100.0*((double)sw_hw_wait_time/ttime));
#endif /* PHASE_PROFILING */
	
	free(trans_labels);
}

#else /* NO PROFILING */

#define setProfilingReferenceTime();         /* nothing */
#define updateTransitionProfilingData(m);    /* nothing */
#define phase_profiling_init();              /* nothing */
#define phase_profiling_start();             /* nothing */
#define phase_profiling_stop();              /* nothing */
#define phase_profiling_report();            /* nothing */

#endif /* NO PROFILING */
