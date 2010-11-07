#define _GNU_SOURCE
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/select.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <pty.h>

#include <pthread.h>

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))


int master_controlling_tty;
int master_pty;
int input[2];
int slave_pty;
char *in_file;

int pid;

struct termios term_settings;
struct termios orig_settings;
struct winsize term_size;

void sigwinch_handler(int sig_num);
void sigterm_handler(int sig_num);

static void *input_spooler();


int main(int argc, char *argv[])
{
	char *term = NULL;
	pthread_t thread;

	struct sigaction chld;
	struct sigaction winch;
	struct sigaction sigtrm;
	struct sigaction sigint;
	struct sigaction sighup;

	memset(&chld, 0, sizeof(chld));
	memset(&winch, 0, sizeof(winch));
	memset(&sigtrm, 0, sizeof(sigtrm));
	memset(&sigint, 0, sizeof(sigint));
	memset(&sighup, 0, sizeof(sighup));
	chld.sa_handler = &sigterm_handler;
	winch.sa_handler = &sigwinch_handler;
	sigtrm.sa_handler = &sigterm_handler;
	sigint.sa_handler = &sigterm_handler;
	sighup.sa_handler = &sigterm_handler;

	term = getenv("TERM");

	fclose(stdin);
	close(STDIN_FILENO);

	master_controlling_tty = open("/dev/tty", O_RDWR | O_NOCTTY);
	ioctl(master_controlling_tty, TIOCGWINSZ, &term_size); /* save terminal size */


	if(argc != 2) {
		fprintf(stderr, "Improper number of arguments.\n");
		exit(EXIT_FAILURE);
	}
	in_file = argv[1];

	if((pid = forkpty(&master_pty, NULL, NULL, &term_size)) == 0) {  /* if child */
		setenv("TERM", term, 1);
		system("vim -N -u NONE");
		exit(EXIT_FAILURE);
	}

	tcgetattr(master_controlling_tty, &term_settings);
	tcgetattr(master_controlling_tty, &orig_settings);
	cfmakeraw(&term_settings);
	tcsetattr(master_controlling_tty, TCSANOW, &term_settings);


	sigaction(SIGCHLD, &chld, NULL);
	sigaction(SIGWINCH, &winch, NULL);
	sigaction(SIGTERM, &sigtrm, NULL);
	sigaction(SIGINT, &sigint, NULL);
	sigaction(SIGHUP, &sighup, NULL);

	pipe(input);

	pthread_create(&thread, NULL, input_spooler, NULL);


	while(1) {
		int nfds=0;
		int r;
		int ret;
		char buf[BUFSIZ];

		fd_set rd, wr, er;
		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&er);

		nfds = max(nfds, input[0]);
		FD_SET(input[0], &rd);

		nfds = max(nfds, master_pty);
		FD_SET(master_pty, &rd);

		r = select(nfds+1, &rd, &wr, &er, NULL);

		if(r == -1) {
			continue;
		}

		if(FD_ISSET(input[0], &rd)) {
			ret = read(input[0], buf, BUFSIZ*sizeof(char));
			write(master_pty, buf, ret*sizeof(char));
		}
		if(FD_ISSET(master_pty, &rd)) {
			ret = read(master_pty, buf, BUFSIZ*sizeof(char));
			write(STDOUT_FILENO, buf, ret*sizeof(char));
		}
	}

	return 0;
}

static void *input_spooler()
{
	int c;
	FILE *in = fopen(in_file, "r");

	/* write input onto the input pipe in an entertaining fashion. */
	while((c = fgetc(in)) != EOF) {
		write(input[1], &c, 1);

		if(c == 27)
			usleep(200000);
		else
			usleep(100000);

	}

	fclose(in);

	return NULL;
}

void sigterm_handler(int sig_num)
{
	tcsetattr(master_controlling_tty, TCSANOW, &orig_settings);
	exit(EXIT_SUCCESS);
	return;
}

void sigwinch_handler(int sig_num)
{
	ioctl(master_controlling_tty, TIOCGWINSZ, &term_size);  /* save new terminal size */
	ioctl(slave_pty, TIOCSWINSZ, &term_size);  /* set terminal size */

	kill(pid, SIGWINCH);  /* send resize signal to child */
}
