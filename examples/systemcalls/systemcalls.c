#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int retval = system(cmd);

    if(retval == 0) {
        return true;
    }

    return false;
}

bool is_command_successful(const int status) {
    // If the process exited normally
    if (WIFEXITED(status)) {
        const int exit_code = WEXITSTATUS((status));

        // If the process exited successfully
        if (exit_code == 0) {
            return true;
        }
        return false;
    }

    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    va_end(args);

    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return false;
    } 
    
    // The child
    if (pid == 0) {
        execv(command[0], command);

        // If execv fails, return an error
        return false;
    }

    // The parent
    int status;
    // If function fails
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return false;
    }

    return is_command_successful(status);
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    va_end(args);
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    


    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return false;
    } 
    
    // The child
    if (pid == 0) {
        const int fd = creat(outputfile, 0644);
        if (fd == -1) {
            perror("creat");
            return false;
        }

        // redirect standard out
        if (dup2(fd, 1) == -1) {
            perror("dup2");
            return false;
        }
        close(fd);

        execv(command[0], command);

        // If execv fails, return an error
        return false;
    }

    // The parent
    int status;
    // If function fails
    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        return false;
    }

    return is_command_successful(status);
}
