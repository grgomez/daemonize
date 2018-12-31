#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/stat.h>

#define LOCKFILE "/var/run/daemon-testd.pid"
#define LOCKMODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define ATTACH_TO_DEV_NULL "/dev/null"
#define WORKING_DIR "/"

/* close all file descriptors */
void closefd(struct rlimit * rl)
{
	/* close all file descriptors */
	if (RLIM_INFINITY == rl->rlim_max)
	{
		rl->rlim_max = 1024;
	}

	for (int i = 0; i < rl->rlim_max; i++)
	{
		close(i);
	}
}

/* Attach file descriptors 0, 1, and 2 to /dev/null */
/*
 * 0 = stdin
 * 1 = stdout
 * 2 s stderr
 * 
 * Upon failure, return -1 otherwise 0/
 */
int attachfd(void)
{
	int fd0, fd1, fd2;

	fd0 = open(ATTACH_TO_DEV_NULL, O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);

	if (-1 == fd0 || -1 == fd1 || -1 == fd2)
	{
		return -1;
	}

	return 0;
}

void sighandler(int sig)
{
	switch(sig)
	{
		case SIGTERM:
			exit(EXIT_SUCCESS);
			break;
	}
}

/* Disable signals that are no longer needed and catch other ones */
void setsignals(void)
{
       signal(SIGCHLD, SIG_IGN); /* ignore child */
       signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
       signal(SIGTTOU, SIG_IGN); /* ignore terminal output signals */
       signal(SIGTTIN, SIG_IGN); /* ignore terminal input signals */
       signal(SIGHUP, SIG_IGN); /* ignore hangup signals */
       signal(SIGTERM, sighandler); /* catch termination signals */
}

/*
 * Try to create a write lock, if it fails then
 * it will populate errno
 */
int lockfile(int fd)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_start = 0;
	fl.l_whence = SEEK_SET;
	fl.l_len = 0;

	return (fcntl(fd, F_SETLK, &fl));
}

void daemonize(void)
{
	pid_t pid;
	struct rlimit rl;
	int lock_fd;
	char buf[16];

	if (1 == getppid())
	{
		fprintf(stdout, "Is already a daemon.\n");
		exit(EXIT_FAILURE);
	}
	
	/* clear file creation mask */
	umask(0);

	/* Get maximum number of file descriptors */
	if (0 > getrlimit(RLIMIT_NOFILE, &rl))
	{
		fprintf(stdout, "Can't get file limit.\n");
		exit(EXIT_FAILURE);
	}

	if (0 > (pid = fork()))
	{
		fprintf(stdout, "Can't fork.\n");
		exit(EXIT_FAILURE);
	}

	if (0 < pid)
	{
		/* parent exits */
		exit(EXIT_SUCCESS);
	}

	/* daemon continues */
	setsid(); /* obtain a new process group */

	closefd(&rl); /* close all file descriptors */
	
	/* Initialize the log file */
	openlog(NULL, LOG_CONS|LOG_PID, LOG_DAEMON);
	
	syslog(LOG_NOTICE, "Daemon started - Continuing init...");

	if (attachfd())
	{
		syslog(LOG_ERR, 
			"Could not attach file descriptors 0, 1, and 2 to /dev/null: %s", 
			strerror(errno));

		exit(EXIT_FAILURE);
	}

	/* reset file permissions -- set file creation mode to 750 */
	/*umask(027);*/

	/* 
	 * Change the current directory to the root.
	 * so we won't prevent file systems from being unmounted.
	 */
       if (0 > chdir(WORKING_DIR))
       {
		syslog(LOG_ERR, 
			"Can't change directory to %s", WORKING_DIR); 

		exit(EXIT_FAILURE);
       }	       

       /* disable signals and set signal handlers */
	setsignals();

       /* only one daemon can run at a time */
       lock_fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE);
       if (0 >= lock_fd)
       {
		syslog(LOG_ERR,
			"Can't open %s: %s", 
			LOCKFILE, strerror(errno));

		exit(EXIT_FAILURE);
       }

       if (0 > lockfile(lock_fd))
       {
		if (EACCES == errno || EAGAIN == errno)
		{
			close(lock_fd);
			
			syslog(LOG_ERR,
				"The lock file %s is already open",
				LOCKFILE);

			exit(EXIT_FAILURE);
		}

		syslog(LOG_ERR, 
			"Can't lock %s: %s",
			LOCKFILE, strerror(errno));
       }

       if (0 > ftruncate(lock_fd, 0))
       {	
	       syslog(LOG_ERR,
			"Can't truncate lock %s: %s",
			LOCKFILE, strerror(errno));
       }

       sprintf(buf, "%ld", (long)getpid());
       if (0 > write(lock_fd, buf, strlen(buf)+1))
       {
		syslog(LOG_ERR,
			"Can't write into  lock %s: %s",
			LOCKFILE, strerror(errno));

       }

       syslog(LOG_NOTICE, "Daemonization done!");
}

int main(int argc, char ** argv)
{
	daemonize();
	while(1) 
	{
		syslog(LOG_NOTICE, "Hello world!");
		sleep(1);
	}

	return EXIT_SUCCESS;
}
