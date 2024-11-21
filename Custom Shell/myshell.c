#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#define TRUE 1
#define maxinputwords (512 / 32)
#define STD_IN 0
#define STD_OUT 1

void shell_start()
{
    printf("my_shell$ ");
    // fflush(stdout);
}

char *trim_wht(char *str)
{

    // Trim leading spaces
    while (isspace((unsigned char)*str))
        str++;

    // Trim trailing spaces
    char *eWh = str + strlen(str) - 1;
    while (eWh > str && isspace((unsigned char)*eWh))
    {
        *eWh = '\0';
        eWh--;
    }

    return str;
}

int pipe_parse(char *inputL, char *cmds[]) // parses and returns num of commands too
{
    char *inputL_ = strdup(inputL);

    char *token = strtok(inputL_, "|");
    int cmdin = 0;
    while (token != NULL)
    {
        token = trim_wht(token);
        cmds[cmdin] = token;
        cmdin++;

        token = strtok(NULL, "|");
    }
    cmds[cmdin] = NULL; // null terminate last
    return cmdin;
}

void space_parse(char *inputL, char *cmds[])
{
    char *inputL_ = strdup(inputL);

    int i;
    for (i = 0; cmds[i] != NULL; i++) // clear the array in case of previous use
    {
        cmds[i] = NULL;
    }

    char *token = strtok(inputL_, " ");
    int cmdin = 0;
    while (token != NULL)
    {

        char *s = token;

        cmds[cmdin] = s;
        cmdin++;

        token = strtok(NULL, " ");
    }

    cmds[cmdin] = NULL;
}

void pipe_print(char *cmds[])
{
    int i = 0;
    while (cmds[i] != NULL)
    {
        printf("%s\n", cmds[i]);
        fflush(stdout);
        i++;
    }
}

