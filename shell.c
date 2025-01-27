#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

#define maxInputSize 1024
#define maxArgs 256
#define maxPath 4096
#define maxCmds 8192 

char currDir[maxPath];

void getCurrDir() 
{
    pid_t pid = fork();
    
    if(pid == 0) 
    {  
        FILE *temp = fopen("/tmp/pwd_output", "w");
        if(temp) 
        {
            dup2(fileno(temp), STDOUT_FILENO);
            fclose(temp);
            char *args[] = {"/bin/pwd", NULL};
            execv("/bin/pwd", args);
        }

        exit(1);
    } 
    else if (pid > 0) 
    {  
        int status;
        waitpid(pid, &status, 0);
      
        FILE *temp = fopen("/tmp/pwd_output", "r");
        if (temp) 
        {
            if (fgets(currDir, sizeof(currDir), temp)) 
            {
                currDir[strcspn(currDir, "\n")] = 0;
            }
            fclose(temp);
            remove("/tmp/pwd_output");
        }
    }
}

void parseInput(char **args, int *argCount, char *input) 
{
    char *token = strtok(input, " \t");
    *argCount = 0;
    
    while((*argCount < maxArgs - 1)&& token != NULL) 
    {
        args[*argCount] = token;
        (*argCount)++;
        token = strtok(NULL, " \t");
    }

    args[*argCount] = NULL;
}

void runHelp() 
{
    printf("MyShell Help:\n");
    printf("Built-in commands:\n");
    printf("  cd <directory> - Change current directory\n");
    printf("  help           - Display this help message\n");
    printf("  exit           - Exit the shell\n");
    printf("  ls             - Lists files and directories\n");
    printf("I/O Redirection:\n");
    printf("  command < inputFile  - Input redirection\n");
    printf("  command > outputFile - Output redirection\n");
    printf("Pipe Support:\n");
    printf("  command1 | command2   - Pipe output of command1 to command2\n");
    printf("Background Execution:\n");
    printf("  command &              - Run command in background\n");
}

