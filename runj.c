/* runj - run commands in parallel
 * Copyright 2025 kmx.io <contact@kmx.io>
 **/
#include <assert.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#if (defined(__APPLE__) ||  \
     defined(__FreeBSD__) || \
     defined(__NetBSD__) || \
     defined(__OpenBSD__))
# include <sys/sysctl.h>
#else
# include <sys/sysinfo.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

#define TIMEOUT_SEC 0
#define TIMEOUT_USEC (10 * 1000)

int kill(pid_t pid, int sig);

static int find_pid (pid_t key, pid_t *pids, size_t pids_size);
int        main (int argc, char **argv);
static int ncpu (void);
static int runj (int count, char **argv);
static int runj_rx (int fd_read, FILE *fp_write);
static int runj_tx (FILE *fp_read, int fd_write);
static int usage (char *argv0);

static int find_pid (pid_t key, pid_t *pids, size_t pids_size)
{
	int count = pids_size / sizeof(pid_t);
	int i = 0;
	while (i < count) {
		if (pids[i] == key)
			return i;
		i++;
	}
	return -1;
}

int main (int argc, char **argv)
{
	int count;
	if (! argc || ! argv || ! argv[0])
		return usage("runj");
	if (argc < 2)
		return usage(argv[0]);
	count = atoi(argv[1]);
	if (! count || count < -1)
		return usage(argv[0]);
	if (count == -1)
		count = ncpu();
	return runj(count, argv + 2);
}

static int ncpu (void)
{
#if (defined(__APPLE__) ||  \
     defined(__FreeBSD__) || \
     defined(__NetBSD__) ||  \
     defined(__OpenBSD__))
	int mib[2];
	int hw_ncpu;
	unsigned long len;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	len = sizeof(hw_ncpu);
	if (sysctl(mib, 2, &hw_ncpu, &len, NULL, 0) == -1)
		err(1, "ncpu: sysctl(hw.ncpu)");
	return hw_ncpu;
#else
	return get_nprocs();
#endif
}