void killthechildren(int signum)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int amperCHECK(char *input)
{

    if (input[strlen(input) - 1] == '&') // Check if command ends with '&'
    {
        input[strlen(input) - 1] = '\0'; // Remove the '&' from the command
        return 1;
    }
    return 0;
}
void pipe_line(char *inputL)
{
    char *inputL_ = strdup(inputL);

    int amper = amperCHECK(inputL_); //first check for &

    char *Pcommands[maxinputwords + 1];
    char *Scommands[maxinputwords + 1][maxinputwords + 1]; // 2d array to hold groups of space parsed commands
    int num_cmds = pipe_parse(inputL_, Pcommands);
    int s;
    for (s = 0; s < num_cmds; s++)
    {
        space_parse(Pcommands[s], Scommands[s]); // Parse the space-separated arguments for each command
    }

    int fds[2 * (num_cmds - 1)];
    // create pipes
    int j;
    for (j = 0; j < (num_cmds - 1); j++)
    {
        if (pipe(fds + j * 2) == -1)
        {
            perror("ERROR: pipe fail");
            exit(EXIT_FAILURE);
        }
    }

    int i;
    for (i = 0; i < num_cmds; i++)
    {
        int pid;
        pid = fork();
        if (pid == -1)
        {
            perror("ERROR: process creation failed");
            exit(EXIT_FAILURE);
        }

        if (pid == 0) // child
        {
            if (i == 0) // only very first command can potentially have input redir
            {
                char *INfilePart;
                int inTrue = 0;
                int a;
                for (a = 0; Scommands[i][a] != NULL; a++)
                {
                    if (strcmp(Scommands[i][a], "<") == 0) // case: cat < file.txt
                    {
                        inTrue = 1;
                        INfilePart = Scommands[i][a + 1];
                        Scommands[i][a] = NULL; // Null terminate arguments before input file so exec is smooth
                    }

                    else if (strchr(Scommands[i][a], '<') != NULL) //if < is attached to the word
                    {
                        inTrue = 1;
                        char *string = Scommands[i][a]; // The string containing '<'
                        int len = strlen(string);

                        if (string[0] == '<') // case: cat <file.txt
                        {
                            INfilePart = strtok(string + 1, "<");
                            INfilePart = trim_wht(INfilePart);
                            Scommands[i][a] = NULL; // this word is not needed for exec
                        }

                        else if (string[len - 1] == '<') // case: cat< file.txt
                        {
                            INfilePart = Scommands[i][a + 1]; // file is the next token
                            INfilePart = trim_wht(INfilePart);
                            Scommands[i][a + 1] = NULL; // this word is not needed for exec

                            char *carrotpos = strchr(Scommands[i][a], '<');
                            *carrotpos = '\0'; // null terminate the word before '<'
                        }

                        else // case: cat<file.txt
                        {
                            char *cmdPart = strtok(string, "<"); // Command before '<'
                            INfilePart = strtok(NULL, "<");      // File after '<'

                            cmdPart = trim_wht(cmdPart);
                            INfilePart = trim_wht(INfilePart);

                            Scommands[i][a] = cmdPart;
                            Scommands[i][a + 1] = NULL; // null terminate
                        }
                    }
                    if (inTrue) //if there is input redirection
                    {
                        int fd_IN = open(INfilePart, O_RDONLY);
                        if (fd_IN == -1)
                        {
                            perror("ERROR: opening input file failed");
                            exit(EXIT_FAILURE);
                        }

                        dup2(fd_IN, STD_IN); // input from file, not stdin
                        close(fd_IN);
                    }
                }
            }
            if (i == (num_cmds - 1)) // only very last command can potentially have output redir
            {
                char *OUTfilePart;
                int outTrue = 0;
                int a;
                for (a = 0; Scommands[i][a] != NULL; a++)
                {
                    if (strcmp(Scommands[i][a], ">") == 0) //case: echo > file.txt
                    {
                        outTrue = 1;
                        OUTfilePart = Scommands[i][a + 1];
                        Scommands[i][a] = NULL; // Null terminate arguments before output file so exec is smooth
                    }
                    else if (strchr(Scommands[i][a], '>') != NULL) //if > is attached to the word
                    {
                        outTrue = 1;
                        char *string = Scommands[i][a]; // The string containing '>'
                        int len = strlen(string);

                        if (string[0] == '>') // case: echo >file.txt
                        {
                            OUTfilePart = strtok(string + 1, ">");
                            OUTfilePart = trim_wht(OUTfilePart);
                            Scommands[i][a] = NULL; // this word is not needed for exec
                        }

                        else if (string[len - 1] == '>') // case: echo> file.txt
                        {
                            OUTfilePart = Scommands[i][a + 1]; // file is the next token
                            OUTfilePart = trim_wht(OUTfilePart);
                            Scommands[i][a + 1] = NULL; // this word is not needed for exec

                            char *carrotpos = strchr(Scommands[i][a], '>');
                            *carrotpos = '\0'; // null terminate the cmd with '>'
                        }

                        else // case: echo>file.txt
                        {
                            char *cmdPart = strtok(string, ">"); // Command before '<'
                            OUTfilePart = strtok(NULL, ">");     // File after '<'

                            cmdPart = trim_wht(cmdPart);
                            OUTfilePart = trim_wht(OUTfilePart);

                            Scommands[i][a] = cmdPart;
                            Scommands[i][a + 1] = NULL; // null terminate
                        }
                    }
                }

                if (outTrue) //if there is output redirection
                {
                    int fd_OUT = open(OUTfilePart, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_OUT == -1)
                    {
                        perror("ERROR: opening output file failed");
                        exit(EXIT_FAILURE);
                    }

                    dup2(fd_OUT, STD_OUT); // output to file, not stdout
                    close(fd_OUT);
                }
            }

            // if not very first cmd, set input to the previous pipe's read end
            // if first, input is simply STD_IN (ignores this if)
            if (i > 0)
                dup2(fds[(i - 1) * 2], STD_IN);

            // if not very last cmd, set output to the next pipe's write end
            // if last, output is simply STD_OUT (ignores this if)
            if (i < (num_cmds - 1))
                dup2(fds[i * 2 + 1], STD_OUT);

            // Close all child pipe FDs
            int h;
            for (h = 0; h < 2 * (num_cmds - 1); h++)
            {
                close(fds[h]);
            }

            if (execvp(Scommands[i][0], Scommands[i]) == -1)
            {
                perror("ERROR: execution failed");
                exit(EXIT_FAILURE);
            }
        }
    }

    int g; // close parent pipes
    for (g = 0; g < 2 * (num_cmds - 1); g++)
    {
        close(fds[g]);
    }

    if (!amper)
    {
        int k;
        for (k = 0; k < num_cmds; k++)
        {
            wait(NULL);
        }
    }
    
    else
    {
        signal(SIGCHLD, killthechildren);
    }
}

