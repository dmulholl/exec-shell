// C standard library.
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// POSIX.
#include <unistd.h>
#include <sys/wait.h>


// Writes the null-terminated string [input] to the file descriptor [fd].
bool write_to_fd(int fd, const char* input) {
    size_t index = 0;
    size_t length = strlen(input);

    while (index < length) {
        size_t num_bytes_to_write = length - index;
        ssize_t count = write(fd, &input[index], num_bytes_to_write);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return false;
            }
        }
        index += count;
    }

    return true;
}


// Reads from the file descriptor [fd].
bool read_from_fd(int fd) {
    uint8_t buf[1024];

    while (true) {
        ssize_t count = read(fd, buf, 1024);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                return false;
            }
        } else if (count == 0) {
            break;
        } else {
            printf("%.*s", (int)count, buf);
        }
    }

    return true;
}


// Executes the null-terminated string [cmd] as a shell command.
// Writes the null-terminated string [input] to the command's stdin.
void exec_shell_cmd(const char* cmd, const char* input) {
    int child_stdin_pipe[2];
    if (pipe(child_stdin_pipe) == -1) {
        perror("failed to create child's stdin pipe");
        exit(1);
    }

    int child_stdout_pipe[2];
    if (pipe(child_stdout_pipe) == -1) {
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        perror("failed to create child's stdout pipe");
        exit(1);
    }

    int child_stderr_pipe[2];
    if (pipe(child_stderr_pipe) == -1) {
        close(child_stdin_pipe[0]);
        close(child_stdin_pipe[1]);
        close(child_stdout_pipe[0]);
        close(child_stdout_pipe[1]);
        perror("failed to create child's stderr pipe");
        exit(1);
    }

    pid_t child_pid = fork();

    // If child_pid == 0, we're in the child.
    if (child_pid == 0) {
        close(child_stdin_pipe[1]); // close the write end of the input pipe
        while ((dup2(child_stdin_pipe[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stdin_pipe[0]);

        close(child_stdout_pipe[0]); // close the read end of the output pipe
        while ((dup2(child_stdout_pipe[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stdout_pipe[1]);

        close(child_stderr_pipe[0]); // close the read end of the error pipe
        while ((dup2(child_stderr_pipe[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
        close(child_stderr_pipe[1]);

        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        perror("returned from execl() in child");
        exit(1);
    }

    // If child_pid > 0, we're in the parent.
    else if (child_pid > 0) {
        close(child_stdin_pipe[0]);  // close the read end of the input pipe
        close(child_stdout_pipe[1]); // close the write end of the output pipe
        close(child_stderr_pipe[1]); // close the write end of the error pipe

        // If [input] is not NULL, write it to the child's stdin pipe.
        if (input) {
            if (!write_to_fd(child_stdin_pipe[1], input)) {
                close(child_stdin_pipe[1]);
                close(child_stdout_pipe[0]);
                close(child_stderr_pipe[0]);
                perror("error while writing to child's stdin pipe");
                exit(1);
            }
        }
        close(child_stdin_pipe[1]);

        // Read from the child's stdout pipe.
        printf("[CHILD STDOUT]: \n");
        if (!read_from_fd(child_stdout_pipe[0])) {
            close(child_stdout_pipe[0]);
            close(child_stderr_pipe[0]);
            perror("error while reading from child's stdout pipe");
            exit(1);
        }
        close(child_stdout_pipe[0]);

        // Read from the child's stderr pipe.
        printf("[CHILD STDERR]: \n");
        if (!read_from_fd(child_stderr_pipe[0])) {
            close(child_stderr_pipe[0]);
            perror("error while reading from child's stderr pipe");
            exit(1);
        }
        close(child_stderr_pipe[0]);

        // Wait for the child to exit.
        int status;
        do {
            waitpid(child_pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        printf("[CHILD EXIT CODE]: %d\n", status);
    }

    // If child_pid < 0, the attempt to fork() failed.
    else {
        perror("fork() failed");
        exit(1);
    }
}


int main(int argc, char** argv) {
    printf("---\n");
    exec_shell_cmd("ls", NULL);

    printf("---\n");
    exec_shell_cmd("cat", "foo bar\n");

    printf("---\n");
    exec_shell_cmd("echo $HOME", NULL);

    printf("---\n");
    return 0;
}