static int runj (int count, char **argv)
{
	int eof = 0;
	int exited = 0;
	fd_set fds_read;
	fd_set fds_write;
	int fd_max = 0;
	int i;
	pid_t *pid;
	int *pipe_fd;
	int *pipe_fd_i;
	int r;
	int status;
	const struct timeval timeout_init = {
	  TIMEOUT_SEC, TIMEOUT_USEC
	};
	struct timeval timeout_select = {0};
	pid_t wpid;
	if (count <= 0 || ! argv || ! *argv)
		return 1;
	if (! (pid = calloc(count, sizeof(pid_t))))
		return 1;
	if (! (pipe_fd = calloc(count, sizeof(int) * 4)))
		return 1;
	pipe_fd_i = pipe_fd;
	i = 0;
	while (i < count) {
		if (pipe(pipe_fd_i)) {
			warn("pipe 1");
			i++;
			r = 1;
			goto stop;
		}
		if (pipe(pipe_fd_i + 2)) {
			warn("pipe 2");
			i++;
			r = 1;
			goto stop;
		}
		if ((pid[i] = fork()) < 0) {
			warn("fork");
			i++;
			r = 1;
			goto stop;
		}
		if (! pid[i]) {
			dup2(pipe_fd_i[0], 0);
			close(pipe_fd_i[1]);
			close(pipe_fd_i[2]);
			dup2(pipe_fd_i[3], 1);
			if (0)
				fprintf(stderr, "runj: child: %s\n",
					argv[0]);
			execvp(argv[0], argv);
			err(1, "runj: execvp: %s", argv[0]);
			_exit(1);
		}
		close(pipe_fd_i[0]);
		close(pipe_fd_i[3]);
		pipe_fd_i += 4;
		i++;
	}
	while (exited < count && eof < count * 2) {
		FD_ZERO(&fds_read);
		FD_ZERO(&fds_write);
		fd_max = 0;
		pipe_fd_i = pipe_fd;
		i = 0;
		while (i < count) {
			if (pipe_fd_i[1] >= 0) {
				FD_SET(pipe_fd_i[1], &fds_write);
				if (pipe_fd_i[1] + 1 > fd_max)
					fd_max = pipe_fd_i[1] + 1;
			}
			if (pipe_fd_i[2] >= 0) {
				FD_SET(pipe_fd_i[2], &fds_read);
				if (pipe_fd_i[2] + 1 > fd_max)
					fd_max = pipe_fd_i[2] + 1;
			}
			pipe_fd_i += 4;
			i++;
		}
		timeout_select = timeout_init;
		if (select(fd_max, &fds_read, &fds_write, NULL,
			   &timeout_select) > 0) {
			if (0)
				fprintf(stderr, "select\n");
			pipe_fd_i = pipe_fd;
			i = 0;
			while (i < count) {
				if (pipe_fd_i[2] >= 0 &&
				    FD_ISSET(pipe_fd_i[2], &fds_read))
					if (runj_rx(pipe_fd_i[2],
						    stdout) <= 0) {
						if (0)
							fprintf(stderr,
								"runj: close r %d\n",
								pipe_fd_i[2]);
						close(pipe_fd_i[2]);
						pipe_fd_i[2] = -1;
						eof++;
					}
				if (pipe_fd_i[1] >= 0 &&
				    FD_ISSET(pipe_fd_i[1], &fds_write))
					if (runj_tx(stdin,
						    pipe_fd_i[1]) <= 0) {
						if (0)
							fprintf(stderr,
								"runj: close w %d\n",
								pipe_fd_i[1]);
						close(pipe_fd_i[1]);
						pipe_fd_i[1] = -1;
						eof++;
					}
				pipe_fd_i += 4;
				i++;
			}
		}
		if ((wpid = waitpid(-1, &status, WNOHANG)) > 0 &&
		    WIFEXITED(status)) {
			if ((i = find_pid(wpid, pid, sizeof(pid))) < 0) {
				if (0)
					warnx("runj: find_pid exit");
				continue;
			}
			pid[i] = 0;
			if ((r = WEXITSTATUS(status))) {
				i = count;
				goto stop;
			}
			if (0)
				fprintf(stderr, "runj: %d: ok\n",
					pipe_fd[i * 4 + 1]);
			exited++;
		}
		else if (wpid > 0 && WIFSIGNALED(status)) {
			if ((i = find_pid(wpid, pid, sizeof(pid))) < 0)
				errx(1, "runj: find_pid signal");
			if (0)
				fprintf(stderr, "runj: %d: signal %d\n",
					pid[i], WTERMSIG(status));
			pid[i] = 0;
			i = count;
			r = 1;
			goto stop;
		}
	}
	pipe_fd_i = pipe_fd;
	i = 0;
	while (i < count) {
		close(pipe_fd_i[1]);
		close(pipe_fd_i[2]);
		pipe_fd_i += 4;
		i++;
	}
	return 0;
 stop:
	pipe_fd_i = pipe_fd + count - 1;
	while (i-- > 0) {
		if (pid[i]) {
			kill(pid[i], SIGINT);
			close(pipe_fd_i[1]);
			close(pipe_fd_i[2]);
		}
		pipe_fd_i -= 4;
 	}
	return r;
}

static int runj_rx (int fd_read, FILE *fp_write)
{
	char buf[1024 * 1024];
	ssize_t done;
	ssize_t r;
	ssize_t remaining;
	assert(fp_write);
	if (0)
		fprintf(stderr, "runj_rx\n");
	if ((r = read(fd_read, buf, sizeof(buf) - 1)) <= 0)
		return r;
	buf[r] = 0;
	if (0)
		fprintf(stderr, "runj_rx: %ld: %s\n", r, buf);
	done = 0;
	remaining = r;
	while (remaining > 0) {
		if ((r = fwrite(buf + done, 1, remaining, fp_write)) <= 0)
			err(1, "runj_rx: write");
		done += r;
		remaining -= r;
	}
	fflush(fp_write);
	return done;
}

static int runj_tx (FILE *fp_read, int fd_write)
{
	char buf[1024 * 1024];
	ssize_t done;
	ssize_t r;
	ssize_t remaining;
	assert(fp_read);
	if (! fgets(buf, sizeof(buf), fp_read)) {
		if (feof(fp_read))
			return 0;
		errx(1, "runj_tx: fgets");
	}
	if (0)
		fprintf(stderr, "runj_tx: %d: %s\n", fd_write, buf);
	done = 0;
	remaining = strlen(buf);
	while (remaining > 0) {
		if ((r = write(fd_write, buf + done, remaining)) <= 0)
			err(1, "runj_tx: write");
		done += r;
		remaining -= r;
	}
	return done;
}

static int usage (char *argv0)
{
	fprintf(stderr, "Usage: %s JOB_COUNT COMMAND ...\n", argv0);
	fprintf(stderr, "       %s -1 COMMAND ...\n", argv0);
	return 1;
}
