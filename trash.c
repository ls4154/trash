#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define CMDLINE_MAX 32768
char cmdline[CMDLINE_MAX];

struct cmd {
	int argc;
	char *argv[64];
};
struct cmd cmdv[64];
int cmd_cnt;
char *infile;
char *outfile;

pid_t pidv[64];
int pipev[64][2];

void print_prompt(void)
{
	printf("trash >> ");
	fflush(stdout);
}
void get_cmdline(void)
{
	fgets(cmdline, CMDLINE_MAX, stdin);
}

int terminate(void)
{
	return feof(stdin);
}

void parse_cmdline(void)
{
	char *p = cmdline;
	int quote = 0;
	int finish = 0;

	infile = NULL;
	outfile = NULL;

	for (cmd_cnt = 0; !finish; cmd_cnt++) {
		int argc;
		cmdv[cmd_cnt].argc = 0;
		for (argc = 0; ; argc++) {
			while (*p == ' ')
				p++;
			cmdv[cmd_cnt].argv[argc] = p;

			if (*p == '\n') {
				return;
			} else if (*p == '\'') {
				*p = '\0';
				p++;
				cmdv[cmd_cnt].argv[argc] = p;
				while (*p != '\'')
					p++;
				*p = '\0';
				p++;
			} else if (*p == '"') {
				*p = '\0';
				p++;
				cmdv[cmd_cnt].argv[argc] = p;
				while (*p != '"')
					p++;
				*p = '\0';
				p++;
			} else {
				cmdv[cmd_cnt].argv[argc] = p;
				while (*p != ' ' && *p != '|' && *p != '<' && *p != '>' && *p != '\n')
					p++;
			}

			while (*p == ' ') {
				*p = '\0';
				p++;
			}

			if (*p == '\n') {
				*p = '\0';
				finish = 1;
				break;
			} else if (*p == '|') {
				*p = '\0';
				p++;
				break;
			} else if (*p == '<' || *p == '>') {
				int rin = *p == '<' ? 1 : 0;
				*p = '\0';
				p++;
				while (*p == ' ') {
					*p = '\0';
					p++;
				}
				if (rin)
					infile = p;
				else
					outfile = p;
				while (*p != ' ' && *p != '|' && *p != '<' && *p != '>' && *p != '\n')
					p++;
				while (*p == ' ') {
					*p = '\0';
					p++;
				}

				if (*p == '\n') {
					*p = '\0';
					finish = 1;
					break;
				} else if (*p == '|') {
					*p = '\0';
					p++;
					break;
				} else if (*p == '<' || *p == '>') {
					rin = *p == '<' ? 1 : 0;
					*p = '\0';
					p++;
					while (*p == ' ') {
						*p = '\0';
						p++;
					}
					if (rin)
						infile = p;
					else
						outfile = p;
					while (*p != ' ' && *p != '|' && *p != '\n')
						p++;
					while (*p == ' ') {
						*p = '\0';
						p++;
					}
					if (*p == '\n') {
						*p = '\0';
						finish = 1;
						break;
					} else if (*p == '|') {
						*p = '\0';
						p++;
						break;
					} else {
						assert(0);
					}
				} else {
					assert(0);
				}
			}
		}
		argc++;
		cmdv[cmd_cnt].argv[argc] = NULL;
		cmdv[cmd_cnt].argc = argc;
	}
}

void print_parse_result(void)
{
	printf("%d cmds\n", cmd_cnt);
	for (int i = 0; i < cmd_cnt; i++) {
		printf("cmd %d: ", i);
		for (int j = 0; j < cmdv[i].argc; j++)
			printf("[%s] ", cmdv[i].argv[j]);
		puts("");
	}
	if (infile)
		printf("infile: [%s]\n", infile);
	if (outfile)
		printf("outfile: [%s]\n", outfile);
}

void cmd_exit();
void cmd_cd();

void run_cmdline(void)
{
	if (!strcmp(cmdv[0].argv[0], "exit"))
		cmd_exit();

	if (!strcmp(cmdv[0].argv[0], "cd")) {
		cmd_cd(cmdv[0].argc, cmdv[0].argv);
		return;
	}

	for (int i = 0; i < cmd_cnt - 1; i++)
		pipe(pipev[i]);

	for (int i = 0; i < cmd_cnt; i++) {
		pidv[i] = fork();
		if (pidv[i] == 0) {
			if (i == 0 && infile != NULL) {
				close(STDIN_FILENO);
				if (open(infile, O_RDONLY) < 0) {
					printf("file open error\n");
					return;
				}
			}
			if (i == cmd_cnt - 1 && outfile != NULL) {
				close(STDOUT_FILENO);
				if (open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644) < 0) {
					printf("file open error\n");
					return;
				}
			}
			if (i > 0) {
				close(STDIN_FILENO);
				dup(pipev[i - 1][0]);
			}
			if (i < cmd_cnt - 1) {
				close(STDOUT_FILENO);
				dup(pipev[i - 1][0]);
			}
			for (int j = 0; j < cmd_cnt - 1; j++) {
				close(pipev[j][0]);
				close(pipev[j][1]);
			}
			execvp(cmdv[i].argv[0], cmdv[i].argv);
			assert(0);
		}
	}

	for (int j = 0; j < cmd_cnt - 1; j++) {
		close(pipev[j][0]);
		close(pipev[j][1]);
	}

	int status;
	for (int i = 0; i < cmd_cnt; i++)
		waitpid(pidv[i], &status, 0);
}

void cmd_exit()
{
	exit(0);
}
void cmd_cd(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "cd: missing arguments\n");
		return;
	}
	if (chdir(argv[1]) == -1) {
		perror("chdir");
		return;
	}
}

int main(void)
{
	while (1) {
		print_prompt();
		get_cmdline();
		if (terminate())
			break;
		parse_cmdline();
		// print_parse_result();
		run_cmdline();
	}
	return 0;
}
