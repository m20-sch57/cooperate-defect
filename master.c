#define _GNU_SOURCE

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef WAIT_INIT
#define WAIT_INIT	5000
#endif

#ifndef WAIT_SIGKILL
#define WAIT_SIGKILL	100
#endif

#ifndef WAIT_SIGTERM
#define WAIT_SIGTERM	1000
#endif

#ifndef TIMEOUT_MSEC
#define TIMEOUT_MSEC	250
#endif

#ifndef BUFSIZE
#define BUFSIZE		1024
#endif

#ifndef READ_SIZE
#define READ_SIZE	2
#endif

#ifndef NUM_ROUNDS
#define NUM_ROUNDS	100
#endif

#ifndef MEMLIMIT
#define MEMLIMIT	536870912l
#endif

#ifndef GAMELOG_SIZE
#define GAMELOG_SIZE	(NUM_ROUNDS*4)
#endif

#define ARG_LENGTH	7
#define ARG_N_POS	4
char *USAGE = "usage: %s <config_log> <machine_log> <human_log> <error_log> <st_1> <st_2>\n";
char *FIFO_NAME_TEMPL = "fifo%i.%s";

int MACHINE_LOG;
int EXIT_CODE;

volatile sig_atomic_t got_SIGCHLD = 0;

void sa_sigchld(int __attribute__((unused)) sig) {
	got_SIGCHLD = 1;
}

