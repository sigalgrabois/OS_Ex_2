// Sigal graboys 319009304

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

// defines for return values.
#define ERROR       -1
#define SUCCESS     0
#define IDENTICAL   1
#define DIFFERENT   2
#define SIMILAR     3
#define FAIL     4   // Fail return 4 if program failed.
#define NOT_FOUND   5
#define TIMED_OUT   124 // Timeout return 124 if program timed-out.

// Define max path length.
#define PATH_MAX    4096


// print() function declaration (defined below).
// return 0 for success, -1 for error.
int print(const char *message) {
    if (write(1, message, strlen(message)) == ERROR) {
        return ERROR;
    }
    return SUCCESS;
}


int redirectIO(const char *file, int new_fd) {
    // output redirection.

    if (new_fd == 1 || new_fd == 2) {

        int dst_fd = open(file, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (dst_fd == ERROR) {
            print("Error in: open\n");
            return ERROR;
        }
        if (dup2(dst_fd, new_fd) == ERROR) {
            close(dst_fd);
            print("Error in: dup2\n");
            return ERROR;
        }
        if (close(dst_fd) == ERROR) {
            print("Error in: close\n");
            return ERROR;
        }
        return SUCCESS;
    }

    // Input redirection.
    if (new_fd == 0) {
        int in_fd = open(file, O_RDONLY);
        if (in_fd == ERROR) {
            print("Error in: open\n");
            return ERROR;
        }
        if (dup2(in_fd, new_fd) == ERROR) {
            print("Error in: dup2\n");
            close(in_fd);
            return ERROR;
        }
        if (close(in_fd) == ERROR) {
            print("Error in: close\n");
            return ERROR;
        }
        return SUCCESS;
    }

    // Illegal new_fd.
    return ERROR;
}


int writeResults(const char *student_name, const char *grade, const char *details) {
    // Open the file.
    int csv_file_descriptor = open("./results.csv", O_WRONLY | O_APPEND | O_CREAT,
                                   S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (csv_file_descriptor == ERROR) {
        print("Error in: open\n");
        return ERROR;
    }

    // Create the line to write.
    char line[strlen(student_name) + strlen(grade) + strlen(details) + 4];
    // copy the strings to the line.
    strcpy(line, student_name);
    strcat(line, ",");
    strcat(line, grade);
    strcat(line, ",");
    strcat(line, details);
    strcat(line, "\n");

    // check if the line is valid. If not, return error.
    if (write(csv_file_descriptor, line, strlen(line)) == ERROR) {
        return ERROR;
    }

    // Close the file and handle "./errors.txt".
    if (close(csv_file_descriptor) != SUCCESS) {
        print("Error in: close\n");
        return ERROR;
    }
    return SUCCESS;
}

int deleteFile(const char *fileName) {
    // Check if the file exists.
    if (access(fileName, F_OK) == SUCCESS) {

        // If it exists, delete it (return -1 if something wrong happened).
        if (remove(fileName) != SUCCESS) {
            print("Error in: remove\n");
            return ERROR;
        }
    }
    return SUCCESS;
}


int runCommand(char **args, const char *inputPath) {
    pid_t pid = fork();

    if (pid < 0) {
        // fork() failed.
        print("Error in: fork\n");
        return ERROR;
        // Child process.
    } else if (pid == 0) {
        // use redirection to redirect input to input.txt file - if exists.
        if (inputPath != NULL && redirectIO(inputPath, 0)) {
            return ERROR;
        }
        // use redirection to redirect "./errors.txt" to "./errors.txt".txt file and "./output.txt" to "./results.csv" file.
        if (redirectIO("./errors.txt", 2) == ERROR || redirectIO("./output.txt", 1) == ERROR) {
            return ERROR;
        }
        // execute the command using execvp.
        if (execvp(args[0], args) != SUCCESS) {
            print("Error in: execvp\n");
            return ERROR;
        }
    } else {
        // Wait for the child process to finish.
        int status;
        if (waitpid(pid, &status, WUNTRACED) < 0) {
            print("Error in: waitpid\n");
            return ERROR;
        }

        // If the child process timed out, return 124 for failure.
        if (inputPath != NULL && WIFEXITED(status) && WEXITSTATUS(status) == TIMED_OUT) {
            return TIMED_OUT;
        }

        // If the child process finished on time, check exit status and return it.
        if (!strcmp(args[0], "./comp.out") && WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
    }

    return SUCCESS;
}

int check_extension(const char *filename) {
    int limit = strlen(filename);
    for (int i = 0; i < limit; ++i) {
        if (filename[i] == '.') {
            if (filename[i + 1] == 'C' || filename[i + 1] == 'c') {
                if (filename[i + 2] == '\0') {
                    return SUCCESS; // success
                }
            }
        }
    }
    return FAIL;
}


int CompileCFile(const char *filename, const char *directoryPath) {
    // Open current directory.
    DIR *dirptr;
    if (!(dirptr = opendir(directoryPath))) {
        print("Error in: opendir\n");
        return ERROR;
    }
    struct dirent *dirEntry;
    // init file status  as not found.
    int iscFileFound = NOT_FOUND;

    // look for a c file in the directory
    while ((dirEntry = readdir(dirptr)) != NULL) {
        // Check file's extension.
        if (check_extension(dirEntry->d_name) == SUCCESS) {

            // Creates a path to the C file.
            char path[PATH_MAX];
            strcpy(path, directoryPath);
            strcat(path, "/");
            strcat(path, dirEntry->d_name);
            // Create the command to compile the C file.
            char *args[5] = {"gcc", "-o", "./b.out", path, NULL};

            // check if the file is a c file or a directory
            struct stat path_stat;
            stat(path, &path_stat);
            if (S_ISDIR(path_stat.st_mode)) {
                continue;
            }

            // compile the C file using execute() function (that uses fork() and execvp()).
            iscFileFound = runCommand(args, NULL);

            // If execution failed, write the "./results.csv" file and return FAIL.
            if (iscFileFound != SUCCESS) {
                if (closedir(dirptr)) {
                    print("Error in: closedir\n");
                    return ERROR;
                }
                if (writeResults(filename, "10", "COMPILATION_ERROR") == ERROR) {
                    return ERROR;
                }
                return FAIL;
            }

            // Else, execution was success, so verify the existance of the binary file.
            if (access("./b.out", F_OK) == SUCCESS) {
                if (closedir(dirptr)) {
                    print("Error in: closedir\n");
                    return ERROR;
                }
                return SUCCESS;
            }

        }
    }

    // Close directory (return -1 if failed).
    if (closedir(dirptr) == ERROR) {
        print("Error in: closedir\n");
        return ERROR;
    }

    // Handle case C file not found.
    if (iscFileFound == NOT_FOUND) {
        if (writeResults(filename, "0", "NO_C_FILE") == ERROR) {
            return ERROR;
        }
        return FAIL;
    }

    // Handle case binary file not found.
    if (writeResults(filename, "10", "COMPILATION_ERROR") == ERROR) {
        return ERROR;
    }

    // Return 4 for failure for any other unexpected case.
    return FAIL;

}

int runProgram(const char *name, const char *inputFile) {
    // command init to run the program, using timeout to limit the run time to 5 seconds.
    char *command[] = {"timeout", "5s", "./b.out", NULL};
    // run the command with the input file, returns 0 for success, -1 for error or 124 for time-out.
    int status = runCommand(command, inputFile);

    // Write to the CSV if operation was timed-out.
    if (status == TIMED_OUT) {
        if (writeResults(name, "20", "TIMEOUT") == ERROR) {
            return ERROR;
        }
    }

    // Return 0 for success, -1 for error and 4 for failure.
    return status;

}

/**********************************************************************************
* Function:     test"./output.txt"
* Input:        Current file's name and sharedAccess.
* "./output.txt":       The return value of ex31.c (1, 2 or 3), or -1 for error.
* Operation:    This function tests the "./output.txt" using ex31.c program and write
*               write the result to "./results.csv".csv file. The function return ex31.c
*               return value or -1 if an error occured.
***********************************************************************************/
int testOutput(const char *name, const char *correctOutputFile) {

    // Make a copy to avoid 'const' warning message.
    char correctFileCopy[strlen(correctOutputFile)];
    strcpy(correctFileCopy, correctOutputFile);

    char *command[] = {"./comp.out", "./output.txt", correctFileCopy, NULL};

    // run command
    int status = runCommand(command, NULL);

    // Write result to "./results.csv" and return status code.
    // Return -1 for error in case of an unexpected result.
    switch (status) {
        case IDENTICAL:
            return writeResults(name, "100", "EXCELLENT");
        case DIFFERENT:
            return writeResults(name, "50", "WRONG");
        case SIMILAR:
            return writeResults(name, "75", "SIMILAR");
        default:
            return ERROR;
    }

}

/**********************************************************************************
* Function:     runTest
* Input:        Target directory, input file location, and "./output.txt" file location.
* "./output.txt":       0 for success, -1 for failure.
* Operation:    This is the main test function. It verifies existance of necessary
*               files, open the target directory, and for each directory entry it
*               finds, it does 3 things:
*                   1) Look for a C file and "./comp.out"ile it (with findAnd"./comp.out"ile()).
*                   2) Try to run the binary for 5 seconds (with runProgram()).
*                   3) "./comp.out"are the "./output.txt" with the correct onr (with test"./output.txt"()).
*               Each and every operation is checked, and the function returns -1
*               if any significant error occured.
***********************************************************************************/
int runTest(const char *target, const char *inputFile, const char *correctOutputFile) {

    // Try to open the target directory.
    DIR *dir = opendir(target);
    if (!dir) {
        print("Not a valid directory\n");
        return ERROR;
    }

    // Initialize status flag variable.
    int status;

    // Traverse sub-directories and do the job.
    struct dirent *dirEnt;
    while ((dirEnt = readdir(dir)) != NULL) {

        // Save directory name in a variable for convenience reason.
        const char *name = dirEnt->d_name;

        // Avoid current and previous directories references.
        if (!strcmp(name, ".") || !strcmp(name, "..")) {
            continue;
        }

        // Create a path to the next directory.
        char path[PATH_MAX];
        strcpy(path, target);
        strcat(path, "/");
        strcat(path, dirEnt->d_name);

        // Create stat to identify directory entry type.
        struct stat pathStat;
        if (stat(path, &pathStat) == ERROR) {
            return ERROR;
        }

        // Engage only directories.
        if (!S_ISDIR(pathStat.st_mode)) {
            continue;
        }

        // Look for a C file in the sub-directory and compile it. Return Error (-1) if needed.
        status = CompileCFile(name, path);
        if (status == ERROR) {
            closedir(dir);
            return ERROR;
        }

        // If a C file was found and successfully compiled, try to run its binary with the given input file.
        if (status == SUCCESS) {
            status = runProgram(name, inputFile);
            if (status == ERROR) {
                closedir(dir);
                return ERROR;
            }
            // If the program ran succesfully, compare its "./output.txt" to the given correct "./output.txt" file.
            if (status == SUCCESS) {
                status = testOutput(name, correctOutputFile);
                if (status == ERROR) {
                    closedir(dir);
                    return ERROR;
                }
            }
        }

        // Cleanup - remove redundent files.
        if (deleteFile("./b.out") == ERROR || deleteFile("./output.txt") == ERROR) {
            return ERROR;
        }

    }

    // Close target directory.
    if (closedir(dir) == ERROR) {
        return ERROR;
    }

    // Return 0, which means success.
    return SUCCESS;

}

/**********************************************************************************
* Function:     setupChecks
* Input:        Target directory, input file location, and "./output.txt" file location.
* "./output.txt":       0 for success, -1 for failure.
* Operation:    This function remove old "./errors.txt".txt and "./results.csv".csv files and
*               verifies that the given target directory, input file, and correct
*               "./output.txt" file are exists and from the correct types.
***********************************************************************************/
int setupChecks(const char *directory, const char *input, const char *correct) {

    // Remove old files if exists.
    if (deleteFile("./errors.txt") == ERROR || deleteFile("./results.csv") == ERROR) {
        return ERROR;
    }

    // Create stat to identify entry type.
    struct stat entry;

    // Verify target dictionary existance and type.
    if (access(directory, F_OK) != SUCCESS || stat(directory, &entry) == ERROR || !S_ISDIR(entry.st_mode)) {
        print("Not a valid directory\n");
        return ERROR;
    }

    // Verify input file existance and type.
    if (access(input, F_OK) != SUCCESS || stat(input, &entry) == ERROR || !S_ISREG(entry.st_mode)) {
        print("Input file not exist\n");
        return ERROR;
    }

    // Verify "./output.txt" file existance.
    if (access(correct, F_OK) != SUCCESS || stat(correct, &entry) == ERROR || !S_ISREG(entry.st_mode)) {
        print("Output file not exist\n");
        return ERROR;
    }

    // Return 0 if all checked.
    return SUCCESS;

}

/*
 * this is the main function of the program.
 * it gets the configuration file as an argument and runs the test.
*/
int main(int argc, char **argv) {

    // Open configuration file.
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        print("Error in: open\n");
        exit(ERROR);
    }

    // Set buffer and read configuration file.
    char buffer[1024];
    if (read(fd, buffer, 1024) == ERROR) {
        print("Error in: read\n");
        close(fd);
        exit(ERROR);
    }

    // Close configuration file descriptor.
    if (close(fd) == ERROR) {
        print("Error in: close\n");
        exit(ERROR);
    }


    // Extract information from the configuration file.
    char *targetDirectory = strtok(buffer, "\n");
    char *inputFile = strtok(NULL, "\n");
    char *correctFile = strtok(NULL, "\n");

    if (setupChecks(targetDirectory, inputFile, correctFile) == ERROR) {
        return ERROR;
    }

    // Run test -- find C files, compile each of them, run and test "./output.txt"s.
    int status = runTest(targetDirectory, inputFile, correctFile);

    // If unexpected error will occur, runTest() will return -1, and the main will exit with code -1.
    if (status != SUCCESS) {
        exit(ERROR);
    }

    // Done.
    return SUCCESS;

}