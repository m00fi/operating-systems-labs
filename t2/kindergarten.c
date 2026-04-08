#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// MAKRA
#define ERR(source) \
(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

// STRUKTURA DZIECKA
typedef struct child {
	pid_t pid;
	int couched_count;
	int in_kg;
} child_t;

// DEFINICJE FUNKCJI
void usage(char *name);
void make_children(int n, int k, int p);
int child_work(int k, int p, int _is_ill);
void parent_work(int t, child_t children[]);
void set_handler(void (*f)(int), int signum);
void set_usr_handler(void (*f)(int, siginfo_t*, void*), int signum) ;
void sig_alarm(int signum);
void sig_usr1(int signum, siginfo_t *info, void *context);
void sig_term(int signum);
void print_kg_status(child_t children[], int n);

// ZMIENNE GLOBALNE
volatile sig_atomic_t is_alarm = 0;
volatile sig_atomic_t is_ill = 0;
volatile sig_atomic_t got_term = 0;
volatile sig_atomic_t coughed = 0;



// MAIN
int main(int argc, char **argv) {
	if (argc < 5) {
		usage(argv[0]);
	}

	// wczytanie argumentów
	int t, k, n, p;
	t = atoi(argv[1]); // czas symulacji (s)
	k = atoi(argv[2]); // czas przyjechania po chore dziecko (s)
	n = atoi(argv[3]); // liczba dzieci
	p = atoi(argv[4]); // procent na zachorowanie (%)

	set_handler(SIG_IGN, SIGTERM);
	set_handler(SIG_IGN, SIGUSR1);
	set_handler(sig_alarm, SIGALRM);

	make_children(n, k, p);

	child_t children[n];
	parent_work(t, children);
	print_kg_status(children, n);

	return EXIT_SUCCESS;
}





// INICJALIZACJA FUNKCJI
void usage(char *name) {
	fprintf(stderr, "USAGE: %s m b s \n", name);
	exit(EXIT_FAILURE);
}

void make_children(int n, int k, int p) {
	for (int i = 0; i < n; ++i) {
		switch (fork()) {
			case 0:
				set_usr_handler(sig_usr1, SIGUSR1);
				set_handler(sig_term, SIGTERM);
				int status = child_work(k, p, i == 0 ? 1 : 0);
				exit(status);

			case -1:
				ERR("Fork: ");
		}
	}
}

int child_work(int k, int p, int _is_ill) {
	TEMP_FAILURE_RETRY(printf("Child[%ld] starts day in the kindergarten, ill: %s\n", (long)getpid(), _is_ill ? "yes" : "no"));

	long pid = (long) getpid();
	int cough_counter = 0;
	srand(time(NULL) * getpid());

	is_ill = _is_ill;
	if (is_ill) {
		// jest chore
		set_handler(SIG_IGN, SIGUSR1);
		alarm(k);
	}

	struct timespec ts = {0, 0}, rem;

	while (got_term == 0) {
		if (is_ill) {
			ts.tv_nsec = (50 + rand() % 151) * 1000000;
			while (nanosleep(&ts, &rem) == -1) {
				if (errno == EINTR) {
					ts = rem;
				}
				else {
					ERR("Nanosleep: ");
				}
			}

			// czy nie zostało odebrane przez rodziców
			if (is_alarm) {
				break;
			}

			// wysłanie sygnału do innych
			++cough_counter;
			TEMP_FAILURE_RETRY(printf("Child[%ld] is coughing %d. time\n", pid, cough_counter));
			if (kill(0, SIGUSR1) < 0) {
				ERR("Kill: ");
			}
		}
		else if (coughed) {
			TEMP_FAILURE_RETRY(printf("Child[%ld]: %ld has coughed at me!\n", pid, (long)coughed));

			if (rand() % 100 < p) {
				// zachorowano
				TEMP_FAILURE_RETRY(printf("Child[%ld] get sick!\n", pid));
				is_ill = 1;
				set_handler(SIG_IGN, SIGUSR1);
				alarm(k);
			}

			coughed = 0;
		}
	}

	TEMP_FAILURE_RETRY(printf("Child[%ld] exits with %d\n", pid, cough_counter));
	return cough_counter;
}

void parent_work(int t, child_t children[]) {
	TEMP_FAILURE_RETRY(printf("KG[%ld]: Alarm has been set for %d sec\n", (long)getpid(), t));
	alarm(t);

	pid_t child_pid;
	int status;
	int iter = 0;
	while (is_alarm == 0) {
		while ((child_pid = waitpid(-1, &status, WNOHANG)) > 0) {
			// dziecko odebrane przez rodzica
			children[iter].pid = child_pid;
			if (!WIFEXITED(status)) {
				ERR("Waitpid: ");
			}
			children[iter].couched_count = WEXITSTATUS(status);
			children[iter].in_kg = 0;
			iter++;
		}
		if (child_pid < 0 && errno == ECHILD) {
			break;
		}
	}

	// zebranie dzieci i zakończenie symulacji
	if (kill(0, SIGTERM) < 0) {
		ERR("Kill: ");
	}

	while ((child_pid = wait(&status)) > 0) {
		// dziecko zostało w przedszkolu
		children[iter].pid = child_pid;
		if (!WIFEXITED(status)) {
			ERR("Waitpid: ");
		}
		children[iter].couched_count = WEXITSTATUS(status);
		children[iter].in_kg = 1;
		iter++;
	}

	TEMP_FAILURE_RETRY(printf("KG[%ld]: Simulation has ended!\n", (long)getpid()));
}

void set_handler(void (*f)(int), int signum) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));

	act.sa_handler = f;

	if (-1 == sigaction(signum, &act, NULL)) {
		ERR("Sigaction: ");
	}
}

void set_usr_handler(void (*f)(int, siginfo_t*, void*), int signum) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));

	if (signum != SIGUSR1) {
		ERR("Used wrong func. Use set_handler instead");
	}

	act.sa_flags = SA_SIGINFO;
	act.sa_sigaction = f;

	if (-1 == sigaction(signum, &act, NULL)) {
		ERR("Sigaction: ");
	}
}

void sig_alarm(int signum) {
	is_alarm = 1;
}

void sig_usr1(int signum, siginfo_t *info, void *context) {
	if (info->si_code == SI_USER) {
		coughed = info->si_pid;
	}
}

void sig_term(int signum) {
	got_term = 1;
}

void print_kg_status(child_t children[], int n) {
	TEMP_FAILURE_RETRY(printf("No. | Child ID | Status\n"));

	int kg_count = 0;
	for (int i = 0; i < n; ++i) {
		TEMP_FAILURE_RETRY(printf("%3d | %8ld | Coughed %d times and ", i+1, (long) children[i].pid, children[i].couched_count));
		if (children[i].in_kg == 1) {
			++kg_count;
			TEMP_FAILURE_RETRY(printf("is still in the kindergarten!\n"));
		}
		else {
			TEMP_FAILURE_RETRY(printf("parents picked them up!\n"));
		}
	}

	TEMP_FAILURE_RETRY(printf("%d out of %d children stayed in the kindergarten!\n", kg_count, n));
}