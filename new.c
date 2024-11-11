#include <linux/limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <header.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

int main()
{
    int status = 0;
    while (true)
    {
        char *cwd = getcwd(NULL, 0);
        printf("%s => ", cwd);

        char *line = read_line();
        char **commands = split_pipes(line);
        char ***args = split_args(commands, &status);
        if (status == -1)
        {
            fprintf(stderr, "shell: Expected end of quoted string\n");
        }
        else if (sh_run(args, status) == 0)
        {
            exit(1);
        }

        for (int i = 0; args[i]; i++)
        {
            free(args[i]);
        }
        free(args);
        free(commands);
        free(line);
        free(cwd);
    }
}

char *read_line()
{
    char *line = NULL;
    size_t bufsize = 0;
    if (getline(&line, &bufsize, stdin) == -1)
    {
        if (feof(stdin))
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            perror("readline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}

char **split_pipes(char *line)
{
    char **commands = calloc(16, sizeof(char *));
    if (!commands)
    {
        perror("calloc failed");
        exit(1);
    }

    char *command;
    int pos = 0;

    command = strtok(line, "|");
    while (command)
    {
        commands[pos++] = command;
        command = strtok(NULL, "|");
    }

    for (int i = 0; i < pos; i++)
    {
        while (*commands[i] == ' ' || *commands[i] == '\n')
        {
            commands[i]++;
        }

        char *end_of_str = strrchr(commands[i], '\0');
        --end_of_str;
        while (*end_of_str == ' ' || *end_of_str == '\n')
        {
            *end_of_str = '\0';
            --end_of_str;
        }
    }
}

char ***split_args(char **commands, int *status)
{
    int outer_pos = 0;
    char ***command_args = calloc(16, sizeof(char **));
    if (!command_args)
    {
        perror("calloc failed");
        exit(1);
    }

    for (int i = 0; commands[i]; i++)
    {
        int pos = 0;
        char **args = calloc(16, sizeof(char *));
        if (!args)
        {
            perror("calloc failed");
            exit(1);
        }

        bool inside_string = false;
        char *current_token = &commands[i][0];

        for (int j = 0; commands[i][j]; j++)
        {
            if (commands[i][j] == '"' && !inside_string)
            {
                // Begining of a string
                commands[i][j++] = '\0';
                if (commands[i][j] != '"')
                {
                    inside_string = true;
                    current_token = &commands[i][j];
                }
                else
                {
                    commands[i][j] = '\0';
                    current_token = &commands[i][++j];
                }
            }
            else if (inside_string)
            {
                if (commands[i][j] == '"')
                {
                    // Ending of the string
                    inside_string = false;
                    commands[i][j] = '\0';
                    args[pos++] = current_token;
                    current_token = NULL;
                }
                else
                {
                    // character in the string
                    continue;
                }
            }
            else if (commands[i][j] == ' ')
            {
                // space delimeter
                if (current_token && *current_token != ' ')
                {
                    args[pos++] = current_token;
                }
                current_token = &commands[i][j + 1];
                commands[i][j] = '\0';
            }
            else if (commands[i][j] == '$' && commands[i][j + 1] && commands[i][j + 1] != ' ')
            {
                // Environmental variable
                args[pos++] = getenv(&commands[i][++j]);
                while (commands[i][j])
                {
                    ++j;
                }
                current_token = &commands[i][j + 1];
            }
            else
            {
                // regular character
                continue;
            }
        }

        if (inside_string)
        {
            *status = -1;
            return command_args;
        }
        else if (current_token && *current_token != ' ')
        {
            args[pos++] = current_token;
        }

        command_args[outer_pos++] = args;
    }

    *status = outer_pos;
    return command_args;
}

int sh_run(char ***commands, int num_commands)
{
    if (commands[0][0] == NULL)
    {
        // No command given
        return 1;
    }

    // check shell builtins
    for (int i = 0; i < sh_num_builtins(); i++)
    {
        if (strcmp(commands[0][0], builtin_strs[i]) == 0)
        {
            return (*builtin_func[i])(commands);
        }
    }
    return execute_pipeline(commands, num_commands);
}

int sh_cd(char ***args)
{
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);

    if (!pw)
    {
        perror("Retrieve Home Dir");
        return 0;
    }
    if (args[0][1] == NULL)
    {
        if (chdir(pw->pw_dir) != 0)
        {
            perror("sh: cd");
        }
    }
    else
    {
        char path[PATH_MAX];
        char expanded_path[PATH_MAX];
        char *tilda = strchr(args[0][1], '~');
        if (tilda)
        {
            strcpy(path, pw->pw_dir);
            strcat(path, args[0][1]);
        }
        else
        {
            strcpy(path, args[0][1]);
        }

        char *exp_path = realpath(path, expanded_path);
        if (!exp_path)
        {
            perror("realpath");
            return 0;
        }
        if (chdir(exp_path) != 0)
        {
            perror("sh: cd");
            return 0;
        }
        if (chdir(exp_path) != 0)
        {
            perror("sh: cd");
        }
    }
    return 1;
}

int sh_help(char ***args)
{
    printf("Shell Help Page\n\n");
    printf("Available commands: \n");

    for (int i =0; i < sh_num_builtins(); i++) {
        printf("- %s\n", builtin_strs[i]);
    }
}

int sh_exit(char ***args) {return 0;}

int sh_time(char ***args)
{
    args[0]++;

    int i = 0;
    while (args[i++]) {;}

    struct timespec start, end;
    double elapsed_time;

    clock_gettime(CLOCK_MONOTONIC, &start);
    int res = sh_run(args, i);
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1e6;
    printf("Time: %.3f milliseconds\n", elapsed_time); 

    // Return pointer to "time" for free()
    args[0]--;
    return res; 
}

int execute_pipeline(char ***commands, int num_commands)
{
    // No commands given 
    if (commands[0][0][0] == '\0') {    
        return 1;
    }

    // create pipes
    int **pipes = malloc((num_commands - 1) * sizeof(int *));
    for (int i = 0; i < num_commands - 1; i++) {
        pipes[i] = malloc(2 * sizeof(int));
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return 0;
        }
    }
    
    // Execute commands in the pipeline
    for (int i = 0; i < num_commands - 1; i++) {
        int input_fd = (i == 0) ? STDIN_FILENO : pipes[i-1][0];
        int output_fd = pipes[i][1];

        execute_command(commands[i], input_fd, output_fd);
        close(output_fd);
    }

    // Execute last (or only) command
    int input_fd = (num_commands > 1) ? pipes[num_commands-2][0] : STDIN_FILENO;
    int output_fd = STDOUT_FILENO;
    execute_command(commands[num_commands-1], input_fd, output_fd);

    // close/cleanup pipes
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
        free(pipes[i]);
    }

    free(pipes);
    return 1;
}   

void execute_command(char **args, int input_fd, int output_fd) {
    // create child

    pid_t pid;
    pid_t wpid;
    int status;

    if ((pid = fork()) == 0) {
        // child process context

        // redirect in and out file descriptors if needed
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDOUT_FILENO); 
            close(input_fd);
        }

        // Execute command
        if (execvp(args[0], args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        // fork failed 
        perror("fork");
        exit(EXIT_FAILURE);
    }  else {
        // parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALD(status));
    }
}