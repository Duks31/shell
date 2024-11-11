#ifndef SHELL_H_
#define SHELL_H_

int sh_cd(char ***args);
int sh_help(char ***args);
int sh_exit(char ***args);
int sh_time(char ***args);

char *builtin_strs[] = {"cd", "help", "exit", "time"};

int (*builtin_func[])(char ***) = {&sh_cd, &sh_help, &sh_exit, &sh_time};

int sh_num_builtins() { return sizeof(builtin_strs) / sizeof(char *); }

int sh_run(char ***, int);

char *read_line();
char **split_pipes(char *);
char ***split_args(char **);

void execute_command(char **, int, int);
int execute_pipeline(char ***, int);

#endif
