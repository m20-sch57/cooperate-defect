#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define BUF_LEN		4

int main(void) {
	char a[1];
	int i, j, k, N, *ans, buf[BUF_LEN], num_turn;
	pid_t pid;

	pid = getpid();
	fprintf(stderr, "%i: Started!\n", pid);
	N = 0;
	*(buf+0) = 0;
	j = 0;
	while (1) {
		i = read(0, a, 1);
		if (i < 0) {
			perror("read");
			return 1;
		}
		if (i == 0) {
			fprintf(stderr, "EOF\n");
			return 1;
		}
		if (*a == ' ') {
			buf[++j] = 0;
			if (j >= BUF_LEN) {
				goto wrong_number_of_ints;
			}
			continue;
		}
		if (*a == '\n') {
			j++;
			break;
		}
		if ('0' > *a || '9' < *a) {
			fprintf(stderr, "Bad symbol in N: %c\n", *a);
			return 1;
		}
		buf[j] = buf[j]*10 + *a - '0';
	}
	if (j != 1) {
wrong_number_of_ints:
		fprintf(stderr, "%i: expected 1 nonnegative integers, got at least %i:\n", pid, j);
		for (i = 0; i < j && i < BUF_LEN; ++i) {
			fprintf(stderr, (i==j-1||i==BUF_LEN-1) ? "%i\n" : "%i ", buf[i]);
		}
		return 1;
	}
	N = buf[0];
	fprintf(stderr, "%i: number of turns: %i\n", pid, N);
	ans = malloc(sizeof(int)*N);
	if (ans == NULL) {
		perror("malloc");
		return 1;
	}
	for (num_turn = 0; num_turn < N; ++num_turn) {
		k = write(1, "c\n", 2);
		if (k < 0) {
			perror("write");
			return 1;
		}
		if (k != 2) {
			fprintf(stderr, "%i: Number of bytes written is less than 2\n", pid);
			return 1;
		}
		k = read(0, a, 1);
		if (k < 0) {
			perror("read");
			return 1;
		}
		if (k == 0) {
			fprintf(stderr, "%i: stdin is closed, aborting\n", pid);
			return 1;
		}
		fprintf(stderr, "%i: got `%c`\n", pid, *a);
		k = read(0, a, 1);
		if (k < 0) {
			perror("read");
			return 1;
		}
		if (k == 0) {
			fprintf(stderr, "%i: stdin is closed, aborting\n", pid);
			return 1;
		}
		if (*a != '\n') {
			fprintf(stderr, "%i: expected `\n`, got `%c`, aborting\n", pid, *a);
			return 1;
		}
	}
	fprintf(stderr, "%i: Done!\n", pid);
	return 0;
}