int main(int argc, char **argv) {
	int N, i, j, k, round_num, buf_size, gamelog_pos;
	char **in_fifos, **out_fifos;
	struct pollfd *pfds;
	struct rlimit *lim_as;
	struct timespec *tmo_p;
	struct timeval *pfinish, *pnow, *pstart;
	struct sigaction *psa;
	sigset_t sigmask;
	int *in_fds, *out_fds;
	char *bufs, buf[BUFSIZE], gamelog[GAMELOG_SIZE];
	int *buf_pos;
	char *args[2];
	char *not_alive, *ans;
	int *retstatuses, *answered;
	pid_t *pids, pid;

	EXIT_CODE = EXIT_SUCCESS;
	args[1] = NULL;
	if (argc != ARG_LENGTH) {
		fprintf(stderr, USAGE, *argv);
		return EXIT_FAILURE;
	}
	N = 2;
	gamelog_pos = 0;
	if (strcmp(*(argv+1), "-") != 0) {
		k = open(*(argv+1), O_WRONLY | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (k < 0) {
			fprintf(stderr, "'%s' exists, can't create it!\n", *(argv+1));
			perror("open");
			return EXIT_FAILURE;
		}
	} else {
		k = 1;
	}
	dprintf(k, "# N ROUNDS TIMEOUT_MSEC MEMLIMIT\n# Strategies executables\n%i %i %i %li\n", N, NUM_ROUNDS, TIMEOUT_MSEC, MEMLIMIT);
	for (i = 0; i < N; ++i) {
		dprintf(k, (i == N-1) ? "%s\n" : "%s ", *(argv+(ARG_N_POS+1)+i));
	}
	if (k != 1) {
		fsync(k);
		close(k);
	}
	if (strcmp(*(argv+2), "-") != 0) {
		MACHINE_LOG = open(*(argv+2), O_WRONLY | O_CREAT | O_EXCL,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (MACHINE_LOG < 0) {
			fprintf(stderr, "'%s' exists, can't create it!\n", *(argv+2));
			perror("open");
			return EXIT_FAILURE;
		}
	} else {
		MACHINE_LOG = 1;
	}
	if (strcmp(*(argv+3), "-") != 0) {
		k = open(*(argv+3), O_WRONLY | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (k < 0) {
			fprintf(stderr, "'%s' exists, can't create it!\n", *(argv+3));
			perror("open");
			return EXIT_FAILURE;
		}
		dup2(k, 1);
		close(k);
	}
	if (strcmp(*(argv+4), "-") != 0) {
		k = open(*(argv+4), O_WRONLY | O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (k < 0) {
			fprintf(stderr, "'%s' exists, can't create it!\n", *(argv+4));
			perror("open");
			return EXIT_FAILURE;
		}
		dup2(k, 2);
		close(k);
	}
	in_fifos = malloc(sizeof(char *)*N);
	if (in_fifos == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	out_fifos = malloc(sizeof(char *)*N);
	if (out_fifos == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	in_fds = malloc(sizeof(int)*N);
	if (in_fds == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	out_fds = malloc(sizeof(int)*N);
	if (out_fds == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	bufs = malloc(sizeof(char)*N*READ_SIZE);
	if (bufs == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	buf_pos = malloc(sizeof(int)*N);
	if (buf_pos == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	pfds = malloc(sizeof(struct pollfd)*N);
	if (pfds == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	tmo_p = malloc(sizeof(struct timespec));
	if (tmo_p == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	pstart = malloc(sizeof(struct timeval)*N);
	if (pstart == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	pfinish = malloc(sizeof(struct timeval));
	if (pfinish == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	pnow = malloc(sizeof(struct timeval));
	if (pnow == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	lim_as = malloc(sizeof(struct rlimit));
	if (lim_as == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	psa = malloc(sizeof(struct sigaction));
	if (psa == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	memset(psa, 0, sizeof(struct sigaction));
	not_alive = malloc(sizeof(char)*N);
	if (not_alive == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	memset(not_alive, 0, sizeof(char)*N);
	pids = malloc(sizeof(pid_t)*N);
	if (pids == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	memset(pids, 0, sizeof(pid_t)*N);
	retstatuses = malloc(sizeof(int)*N);
	if (retstatuses == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	ans = malloc(sizeof(char)*N);
	if (ans == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	memset(ans, 0, sizeof(char)*N);
	answered = malloc(sizeof(int)*N);
	if (answered == NULL) {
		perror("malloc");
		return EXIT_FAILURE;
	}
	memset(answered, 0, sizeof(int)*N);
	for (i = 0; i < N; ++i) {
		k = snprintf(NULL, 0, FIFO_NAME_TEMPL, i, "in");
		if (k < 0) {
			perror("snprintf");
			return EXIT_FAILURE;
		}
		in_fifos[i] = malloc(sizeof(char)*(k+1));
		if (in_fifos[i] == NULL) {
			perror("malloc");
			return EXIT_FAILURE;
		}
		k = snprintf(in_fifos[i], k+1, FIFO_NAME_TEMPL, i, "in");
		if (k < 0) {
			perror("snprintf");
			return EXIT_FAILURE;
		}
		k = snprintf(NULL, 0, FIFO_NAME_TEMPL, i, "out");
		if (k < 0) {
			perror("snprintf");
			return EXIT_FAILURE;
		}
		out_fifos[i] = malloc(sizeof(char)*(k+1));
		if (out_fifos[i] == NULL) {
			perror("malloc");
			return EXIT_FAILURE;
		}
		k = snprintf(out_fifos[i], k+1, FIFO_NAME_TEMPL, i, "out");
		if (k < 0) {
			perror("snprintf");
			return EXIT_FAILURE;
		}
	}
	for (i = 0; i < N; ++i) {
		k = mkfifo(in_fifos[i], S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (k != 0) {
			perror("mkfifo");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		k = mkfifo(out_fifos[i], S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (k != 0) {
			perror("mkfifo");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		in_fds[i] = -1;
		out_fds[i] = -1;
		pids[i] = -1;
		not_alive[i] = -1;
	}
	sigemptyset(&sigmask);
	psa->sa_flags = 0;
	psa->sa_handler = sa_sigchld;
	psa->sa_mask = sigmask;
	if (sigaction(SIGCHLD, psa, NULL) == -1) {
		perror("sigaction");
		EXIT_CODE = EXIT_FAILURE;
		goto kill_all;
	}
	sigaddset(&sigmask, SIGCHLD);
	if (sigprocmask(SIG_SETMASK, &sigmask, NULL)) {
		perror("sigprocmask");
		EXIT_CODE = EXIT_FAILURE;
		goto kill_all;
	}
	sigemptyset(&sigmask);
	for (i = 0; i < N; ++i) {
		pid = fork();
		if (pid < 0) {
			perror("fork");
			EXIT_CODE = EXIT_FAILURE;
			goto kill_all;
		}
		if (pid == 0) {
			/* child */
			if (MACHINE_LOG != 1) {
				close(MACHINE_LOG);
			}
			for (j = 0; j < i; ++j) {
				if (in_fds[i] >= 0) {
					close(in_fds[j]);
				}
				if (out_fds[i]) {
					close(out_fds[j]);
				}
			}
			fprintf(stderr, "Starting to reopen 0 and 1\n");
			k = open(in_fifos[i], O_WRONLY);
			if (k == -1) {
				perror("open");
				return EXIT_FAILURE;
			}
			dup2(k, 1);
			close(k);
			k = open(out_fifos[i], O_RDONLY);
			if (k == -1) {
				perror("open");
				return EXIT_FAILURE;
			}
			dup2(k, 0);
			close(k);
			k = getrlimit(RLIMIT_AS, lim_as);
			if (k < 0) {
				perror("getrlimit");
				return EXIT_FAILURE;
			}
			if (lim_as->rlim_max == RLIM_INFINITY || lim_as->rlim_max > MEMLIMIT) {
				lim_as->rlim_cur = MEMLIMIT;
				lim_as->rlim_max = MEMLIMIT;
				k = setrlimit(RLIMIT_AS, lim_as);
				if (k < 0) {
					perror("setrlimit");
					return EXIT_FAILURE;
				}
			}
			args[0] = *(argv+(ARG_N_POS+1)+i);
			fprintf(stderr, "Calling execv `%s` (pid %i)\n", args[0], getpid());
			if (execv(*(argv+(ARG_N_POS+1)+i), args) < 0) {
				perror("execv");
				return EXIT_FAILURE;
			}
		} else {
			/* parent */
			fprintf(stderr, "Forked %i\n", pid);
			pids[i] = pid;
			not_alive[i] = 0;
			in_fds[i] = open(in_fifos[i], O_RDONLY);
			if (in_fds[i] == -1) {
				perror("open");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			out_fds[i] = open(out_fifos[i], O_WRONLY);
			if (out_fds[i] == -1) {
				perror("open");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			/* record start time and calculate finish limit */
			if (gettimeofday(pstart+i, NULL) == -1) {
				perror("gettimeofday");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			(pstart+i)->tv_usec += TIMEOUT_MSEC % 1000 * 1000;
			(pstart+i)->tv_sec += TIMEOUT_MSEC / 1000 + (pstart+i)->tv_usec / 1000000;
			(pstart+i)->tv_usec %= 1000000;
			/* tell strategy number of rounds */
			buf_size = snprintf(buf, BUFSIZE, "%i\n", NUM_ROUNDS);
			if (buf_size < 0) {
				perror("snprintf");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			if (buf_size >= BUFSIZE) {
				fprintf(stdout, "Too many rounds, 'N' is longer, than %i symbols\n",
					BUFSIZE);
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			j = 0;
			do {
				k = write(out_fds[i], buf, buf_size - j);
				if (k < 0) {
					perror("write");
					EXIT_CODE = EXIT_FAILURE;
					goto kill_all;
				}
				j += k;
			} while (j < buf_size);
			(pfds+i)->fd = -1 * in_fds[i];
			(pfds+i)->events = POLLIN;
		}
	}
	for (round_num = 0; round_num < NUM_ROUNDS; ++round_num) {
		/* doing one round */
		memset(answered, 0, sizeof(int)*N);
		memset(ans, 0, sizeof(char)*N);
		/* reading strategies actions */
		for (i = 0; i < N; ++i) {
			(pfds+i)->fd = in_fds[i];
			(pfds+i)->events = POLLIN;
			*(buf_pos+i) = 0;
		}
		while (1) {
			/* This is not exactly time limit, as it is real time */
			if (gettimeofday(pnow, NULL) == -1) {
				perror("gettimeofday");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			if (pnow->tv_sec > (pstart+N-1)->tv_sec ||
				(pnow->tv_sec == (pstart+N-1)->tv_sec &&
				pnow->tv_usec >= (pstart+N-1)->tv_usec)) {
				/* may be change to smth else, for example, find out who timed out? */
				fprintf(stderr, "Strategies timed out, aborting\n");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			if ((pstart+N-1)->tv_usec < pnow->tv_usec) {
				tmo_p->tv_sec = (pstart+N-1)->tv_sec - pnow->tv_sec - 1;
				tmo_p->tv_nsec = 1000000000 +
					((pstart+N-1)->tv_usec - pnow->tv_usec) * 1000;
			} else {
				tmo_p->tv_sec = (pstart+N-1)->tv_sec - pnow->tv_sec;
				tmo_p->tv_nsec = ((pstart+N-1)->tv_usec - pnow->tv_usec) * 1000;
			}
			k = ppoll(pfds, N, tmo_p, &sigmask);
			if (k == -1 && errno != EINTR) {
				fprintf(stderr, "ppoll failed\n");
				perror("ppoll");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			for (i = 0; i < N; ++i) {
				if ((pfds+i)->fd > 0 && (pfds+i)->revents != 0) {
					if ((pfds+i)->revents & POLLERR) {
						fprintf(stdout, "Err on polling of %i strategy fd\n", i);
						EXIT_CODE = EXIT_FAILURE;
						goto kill_all;
					}
					if ((pfds+i)->revents & POLLIN) {
						k = read(in_fds[i], bufs+i*READ_SIZE+buf_pos[i], READ_SIZE-buf_pos[i]);
						if (k == -1) {
							perror("read");
							EXIT_CODE = EXIT_FAILURE;
							goto kill_all;
						}
						if (k == 0) {
							fprintf(stdout, "Process %i closed fd, aborting\n", pid);
							EXIT_CODE = EXIT_FAILURE;
							goto kill_all;
						}
						buf_pos[i] += k;
						if (buf_pos[i] == READ_SIZE) {
							if (*(bufs+i*READ_SIZE+1) != '\n') {
								fprintf(stdout, "Strategy %i printed invalid line terminator: `%c` (expected `\\n`), aborting\n", i, *(bufs+i*READ_SIZE+1));
								EXIT_CODE = EXIT_FAILURE;
								goto kill_all;
							}
							if (*(bufs+i*READ_SIZE) != 'c' && *(bufs+i*READ_SIZE) != 'd') {
								fprintf(stdout, "Strategy %i answered invalid action: `%c` (expected `c` or `d`), aborting\n", i, *(bufs+i*READ_SIZE));
								EXIT_CODE = EXIT_FAILURE;
								goto kill_all;
							}
							ans[i] = *(bufs+i*READ_SIZE);
							answered[i] = 1;
							/* mark fd to be ignored in subsequent polls, as got answer */
							(pfds+i)->fd = ~((pfds+i)->fd);
						}
					}
				}
			}
			if (got_SIGCHLD) {
				got_SIGCHLD = 0;
				fprintf(stdout, "Got SIGCHLD\n");
				for (i = 0; i < N; ++i) {
					if (not_alive[i]) {
						continue;
					}
					j = waitpid(pids[i], &k, WNOHANG);
					if (j < 0) {
						perror("waitpid");
					}
					if (j == 0) {
						continue;
					}
					fprintf(stdout, "waited for strategy %i (%i)\n", i, j);
					not_alive[i] = 1;
					retstatuses[i] = k;
				}
				if (round_num != NUM_ROUNDS - 1) {
					EXIT_CODE = EXIT_FAILURE;
					goto kill_all;
				}
				for (i = 0; i < N; ++i) {
					if (!answered[i] && not_alive[i]) {
						EXIT_CODE = EXIT_FAILURE;
						goto kill_all;
					}
				}
			}
			k = 1;
			for (i = 0; i < N; ++i) {
				k = k & answered[i];
			}
			if (k == 1) {
				break;
			}
		}
		memset(answered, 0, sizeof(int)*N);
		/* telling the strategies other answers */
		buf_size = snprintf(buf, BUFSIZE, " \n");
		if (buf_size < 0) {
			perror("snprintf");
			EXIT_CODE = EXIT_FAILURE;
			goto kill_all;
		}
		for (i = 0; i < N; ++i) {
			(pfds+i)->fd = out_fds[i];
			(pfds+i)->events = POLLOUT;
			*(bufs+READ_SIZE*i) = ans[1-i];
			*(bufs+READ_SIZE*i+1) = '\n';
			buf_pos[i] = 0;
		}
		while (1) {
			/* This is not exactly time limit, as it is real time */
			if (gettimeofday(pnow, NULL) == -1) {
				perror("gettimeofday");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			if (pnow->tv_sec > (pstart+N-1)->tv_sec ||
				(pnow->tv_sec == (pstart+N-1)->tv_sec &&
				pnow->tv_usec >= (pstart+N-1)->tv_usec)) {
				/* may be change to smth else, for example, find out who timed out? */
				fprintf(stderr, "Writing to strategies timed out, aborting\n");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			if ((pstart+N-1)->tv_usec < pnow->tv_usec) {
				tmo_p->tv_sec = (pstart+N-1)->tv_sec - pnow->tv_sec - 1;
				tmo_p->tv_nsec = 1000000000 +
					((pstart+N-1)->tv_usec - pnow->tv_usec) * 1000;
			} else {
				tmo_p->tv_sec = (pstart+N-1)->tv_sec - pnow->tv_sec;
				tmo_p->tv_nsec = ((pstart+N-1)->tv_usec - pnow->tv_usec) * 1000;
			}
			k = ppoll(pfds, N, tmo_p, &sigmask);
			if (k == -1 && errno != EINTR) {
				fprintf(stderr, "ppoll failed\n");
				perror("ppoll");
				EXIT_CODE = EXIT_FAILURE;
				goto kill_all;
			}
			for (i = 0; i < N; ++i) {
				if ((pfds+i)->fd > 0 && (pfds+i)->revents != 0) {
					if ((pfds+i)->revents & POLLERR) {
						fprintf(stdout, "Err on polling of %i strategy fd\n", i);
						EXIT_CODE = EXIT_FAILURE;
						goto kill_all;
					}
					if ((pfds+i)->revents & POLLOUT) {
						k = write(out_fds[i], bufs+READ_SIZE*i+buf_pos[i], READ_SIZE);
						if (k < -1) {
							perror("write");
							EXIT_CODE = EXIT_FAILURE;
							goto kill_all;
						}
						buf_pos[i] += k;
						if (buf_pos[i] == 2) {
							(pfds+i)->fd = ~((pfds+i)->fd);
							answered[i] = 1;
						}
					}
				}
			}
			if (got_SIGCHLD) {
				got_SIGCHLD = 0;
				fprintf(stdout, "Got SIGCHLD\n");
				for (i = 0; i < N; ++i) {
					if (not_alive[i]) {
						continue;
					}
					j = waitpid(pids[i], &k, WNOHANG);
					if (j < 0) {
						perror("waitpid");
					}
					if (j == 0) {
						continue;
					}
					fprintf(stdout, "waited for strategy %i (%i)\n", i, j);
					not_alive[i] = 1;
					retstatuses[i] = k;
				}
				if (round_num != NUM_ROUNDS - 1) {
					EXIT_CODE = EXIT_FAILURE;
					goto kill_all;
				}
				for (i = 0; i < N; ++i) {
					if (!answered[i] && not_alive[i]) {
						EXIT_CODE = EXIT_FAILURE;
						goto kill_all;
					}
				}
			}
			k = 1;
			for (i = 0; i < N; ++i) {
				k = k & answered[i];
			}
			if (k == 1) {
				break;
			}
		}
		for (i = 0; i < N; ++i) {
			if (answered[i] == 0) {
				fprintf(stdout, "%i-th strategy %s: %c\n", i, answered[i] ?
				"finished" : "not finished", ans[i]);
			}
		}
		for (i = 0; i < N; ++i) {
			gamelog[gamelog_pos++] = ans[i];
			gamelog[gamelog_pos++] = (i == N-1) ? '\n' : ' ';
		}
	} /* one round routine finished */
kill_all:
	j = 0;
	do {
		k = write(MACHINE_LOG, gamelog+j, gamelog_pos-j);
		if (k < 0) {
			perror("write");
			EXIT_CODE = EXIT_FAILURE;
			goto kill_all;
		}
		j += k;
	} while (j < gamelog_pos);
	if (MACHINE_LOG != 1) {
		if (fsync(MACHINE_LOG) < 0) {
			perror("fsync");
		}
		close(MACHINE_LOG);
	}
	for (i = 0; i < N; ++i) {
		if (in_fds[i] >= 0) {
			close(in_fds[i]);
		}
		if (out_fds[i] >= 0) {
			close(out_fds[i]);
		}
	}
	if (gettimeofday(pfinish, NULL) == -1) {
		perror("gettimeofday");
		EXIT_CODE = EXIT_FAILURE;
		goto cleanup;
	}
	pfinish->tv_usec += WAIT_SIGTERM % 1000 * 1000;
	pfinish->tv_sec += WAIT_SIGTERM / 1000 + pfinish->tv_usec / 1000000;
	pfinish->tv_usec %= 1000000;
	while (1) {
		k = 0;
		for (i = 0; i < N; ++i) {
			if (!not_alive[i]) {
				k = 1;
				break;
			}
		}
		if (k == 0) {
			break;
		}
		if (gettimeofday(pnow, NULL) == -1) {
			perror("gettimeofday");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (pnow->tv_sec > pfinish->tv_sec ||
			(pnow->tv_sec == pfinish->tv_sec &&
			pnow->tv_usec >= pfinish->tv_usec)) {
			break;
		}
		if (pfinish->tv_usec < pnow->tv_usec) {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec - 1;
			tmo_p->tv_nsec = 1000000000 +
				(pfinish->tv_usec - pnow->tv_usec) * 1000;
		} else {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec;
			tmo_p->tv_nsec = (pfinish->tv_usec - pnow->tv_usec) * 1000;
		}
		k = ppoll(pfds, 0, tmo_p, &sigmask);
		if (k == -1 && errno != EINTR) {
			fprintf(stderr, "ppoll failed\n");
			perror("ppoll");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (got_SIGCHLD) {
			got_SIGCHLD = 0;
			for (i = 0; i < N; ++i) {
				if (not_alive[i]) {
					continue;
				}
				j = waitpid(pids[i], &k, WNOHANG);
				if (j < 0) {
					perror("waitpid");
					EXIT_CODE = EXIT_FAILURE;
					goto cleanup;
				}
				if (j == 0) {
					continue;
				}
				fprintf(stdout, "waited for strategy %i (%i)\n", i, j);
				not_alive[i] = 1;
				retstatuses[i] = k;
			}
		}
	}
	for (i = 0; i < N; ++i) {
		if (not_alive[i]) {
			continue;
		}
		if (pids[i] > 0) {
			fprintf(stdout, "Sending SIGTERM to strategy %i (%i)\n", i, pids[i]);
			kill(pids[i], SIGTERM);
			continue;
		}
		if (pids[i] <= 0) {
			fprintf(stdout, "Bad pid (%i) of %i strategy\n", pids[i], i);
		}
	}
	if (gettimeofday(pfinish, NULL) == -1) {
		perror("gettimeofday");
		EXIT_CODE = EXIT_FAILURE;
		goto cleanup;
	}
	pfinish->tv_usec += WAIT_SIGTERM % 1000 * 1000;
	pfinish->tv_sec += WAIT_SIGTERM / 1000 + pfinish->tv_usec / 1000000;
	pfinish->tv_usec %= 1000000;
	while (1) {
		k = 0;
		for (i = 0; i < N; ++i) {
			if (!not_alive[i]) {
				k = 1;
				break;
			}
		}
		if (k == 0) {
			break;
		}
		if (gettimeofday(pnow, NULL) == -1) {
			perror("gettimeofday");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (pnow->tv_sec > pfinish->tv_sec ||
			(pnow->tv_sec == pfinish->tv_sec &&
			pnow->tv_usec >= pfinish->tv_usec)) {
			break;
		}
		if (pfinish->tv_usec < pnow->tv_usec) {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec - 1;
			tmo_p->tv_nsec = 1000000000 +
				(pfinish->tv_usec - pnow->tv_usec) * 1000;
		} else {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec;
			tmo_p->tv_nsec = (pfinish->tv_usec - pnow->tv_usec) * 1000;
		}
		k = ppoll(pfds, 0, tmo_p, &sigmask);
		if (k == -1 && errno != EINTR) {
			fprintf(stderr, "ppoll failed\n");
			perror("ppoll");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (got_SIGCHLD) {
			got_SIGCHLD = 0;
			for (i = 0; i < N; ++i) {
				if (not_alive[i]) {
					continue;
				}
				j = waitpid(pids[i], &k, WNOHANG);
				if (j < 0) {
					perror("waitpid");
					EXIT_CODE = EXIT_FAILURE;
					goto cleanup;
				}
				if (j == 0) {
					continue;
				}
				fprintf(stdout, "waited for strategy %i (%i)\n", i, j);
				not_alive[i] = 1;
				retstatuses[i] = k;
			}
		}
	}
	for (i = 0; i < N; ++i) {
		if (pids[i] > 0 && !not_alive[i]) {
			fprintf(stdout, "Sending SIGKILL to strategy %i (%i)\n", i, pids[i]);
			kill(pids[i], SIGKILL);
		}
	}
	if (gettimeofday(pfinish, NULL) == -1) {
		perror("gettimeofday");
		goto cleanup;
	}
	pfinish->tv_usec += WAIT_SIGKILL % 1000 * 1000;
	pfinish->tv_sec += WAIT_SIGKILL / 1000 + pfinish->tv_usec / 1000000;
	pfinish->tv_usec %= 1000000;
	while (1) {
		k = 0;
		for (i = 0; i < N; ++i) {
			if (!not_alive[i]) {
				k = 1;
				break;
			}
		}
		if (k == 0) {
			break;
		}
		if (gettimeofday(pnow, NULL) == -1) {
			perror("gettimeofday");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (pnow->tv_sec > pfinish->tv_sec ||
			(pnow->tv_sec == pfinish->tv_sec &&
			pnow->tv_usec >= pfinish->tv_usec)) {
			break;
		}
		if (pfinish->tv_usec < pnow->tv_usec) {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec - 1;
			tmo_p->tv_nsec = 1000000000 +
				(pfinish->tv_usec - pnow->tv_usec) * 1000;
		} else {
			tmo_p->tv_sec = pfinish->tv_sec - pnow->tv_sec;
			tmo_p->tv_nsec = (pfinish->tv_usec - pnow->tv_usec) * 1000;
		}
		k = ppoll(pfds, 0, tmo_p, &sigmask);
		if (k == -1 && errno != EINTR) {
			fprintf(stderr, "ppoll failed\n");
			perror("ppoll");
			EXIT_CODE = EXIT_FAILURE;
			goto cleanup;
		}
		if (got_SIGCHLD) {
			got_SIGCHLD = 0;
			for (i = 0; i < N; ++i) {
				if (not_alive[i]) {
					continue;
				}
				j = waitpid(pids[i], &k, WNOHANG);
				if (j < 0) {
					perror("waitpid");
					EXIT_CODE = EXIT_FAILURE;
					goto cleanup;
				}
				if (j == 0) {
					continue;
				}
				fprintf(stdout, "waited for strategy %i (%i)\n", i, j);
				not_alive[i] = 1;
				retstatuses[i] = k;
			}
		}
	}
cleanup:
	for (i = 0; i < N; ++i) {
		if (not_alive[i] == -1) {
			fprintf(stdout, "%i strategy was not started\n", i);
			continue;
		}
		if (not_alive[i]) {
			if (WIFEXITED(retstatuses[i])) {
				fprintf(stdout, "%i strategy (%i) exited with %i\n", i, pids[i], WEXITSTATUS(retstatuses[i]));
			}
			if (WIFSIGNALED(retstatuses[i])) {
				fprintf(stdout, "%i strategy (%i) killed by %i\n", i, pids[i], WTERMSIG(retstatuses[i]));
			}
		}
	}
	for (i = 0; i < N; ++i) {
		k = unlink(in_fifos[i]);
		if (k != 0) {
			perror("unlink");
			return EXIT_FAILURE;
		}
		k = unlink(out_fifos[i]);
		if (k != 0) {
			perror("unlink");
			return EXIT_FAILURE;
		}
	}
	for (i = 0; i < N; ++i) {
		free(in_fifos[i]);
		free(out_fifos[i]);
	}
	free(in_fifos);
	free(out_fifos);
	free(in_fds);
	free(out_fds);
	free(bufs);
	free(buf_pos);
	free(pfds);
	free(tmo_p);
	free(pfinish);
	free(pstart);
	free(pnow);
	free(psa);
	free(lim_as);
	free(not_alive);
	free(pids);
	free(retstatuses);
	free(ans);
	free(answered);
	return EXIT_CODE;
}
