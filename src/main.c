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
        fprintf(stderr, "lsh: allocation error\n");
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
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (execvp(argv[0], argv) == -1) {
            perror(argv[0]);
            write(STDOUT_FILENO, "\r", 1);
            exit(1);
        }
        exit(0);
    } else if (pid < 0) {
        perror("koopsh");
    } else {
        waitpid(pid, &status, WUNTRACED);
        while (!WIFEXITED(status) && !WIFSIGNALED(status)) {
            waitpid(pid, &status, WUNTRACED);
        }
    }

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
    } else if (!strcmp(argv[0], "ls")) {

    } else if (!strcmp(argv[0], "exit")) {
        disable_raw_mode();
        exit(0);
    } else {
        launch(argv);
    }
}

int main(int argc, char **argv) {
    clear();
    enable_raw_mode();
    char cwd[PATH_MAX];
    getcwd(cwd, PATH_MAX);
    write(STDOUT_FILENO, "[koopsh]:", 9);
    printf("%s", cwd);
    fflush(stdout);
    write(STDOUT_FILENO, "$ ", 2);
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
            printf("^C\r\n[koopsh]:%s$ ", cwd);
            fflush(stdout);
        } else if (key == BACKSPACE) {
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
            printf("[koopsh]:%s$ ", cwd);
            fflush(stdout);
        } else if (key != '\0') {
            write(STDOUT_FILENO, &key, 1);
            line[position++] = key;
        }
    }
    disable_raw_mode();
    return 0;
}
