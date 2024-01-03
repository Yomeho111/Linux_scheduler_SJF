#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>

unsigned long long fib(unsigned long long n)
{
	if (n <= 1)
		return n;
	else
		return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv)
{
	int ret;
	unsigned long long n = atoi(argv[1]);
	int oven_max = sched_get_priority_max(7);
	int oven_min = sched_get_priority_min(7);
	struct sched_param params = {oven_min + n};

	ret = sched_setscheduler(0, 7, &params);
	if (ret) {
		fprintf(stderr, "OVEN scheduling policy does not exist\n");
		exit(1);
	}

	if (argc != 2) {
		fprintf(stderr, "Usage: ./fibonacci [number]\n");
		exit(1);
	}

	return fib(n);
}