int setupPipe(char **args, int argCount) 
{
    int pipeIndex = -1;
    char *firstCmd[maxArgs] = {NULL};
    char *secondCmd[maxArgs] = {NULL};
    char *redirOutput = NULL;
    int FirstCount = 0;
    int SecondCount = 0;

    for(int i = 0; i < argCount; i++) 
    {
        if(strcmp(args[i], "|") == 0) 
        {
            pipeIndex = i;
            break;
        }
    }
    
    if(pipeIndex == -1)
    {
        return 0;
    }

    for(int i = 0; i < pipeIndex; i++) 
    {
        firstCmd[FirstCount++] = args[i];
    }

    firstCmd[FirstCount] = NULL;

    int secondStart = pipeIndex + 1;
    for(int i = secondStart; i < argCount; i++) 
    {
        if (strcmp(args[i], ">") == 0 && i + 1 < argCount) {
            redirOutput = args[i + 1];
            break;
        }
        secondCmd[SecondCount++] = args[i];
    }
    secondCmd[SecondCount] = NULL;

    int pipefd[2];
    if(pipe(pipefd) == -1) 
    {
        perror("Pipe creation failed");
        return -1;
    }

    pid_t pid1 = fork();
    if(pid1 == 0) 
    { 
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(firstCmd[0], firstCmd);
        perror("First command execution failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if(pid2 == 0) 
    { 
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);

        if(redirOutput) 
        {
            int fd = open(redirOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd == -1) 
            {
                perror("Output redirection failed");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(secondCmd[0], secondCmd);
        perror("Second command execution failed");
        exit(1);
    }
    
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 1;
}

void killZombieProcesses() 
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int runBackgroundCommand(char **args, int argCount, char *inputFile, char *outputFile) 
{
    pid_t pid = fork();

    if(pid < 0) 
    {
        perror("Fork failed");
        return 0;
    } 
    else if(pid == 0) 
    {
        if(inputFile) 
        {
            int fd = open(inputFile, O_RDONLY);
            if (fd == -1) {
                perror("Input redirection failed");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        if(outputFile) 
        {
            int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd == -1) 
            {
                perror("Output redirection failed");
                exit(1);
            }

            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        char cmd[maxCmds];
        snprintf(cmd, sizeof(cmd), "cd \"%s\" && %s", currDir, args[0]);
        
        for(int i = 1; i < argCount; i++) 
        {
            strcat(cmd, " ");
            strcat(cmd, args[i]);
        }

        char *sh_args[] = {"/bin/sh", "-c", cmd, NULL};
        execv("/bin/sh", sh_args);
        perror("Command execution failed");
        exit(1);
    } 
    else 
    {
        printf("Started background process with PID %d\n", pid);
    }

    return 0;
}

int runCommands(char **args, int argCount) 
{
    char *inputFile = NULL;
    char *outputFile = NULL;
    int newArgCount = 0;
    char *newArgs[maxArgs];
    char cmd[maxCmds];

    int pipeFound = 0;
    for(int i = 0; i < argCount; i++) 
    {
        if(strcmp(args[i], "|") == 0) 
        {
            pipeFound = setupPipe(args, argCount);
            return 0;
        }
    }

    int isBackground = 0;
    if(argCount > 0 && strcmp(args[argCount - 1], "&") == 0) 
    {
        isBackground = 1;
        argCount--; 
    }

    for(int i = 0; i < argCount; i++) 
    {
        if(strcmp(args[i], "<") == 0 && i + 1 < argCount) 
        {
            inputFile = args[++i];
        }
        else if(strcmp(args[i], ">") == 0 && i + 1 < argCount) 
        {
            outputFile = args[++i];
        }
        else 
        {
            newArgs[newArgCount++] = args[i];
        }
    }

    newArgs[newArgCount] = NULL;

    if(strcmp(newArgs[0], "exit") == 0) 
    {
        printf("Exiting MyShell...\n");
        return 1;
    }
    else if(strcmp(newArgs[0], "help") == 0) 
    {
        runHelp();
        return 0;
    }
    else if(strcmp(newArgs[0], "cd") == 0) 
    {
        if(newArgs[1] == NULL) 
        {
            printf("cd: missing directory argument\n");
            return 0;
        }
        
        pid_t pid = fork();
        
        if(pid == 0) 
        {
            char *cd_args[] = {"/bin/sh", "-c", NULL, NULL};
            char cmd[maxCmds];
            
            if(strcmp(newArgs[1], ".") == 0) 
            {
              snprintf(cmd, sizeof(cmd), "cd \"%s\" && pwd >/tmp/cd_output", currDir);
            } 
            else if(strcmp(newArgs[1], "..") == 0) 
            {
              snprintf(cmd, sizeof(cmd), "cd \"%s\" && cd .. && pwd >/tmp/cd_output", currDir);
            }
            else
            {
              snprintf(cmd, sizeof(cmd), "cd \"%s\" && cd \"%s\" && pwd >/tmp/cd_output", currDir, newArgs[1]);
            }
            
            cd_args[2] = cmd;
            execv("/bin/sh", cd_args);
            exit(1);
        } 
        else if(pid > 0) 
        {
            int status;
            waitpid(pid, &status, 0);
            
            if(WIFEXITED(status) && WEXITSTATUS(status) == 0) 
            {
                FILE *temp = fopen("/tmp/cd_output", "r");
                if(temp) 
                {
                    if(fgets(currDir, sizeof(currDir), temp)) 
                    {
                        currDir[strcspn(currDir, "\n")] = 0;
                    }

                    fclose(temp);
                    remove("/tmp/cd_output");
                }
            } 
            else 
            {
                printf("cd: No such file or directory\n");
            }
        }

        return 0;
    }
    
    if(isBackground) 
    {
        return runBackgroundCommand(newArgs, newArgCount, inputFile, outputFile);
    } 
    else 
    {
        pid_t pid = fork();

        if(pid < 0) 
        {
            perror("Fork failed");
            return 0;
        }
        else if(pid == 0) 
        {
            if(inputFile) 
            {
                int fd = open(inputFile, O_RDONLY);
                if (fd == -1) {
                    perror("Input redirection failed");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            if(outputFile) 
            {
                int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if(fd == -1) 
                {
                    perror("Output redirection failed");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            char *sh_args[] = {"/bin/sh", "-c", NULL, NULL};
            int ret = snprintf(cmd, sizeof(cmd), "cd \"%s\" && %s", currDir, newArgs[0]);
            if(ret < 0 || ret >= sizeof(cmd)) 
            {
                printf("Command too long\n");
                exit(1);
            }

            for(int i = 1; newArgs[i] != NULL; i++) 
            {
                char temp[maxInputSize];
                ret = snprintf(temp, sizeof(temp), " \"%s\"", newArgs[i]);
                if (ret < 0 || ret >= sizeof(temp) || strlen(cmd) + strlen(temp) >= sizeof(cmd)) {
                    printf("Command too long\n");
                    exit(1);
                }
                strcat(cmd, temp);
            }

            sh_args[2] = cmd;
            execv("/bin/sh", sh_args);
            perror("Command execution failed");
            exit(1);
        } 
        else 
        {
            int status;
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}

int main() 
{
    char input[maxInputSize];
    char *args[maxArgs];
    int argCount;

    printf("MyShell Started....Type 'help' for commands.\n");

    getCurrDir();

    while(1) 
    {    
        killZombieProcesses();

        printf("MyShell%s$ ", currDir);
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) break;

        input[strcspn(input, "\n")] = 0;
        parseInput(args, &argCount, input);

        if (argCount > 0) {
            int isExit = runCommands(args, argCount);
            if(isExit)
            {
                break;
            }
        }
    }

    return 0;
}