/*
	todo:
	use only one character instead of random()
	directional feature
*/

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define MAX_WORMS	256

extern char *optarg;
extern int optopt, optind;

/* option variables...defaults are set below */
int rows = -1;          /* number of screen rows */
int cols = -1;          /* number of screen columns */
#ifdef MSDOS
	int stupid = 3; /* stupid terminal kludge */
#else
	int stupid = 0; /* stupid terminal kludge */
#endif
int max_len = -1;       /* maximum worm length */
int start_chance = -1;  /* 1 in n chance of starting */
int max_speed = -1;     /* maximum value of n for 1/n, 1 = same speed */
int usleep_time = -1;   /* amount of time to sleep between iterations */
char x_color = 0;       /* foreground color, 0-7 */
#ifdef MSDOS
	int allow_keyb = 0;
#else
	int allow_keyb = 1;
#endif
int helpscreen = 0;

struct termios orig_attr;   /* Original state of the terminal */

void sigs(int raised) {
	volatile static int sig = 0;
	struct winsize termsize;

	if(sig) return;
	else sig = raised;

	if(sig == SIGTERM || sig == SIGINT) {
		printf("\e[0m\e[2J");
		printf("\e[%i;0f", rows-1);
		if(allow_keyb) tcsetattr(STDIN_FILENO, TCSANOW, &orig_attr);
		exit(0);
	} else if(sig == SIGWINCH) {
		if(ioctl(STDIN_FILENO, TIOCGWINSZ, &termsize) != -1) {
			rows = termsize.ws_row;
			cols = termsize.ws_col;
			if(rows < 1 || rows > MAX_WORMS) rows = 25;
			if(cols < 1 || cols > MAX_WORMS) cols = 80;
			printf("\e[H\e[J");
		}
	}

	sig = 0;
}

