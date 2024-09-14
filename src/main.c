#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>

struct termios original;

#define LINE_SIZE 1024
#define ENTER_KEY 13
#define TAB 9
#define BACKSPACE 127
#define TOK_DELIM " \t\r\n\a"

// TODO:
// we have a heisenbug where running exit sometimes won't exit

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &original);
    struct termios raw = original;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

void clear() {
    write(STDOUT_FILENO, "\x1b[2;3J\x1b[H", 9);
}

char **split_line(char *line) {
    char **tokens = malloc(LINE_SIZE * sizeof(char*));
    char *token;
    int position = 0;

    if (!tokens) {
        // TODO: do my own stuff here
        fprintf(stderr, "koopsh: allocation error\r\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        fflush(stdout);
        position++;

        token = strtok(NULL, TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int launch(char **argv) {
    disable_raw_mode();
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        if (execvp(argv[0], argv) == -1) {
            perror(argv[0]);
            putchar('\r');
            exit(1);
        }
        exit(0);
    } else if (pid < 0) {
        perror("koopsh");
    } else {
        waitpid(pid, &status, WUNTRACED);
        while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status)) {
            waitpid(pid, &status, WUNTRACED);
        }
        if (WIFSTOPPED(status)) {
            printf("Stopped process %d [%s]\n", pid, argv[0]);
        }
    }
    enable_raw_mode();

    return 1;
}

void execute(char **argv) {
    // TODO: actually make good
    if (!strcmp(argv[0], "clear")) {
        clear();
    } else if (!strcmp(argv[0], "cd")) {
        if (argv[1] == NULL) {
            printf("cd: usage: cd [dir]\r\n");
            fflush(stdout);
        } else if (chdir(argv[1]) == -1) {
            perror("cd");
            printf("\r");
            fflush(stdout);
        }
    } else if (!strcmp(argv[0], "exit")) {
        disable_raw_mode();
        exit(0);
    } else if (!strcmp(argv[0], "vim")) {
        // TODO: put in real aliases. Maybe make a hash map from scratch :P
        argv[0] = "nvim";
        launch(argv);
    } else {
        launch(argv);
    }
}

int main(int argc, char **argv) {
    clear();

    signal(SIGTSTP, SIG_IGN);
    signal(SIGINT, SIG_IGN);

    enable_raw_mode();

    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    printf("\x1b[1;32m[koopsh]:\x1b[33m%s$\x1b[0m ", cwd);
    fflush(stdout);

    // TODO: dynamically allocate and error handling
    char line[LINE_SIZE];
    char **tokens;
    int position = 0;
    while (1) {
        char key = '\0';
        // TODO: fix literally everythings. I found the bug though. It was making the first character
        // of line a null byte every time because it would go all the way to the else branch
        read(STDIN_FILENO, &key, 1);
        if (key == CTRL('c')) {
            position = 0;
            getcwd(cwd, PATH_MAX);
            printf("^C\r\n\x1b[1;32m[koopsh]:\x1b[33m%s$\x1b[0m ", cwd);
            fflush(stdout);
        } else if (key == CTRL('z')) {
        } else if (key == BACKSPACE) {
            if (position == 0) {
                printf("\a");
                continue;
            }
            printf("\b \b");
            line[position--] = '\0';
            fflush(stdout);
        } else if (key == TAB) {

        } else if (key == ENTER_KEY) {
            write(STDOUT_FILENO, "\r\n", 2);
            line[position] = '\0';
            position = 0;
            tokens = split_line(line);
            execute(tokens);
            getcwd(cwd, PATH_MAX);
            printf("\x1b[1;32m[koopsh]:\x1b[33m%s$\x1b[0m ", cwd);
            fflush(stdout);
        } else if (key != '\0') {
            write(STDOUT_FILENO, &key, 1);
            line[position++] = key;
        }
    }
    disable_raw_mode();
    return 0;
}
