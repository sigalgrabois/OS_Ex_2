#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>

#define IDENTICAL 1
#define SIMILAR   3
#define DIFFERENT 2

void similar_reformat(char *input_file, char *output_file) {
    pid_t child_pid;
    int child_status;
    int input_fd, output_fd;

    input_fd = open(input_file, O_RDWR);
    output_fd = open("./tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if ((child_pid = fork()) == 0) {
        dup2(input_fd, STDIN_FILENO);
        dup2(output_fd, STDOUT_FILENO);
        close(input_fd);
        close(output_fd);
        execlp("tr", "tr", "-d", "[:space:]", NULL);
        exit(-1);
    } else {
        waitpid(child_pid, &child_status, 0);
        close(input_fd);
        close(output_fd);

        input_fd = open("./tmp", O_RDWR);
        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if ((child_pid = fork()) == 0) {
            dup2(input_fd, STDIN_FILENO);
            dup2(output_fd, STDOUT_FILENO);
            close(input_fd);
            close(output_fd);
            execlp("tr", "tr", "[:upper:]", "[:lower:]", NULL);
            exit(-1);
        } else {
            waitpid(child_pid, &child_status, 0);
            close(input_fd);
            close(output_fd);
            remove("./tmp");
        }
    }
}

int compareFiles(const char *file1, const char *file2) {
    int fd1 = open(file1, O_RDONLY);
    int fd2 = open(file2, O_RDONLY);

    char buffer1, buffer2;
    ssize_t bytes_read1, bytes_read2;

    do {
        bytes_read1 = read(fd1, &buffer1, 1);
        bytes_read2 = read(fd2, &buffer2, 1);

        if (bytes_read1 < 0 || bytes_read2 < 0) {
            exit(-1);
        }

        if (buffer1 != buffer2) {
            close(fd1);
            close(fd2);
            return 1;
        }
    } while (bytes_read1 > 0 && bytes_read2 > 0);

    close(fd1);
    close(fd2);
    return 0;
}

int main(int argc, char **argv) {
    // check the number of arguments - expected 2
    if (argc < 3) {
        exit(-1);
    }

    // check if files are identical
    int flag = compareFiles(argv[1], argv[2]);
    if (flag == 0) {
        return IDENTICAL;
    }

    // check if files are similar - meaning identical after removing spaces and capital letters
    similar_reformat(argv[1], "./result1");
    similar_reformat(argv[2], "./result2");
    flag = compareFiles("./result1", "./result2");

    if (flag == 0) {
        remove("./result1");
        remove("./result2");
        return SIMILAR;
    }
    remove("./result1");
    remove("./result2");

    return DIFFERENT;
}