void usage() {
	printf("usage: m-code [-dhv] [-r rows] [-c columns] [-l max_len] [-p prob] [-s speeds]
[-t timing] [-x color] [-k level]

-c columns  Specify number of display columns.
-d          Disable keyboard input (default in DOS) - good for piping out
            to other terminals.
-h -?       help
-k level    Kludge mode:
                level = 1: don't print in the bottom right spot
                level = 2: don't use \\v
                level = 4: don't use hibit characters
            levels may be added togeather as follows:
            level = 3: (1+2) both.  (default in DOS)
-l max_len  Specify maximum length of a worm.
-p prob     Specify 1 in prob chance of beginning a new worm on blank lines.
-r rows     Specify number of display rows.
-s speeds   Specify speeds different increments of speeds to run at.
-t timing   Wait timing microseconds between each iteration.
-v          Show version
-x color    Specify color, 0-7, to use.  8 = randomize.  0 = no color codes.

Default:
m-code -r25 -c80 -l20 -p200 -s3 -t30000 -x2
");
	exit(1);
}

void put(int row, int col, char color, int bold) {
	static int cur_row = -1, cur_col = -1, cur_bold = 0;
	static char cur_color = 0;

	if(stupid&1 && row == rows && col == cols) return;
	if(row < 1 || row > rows || col < 1 || col > cols) return;

	if(!(stupid&2) && cur_row + 1 == row) { printf("\v"); cur_row++; }
	if(cur_row == row && cur_col - 1 == col) { printf("\b"); cur_col--; }
	if(cur_row != row || cur_col != col) {
		printf("\e[%i;%if",row,col);
		cur_row = row;
		cur_col = col;
	}

	if(!color) {
		printf(" ");
		cur_col++;
		if(cur_col > cols) cur_col = -1;
		return;
	}

	if(color != '0' && (cur_color != color || cur_bold != bold)) {
		printf("\e[");
		if(!cur_bold && bold) { printf("1"); }
		if(cur_bold && !bold) { printf("0"); cur_color = 0; }
		if(cur_color != color) {
			if(cur_bold != bold) printf(";");
			printf("3%c", color);
		}
		cur_bold = bold;
		cur_color = color;
		printf("m");
	}
	printf("%c", (char)((stupid&4) ? (random()%94+33) : (random()%222+33)));
	cur_col++;
	if(cur_col > cols) cur_col = -1;
	return;
}

int main(int argc, char *argv[]) {
	/* worm variables */
	int heads[MAX_WORMS];	/* stores the line the head is on */
	int tails[MAX_WORMS];	/* stores the line the tail is on */
	int speed[MAX_WORMS];	/* wait n ticks */
	int ticks[MAX_WORMS];	/* tick counter */
	char color[MAX_WORMS];	/* color indicator, '0' - '7' */
	int num_worms = 0, fcntl_flags;
	char key = 0;
	struct termios term_attr;
	int loop;

	signal(SIGINT, sigs);
	signal(SIGTERM, sigs);
	signal(SIGWINCH, sigs);
	kill(getpid(), SIGWINCH);

	while((key = getopt(argc, argv, "dr:c:?hk:l:p:s:t:vx:")) != -1) {
		switch(key) {
		case 'r':
			if(!optarg) usage();
			rows = atoi(optarg);
		break;
		case 'c':
			if(!optarg) usage();
			cols = atoi(optarg);
		break;
		case 'k':
			if(!optarg) usage();
			stupid = atoi(optarg);
		break;
		case 'l':
			if(!optarg) usage();
			max_len = atoi(optarg); /* see also where we display the head */
		break;
		case 'p':                       /* probability */
			if(!optarg) usage();
			start_chance = atoi(optarg);
		break;
		case 's':
			if(!optarg) usage();
			max_speed = atoi(optarg);
		break;
		case 't':
			if(!optarg) usage();
			usleep_time = atoi(optarg);
		break;
		case 'd':
			allow_keyb = !allow_keyb;
		break;
		case 'v':
			printf("m-code version 1.02\n");
			exit(0);
		case 'x':
			if(!optarg) usage();
			x_color = *optarg;
		break;
		case 'h':
		case '?':
		default:
			usage();
		}
	}
	if(optind != argc) usage();

	/* overflow and stupidity checking...gotta be secure you know ;) */
	/* aka "user dicipline" */
	if(rows < 1 || rows > MAX_WORMS) rows = 25;
	if(cols < 1 || cols > MAX_WORMS) cols = 80;
	if(max_len < 1) max_len = 20;
	if(start_chance < 1) start_chance = 80;
	if(max_speed < 1) max_speed = 3;
	if(usleep_time < 1) usleep_time = 30000;
	if(x_color < '0' || x_color > '8') x_color = '2';

	srandom(time(0));
	fcntl_flags = fcntl(STDIN_FILENO, F_GETFL);
	fcntl(STDIN_FILENO, F_SETFL, fcntl_flags|O_NONBLOCK);

	if(allow_keyb) {
		if(tcgetattr(STDIN_FILENO, &orig_attr)) return 1;
		if(tcgetattr(STDIN_FILENO, &term_attr)) return 1;
		term_attr.c_lflag &= ~(ECHO);
		term_attr.c_lflag &= ~(ICANON);
		if(tcsetattr(STDIN_FILENO, TCSANOW, &term_attr)) return 1;
	}

	printf("\e[0m\e[2J");
	for(loop = 1; loop <= MAX_WORMS; loop++) {
		heads[loop] = 0;
	}

	while(1) {
		usleep(usleep_time);
		if(allow_keyb && read(STDIN_FILENO, &key, 1) != 1) key=0;
		if(helpscreen && key) helpscreen = key = 0;
		if(key >= '0' && key <= '8') { x_color = key; }
		if(key == 'h') {
			if(stupid&4) stupid &= ~(4);
			else stupid |= 4;
		}
		if(key == 'q') kill(getpid(), SIGINT);
		if(key == '?') helpscreen = 1;

		/* create some new worms */
		loop = 0;
		while(++loop <= cols) {
			if(heads[loop] == 0 && !(random()%start_chance)) {
				heads[loop] = 1;
				tails[loop] = 1 - (random()%max_len+1);
				speed[loop] = random()%max_speed;
				ticks[loop] = 0;
				if(x_color == '8') color[loop] = random()%8+'0';
				else color[loop] = x_color;
				num_worms++;
			}
		}

		/* move existing worms */
		loop = 0;
		while(++loop <= cols) {
			if(!heads[loop]) continue;    /* no worm */

			if(ticks[loop] < speed[loop]) {
				/* not yet time to move this worm */
				ticks[loop]++;
				continue;
			}
			ticks[loop] = 0;

			/* reprint the old head in non-bold */
			if(heads[loop] > tails[loop]+1) put(heads[loop] - 1, loop, color[loop], 0);

			/* reprint the head in bold */
			put(heads[loop], loop, color[loop], 1);
			heads[loop]++;

			/* clear the tail */
			put(tails[loop], loop, 0, 0);
			tails[loop]++;
			if(tails[loop] > rows+5) { heads[loop] = 0; num_worms--; }
			fflush(stdout);
		}
	}
}