int main(int argc, char *argv[])
{

    int status;
    char input[maxinputwords * 32];
    int silent;
    if (argc > 1)
    {
        if (strcmp(argv[1], "-n") == 0)
            silent = 1;
    }

    while (1)
    {

        if (!silent)
            shell_start();

        if (fgets(input, (maxinputwords * 32), stdin) == NULL)
        {
            // printf("\nEXITING SHELL\n");  dont need apparently
            break;
        }

        if (input[0] == '\n')
        {
            // Just hit Enter, so skip everything and print the shell line again
            continue;
        }

        input[strcspn(input, "\n")] = '\0'; // trim newline character from fgets

        if (strchr(input, '|') == NULL) // just one command, no pipes
        {
            if (strchr(input, '<') != NULL && strchr(input, '>') != NULL) //< and > involved
            {
                int amper = amperCHECK(input);

                char *cmdPart = strtok(input, "<");    // Command before '<'
                char *INfilePart = strtok(NULL, ">");  // File after '<'
                char *OUTfilePart = strtok(NULL, ">"); // File after '>'

                cmdPart = trim_wht(cmdPart);
                INfilePart = trim_wht(INfilePart);
                OUTfilePart = trim_wht(OUTfilePart);

                char *Scommands[maxinputwords + 1];
                space_parse(cmdPart, Scommands); // parse just the command part

                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("ERROR: process creation failed");
                }
                if (pid > 0) // parent
                {
                    if (!amper)
                        waitpid(pid, &status, 0);
                    else
                        signal(SIGCHLD, killthechildren);
                }
                else
                {
                    // input part
                    int fd_IN = open(INfilePart, O_RDONLY);
                    if (fd_IN == -1)
                    {
                        perror("ERROR: opening input file failed");
                        exit(EXIT_FAILURE);
                    }

                    dup2(fd_IN, STD_IN); // input into file, not stdin
                    close(fd_IN);

                    // output part
                    int fd_OUT = open(OUTfilePart, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_OUT == -1)
                    {
                        perror("ERROR: opening output file failed");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_OUT, STD_OUT); // Redirect stdout to file
                    close(fd_OUT);

                    if (execvp(Scommands[0], Scommands) == -1)
                    {
                        perror("ERROR: execution failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            else if (strchr(input, '>') != NULL) // just > involved
            {
                int amper = amperCHECK(input);

                char *cmdPart = strtok(input, ">");    // Command before '>'
                char *OUTfilePart = strtok(NULL, ">"); // File after '>'

                cmdPart = trim_wht(cmdPart);
                OUTfilePart = trim_wht(OUTfilePart);

                char *Scommands[maxinputwords + 1];
                space_parse(cmdPart, Scommands); // parse just the command part

                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("ERROR: process creation failed");
                }
                if (pid > 0) // parent
                {
                    if (!amper)
                        waitpid(pid, &status, 0);
                    else
                        signal(SIGCHLD, killthechildren);
                }
                else
                {
                    int fd_OUT = open(OUTfilePart, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_OUT == -1)
                    {
                        perror("ERROR: opening output file failed");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_OUT, STD_OUT); // Redirect stdout to file
                    close(fd_OUT);

                    if (execvp(Scommands[0], Scommands) == -1)
                    {
                        perror("ERROR: execution failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            else if (strchr(input, '<') != NULL) // just < involved
            {
                int amper = amperCHECK(input);

                char *cmdPart = strtok(input, "<");   // Command before '<'
                char *INfilePart = strtok(NULL, "<"); // File after '<'

                cmdPart = trim_wht(cmdPart);
                INfilePart = trim_wht(INfilePart);

                char *Scommands[maxinputwords + 1];
                space_parse(cmdPart, Scommands); // parse just the command part

                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("ERROR: process creation failed");
                }
                if (pid > 0) // parent
                {
                    if (!amper)
                        waitpid(pid, &status, 0);
                    else
                        signal(SIGCHLD, killthechildren);
                }
                else
                {
                    int fd_IN = open(INfilePart, O_RDONLY);
                    if (fd_IN == -1)
                    {
                        perror("ERROR: opening input file failed");
                        exit(EXIT_FAILURE);
                    }

                    dup2(fd_IN, STD_IN); // input into file, not stdin
                    close(fd_IN);

                    if (execvp(Scommands[0], Scommands) == -1)
                    {
                        perror("ERROR: execution failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            else
            {
                int amper = amperCHECK(input);

                char *Scommands[maxinputwords + 1]; // hold space-parsed statements
                space_parse(input, Scommands);      // space parse first(only) command

                pid_t pid = fork();
                if (pid == -1)
                {
                    perror("ERROR: process creation failed");
                }
                if (pid > 0) // parent
                {
                    if (!amper)
                        waitpid(pid, &status, 0);
                    else
                        signal(SIGCHLD, killthechildren);
                }
                else
                {

                    if (execvp(Scommands[0], Scommands) == -1)
                    {
                        perror("ERROR: execution failed");
                        exit(EXIT_FAILURE);
                    }
                }
            }
        }

        else
        {
            pipe_line(input);
        }
    }
}
