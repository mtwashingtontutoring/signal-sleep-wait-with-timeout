#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#define N_TICKS 30u
#define ALARM_PERIOD_MS 500u

typedef enum _condition {
	ready, not_ready
} condition_t;

static char *condition_to_str(condition_t cond) {
	switch(cond) {
	case ready:
		return "ready";
	case not_ready:
		return "not ready";
	default:
		return "unknown";
	}
}

static int msleep(unsigned int ms) {
	struct timespec ts;
	int res;

	ts.tv_sec = ms/ 1000;
	ts.tv_nsec = (ms % 1000) * 1000;

	do {
		res = nanosleep(&ts, &ts);
	} while (res  && errno == EINTR);

	return res;
}

static volatile int alarm_num = 0;

// used to lock thread till interrupted, timed out or completed task
static volatile bool spinLock;
// a counter used for timeouts, negative values means will never time out
static volatile long timeoutCounter;
// a flag to indicate current task completed
static volatile condition_t test_condition;

static bool keepRunning;

void alarmHandler(int signum) {

	printf("***** Alarm called (alarm_num = %d, timeout counter = %lu, spinLock = %s, test condition = %s)\n",
			alarm_num++, timeoutCounter, (spinLock?"locked":"unlocked"),
			condition_to_str(test_condition)
	);

	// handle the timeout counter and spin lock
	if(timeoutCounter==0) {
		spinLock = false;
		puts("\t resetting spinlock");
	} else if(timeoutCounter > 0) {
		timeoutCounter--;
	}
}

void userHandler(int signum) {
	puts("\t\tinterupted!!");
	// interrupt the spin lock
	spinLock = false;

	// randomly change the test_condition to 2 (20% of the time)
	int test_val = rand() % 5;
	if(test_val == 0) {
		puts("\t\tchanging test value");
		test_condition = ready;
	}
}

static void yield(void) {
	sched_yield();
}

void *utilThread(void *arg) {
	while(keepRunning) {
		// choose a random number of (half) ticks to sleep
		int test_val = ((rand() % 20) * ALARM_PERIOD_MS) / 2;

		msleep(test_val);
		raise(SIGUSR1);
	}

	return 0;
}

void *threadMain(void *arg) {


	timeoutCounter = 5; // how long to wait
	printf("\tthreadMain: simple wait with timeoutCounter = %lu starting\n",
			timeoutCounter);
	// simple wait for condition with timeout
	test_condition = not_ready; // the test condition
	while(test_condition != ready && timeoutCounter != 0) {
		spinLock = true;  // use a spin lock
		while(spinLock) yield(); // keep yielding till spin lock is released
	}

	char *msg = "\tthreadMain: simple wait with timeout finished: "
			    "timeoutCounter = %d, condition = %s\n";

	printf(msg, timeoutCounter, condition_to_str(test_condition) );


	timeoutCounter = -1; // wait forever
	puts("\tthreadMain: simple wait with no timeout");
	// simple wait for condition with timeout
	test_condition = not_ready; // the test condition
	while(test_condition != ready && timeoutCounter != 0) {
		spinLock = true;  // use a spin lock
		while(spinLock) yield(); // keep yielding till spin lock is released
	}

	char *msg2 = "\tthreadMain: simple wait with no timeout finished: "
			    "timeoutCounter = %d, condition = %s\n";

	printf(msg2, timeoutCounter, condition_to_str(test_condition) );

	return 0;
}

int main(int argc, char **argv) {

	srand((unsigned int) time(NULL));

	signal(SIGALRM, alarmHandler);
	signal(SIGUSR1, userHandler);

	unsigned int alarmPeriod = ALARM_PERIOD_MS * 1000U;

	printf("tick period = %d micro seconds\n\n\n", alarmPeriod);

	pthread_t thread1, thread2;

	keepRunning = true;
	pthread_create(&thread2, NULL, utilThread, NULL);
	pthread_create(&thread1, NULL, threadMain, NULL);

	ualarm(alarmPeriod, alarmPeriod);

	// ensure a minimum number of ticks
	//while(alarm_num < N_TICKS) yield();

	pthread_join(thread1, NULL);

	keepRunning = false;
	pthread_join(thread2, NULL);

	printf("\n\nAll threads completed\nGoodbye\n");

	return 0;
}
