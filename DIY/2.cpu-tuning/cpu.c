#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>

static int qnum = 10;
static int delay = 2;//us
static int dmod = 2;//delay mod
static int number = 0;

void swap(int k[], int low, int high)
{
	int temp;
	temp = k[low];
	k[low] = k[high];
	k[high] = temp;
}

int Partition(int k[], int low, int high)
{
	int point;
	point = k[low];
	while(low < high) {
		while(low < high && k[high] >= point) {
			high--;
		}
		swap(k, low, high);	
		while(low < high && k[low] <= point) {
			low++;
		}
		swap(k, low, high);
	}
	return low;
}

void QSort(int k[], int low, int high)
{
	int point;
	if(low < high) {
		point = Partition(k, low, high);
		QSort(k, low, point-1);
		QSort(k, point+1, high);
	}
}

int QuickSort(int k[], int n)
{
	QSort(k, 0, n-1);
	return 0;
}

static int api_get_thread_policy (pthread_attr_t *attr)
{
	int policy;
	int rs = pthread_attr_getschedpolicy (attr, &policy);
	switch (policy) {
		case SCHED_FIFO:
			printf ("policy = SCHED_FIFO\n");
			break;
		case SCHED_RR:
			printf ("policy = SCHED_RR");
			break;
		case SCHED_OTHER:
			printf ("policy = SCHED_OTHER\n");
			break;
		default:
			printf ("policy = UNKNOWN\n");
			break;
	}
	return policy;
}

static void api_set_thread_policy (pthread_attr_t *attr,int policy)
{
	int rs = pthread_attr_setschedpolicy (attr, policy);
	api_get_thread_policy (attr);
}

void *func(void *arg)
{
	int id = *(int*)arg;
	unsigned long add = 0;
	long dec = 4611686018427387904;
	float mul = 1;
	float div = 9223372036854775808;

	unsigned long jia = 0;
	long jian = 4611686018427387904;
	float cheng = 1;
	float chu = 9223372036854775808;

	unsigned long measure = 9223372036854775808;
	unsigned long i = 1;
	unsigned long j = 2;

	int a[1000] = {5, 2, 6, 0, 3, 9, 1, 7, 4, 8};
	int k=0;
#if 0
	pthread_attr_t attr;
	int policy = api_get_thread_policy(&attr);
	printf("policy = %d\n", policy);

	api_set_thread_policy(&attr, SCHED_FIFO);
	policy = api_get_thread_policy (&attr);
    printf("policy = %d\n", policy);

	//sleep(5);
#endif

	srand((unsigned)time(NULL));

	printf("func start %d!\n", ++number);
	//for(i=0; i<9223372036854775808; i++) {
	while(1) {
		i++;
		j = j+2;

		//+-*/ first
		add += i;
		dec = dec - i;
		mul *= 1.0301040105090206;
		div /= 1.0301040105090206;

		//+-*/ double
		jia += j;
		jian = jian -j;
		cheng *= 2.7182818284590452;
		chu /= 2.7182818284590452;

		//printf("add=%lu, dec=%ld, mul=%f, div=%f ----- Thread%d\n", add, dec, mul, div, id);
		//printf("jia=%lu, jian=%ld, cheng=%f, chu=%f ----- Thread%d\n", jia, jian, cheng, chu, id);

		//qsort
		a[0] = (int)i;
		a[1] = (int)j;
		a[2] = (int)add;
		a[3] = (int)dec;
		a[4] = (int)mul;
		a[5] = (int)div;
		a[6] = (int)jia;
		a[7] = (int)jian;
		a[8] = (int)cheng;
		a[9] = (int)chu;

		for(k=10;k<qnum;k++) {
			a[k] = rand();
		}
#if 0
		printf("排序前的结果是1: \n");
		for(k=0; k<qnum; k++) {
			printf("%d, ", a[k]);
		}
		printf("\n\n");
#endif
		QuickSort(a, qnum);
#if 0
		printf("排序后的结果是2: \n");
		for(k=0; k<qnum; k++) {
			printf("%d, ", a[k]);
		}
		printf("\n\n");
		sleep(3);
#endif

		if(add > measure) {
			add = 0;
		//	printf("-------------------- add reset to %lu --------------------\n", add);
		//	sleep(2);
		}
		if(dec < 100) {
			dec = 4611686018427387904;
		//	printf("-------------------- dec reset to %ld --------------------\n", dec);
		//	sleep(2);
		}
		if(mul > measure) {
			mul = 1;
		//	printf("-------------------- mul reset to %f --------------------\n", mul);
		}
		if(div < 100) {
			div = 9223372036854775808;
		//	printf("-------------------- div reset to %f --------------------\n", div);
		}
		if(i > 4294967296)
			i = 1;

		if(jia > measure) {
			jia = 0;
		}
		if(jian < 100) {
			jian = 4611686018427387904;
		}
		if(cheng > measure) {
			cheng = 1;
		}
		if(chu < 100) {
			chu = 9223372036854775808;
		}
		if(j > 4294967296)
			j = 2;

		if(i%dmod == 0)
			usleep(delay);
	}
	return NULL;
}

int main(int argv, char* argc[])
{
	pthread_t t[1000];
	pthread_attr_t attr;
	int ret = 0;
	int i = 0;
	char a;

	//api_get_thread_policy (&attr);
	//api_set_thread_policy(&attr, SCHED_FIFO);

	qnum = atoi(argc[1]);
	delay = atoi(argc[2]);
	dmod = atoi(argc[3]);
	printf("main() start!!! Influence factor: qnum = %d, delay = %dus, dmod = %d\n", qnum, delay, dmod);

	for(i=0; i<300; i++) {
		scanf("%c", &a);
		printf("you input %c", a);
		ret = pthread_create(&t[i], NULL, (void*)func, &i);
	}
	for(i=0; i<300; i++) {
		pthread_join(t[i], NULL);
	}
	printf("main() abort!!! ________\n");
	return 1;
}
