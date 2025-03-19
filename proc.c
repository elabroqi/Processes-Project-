#include <stdio.h>
#include <unistd.h>   // fork()
#include <stdlib.h>   // exit()
#include <inttypes.h> // intmax_t
#include <sys/wait.h> // wait()
#include <string.h>   // strcspn(), strtok
#include <errno.h>    // ECHILD
#include <fcntl.h>    // O_RDONLY, open

#define MAX_COMMANDS 10
#define BUFFER_SIZE 256

void hasPipe(char *commands[], int pipe_index, int redirect_index, int ncommands);
void hasRedirection(char *commands[], int redirect_index, int ncommands);

int main()
{

  char buffer[BUFFER_SIZE];
  char *commands[MAX_COMMANDS];
  int ncommands;

  int redirect_index = -1, pipe_index = -1;

  while (1)
  {
    printf("$ ");

    fflush(stdout);

    // Read input from the user
    if (!fgets(buffer, BUFFER_SIZE, stdin))
    {
      // use ctrl-d to end the input
      fprintf(stderr, "no more input\n");
      exit(EXIT_SUCCESS);
    }

    buffer[strcspn(buffer, "\n")] = '\0';

    ncommands = 0;

    char *delimiters = " ";
    char *token = strtok(buffer, delimiters);

    if (token == NULL)
    {
      continue;
    }

    // Loading tokens in to commands array
    while (token != NULL && ncommands < MAX_COMMANDS)
    {
      commands[ncommands++] = token;
      token = strtok(NULL, delimiters);
    }
    commands[ncommands] = NULL;

    // Flagging indexes for redirection and piping
    for (int i = 0; i < ncommands; i++)
    {
      if (commands[i][0] == '<' || commands[i][0] == '>')
      {
        if (redirect_index != -1)
        {
          // Checks Multiple Input/Output Redirects
          fprintf(stderr, "Error: Multiple %s redirects \n", commands[i][0] == '<' ? "input" : "output");
          exit(EXIT_FAILURE);
        }
        redirect_index = i;
      }
      if (strcmp(commands[i], "|") == 0)
      {
        pipe_index = i;
      }
    }

    // Checks if FileName is Empty
    char *path = commands[redirect_index] + 1;
    if (*path == '\0')
    {
      fprintf(stderr, "Error: %s Redirect is Empty\n", commands[redirect_index][0] == '<' ? "Input" : "Output");
      exit(EXIT_FAILURE);
    }

    // Checks for valid positioning of '<' and '>'
    if (redirect_index != -1 && pipe_index != -1)
    {
      if (commands[redirect_index][0] == '<' && redirect_index > pipe_index)
      {
        fprintf(stderr, "Error: Input redirection cannot occur after a pipe\n");
        exit(EXIT_FAILURE);
      }
      if (commands[redirect_index][0] == '>' && redirect_index < pipe_index)
      {
        fprintf(stderr, "Error: Output redirection cannot occur before a pipe\n");
        exit(EXIT_FAILURE);
      }
    }

    // Has Redirection Only
    if (redirect_index != -1 && pipe_index == -1)
    {
      hasRedirection(commands, redirect_index, ncommands);
    }
    // Has Pipe Only
    else if (pipe_index != -1 && redirect_index == -1)
    {
      hasPipe(commands, pipe_index, redirect_index, ncommands);
    }
    // Has Pipe and Redirection
    else if (redirect_index != -1 && pipe_index != -1)
    {
      hasPipe(commands, pipe_index, redirect_index, ncommands);
    }
    // Has Neither Pipe or Redirection
    else
    {
      pid_t pid = fork();
      if (pid == 0)
      {
        execvp(commands[0], commands);
        perror("execvp"); // Program cannot be executed
      }
      waitpid(pid, NULL, 0);
    }
  }
}

void hasPipe(char *commands[], int pipe_index, int redirect_index, int ncommands)
{

  if (pipe_index == -1 || pipe_index == 0 || pipe_index == ncommands - 1)
  {
    fprintf(stderr, "Error Invalid pipe index");
    exit(EXIT_FAILURE);
  }

  int pipefd[2];
  int redirect_type = 0; // either '<' or '>'
  pid_t leftpid, rightpid;

  if (commands[redirect_index][0] == '<')
  {
    redirect_type = 1; // input
  }
  else if (commands[redirect_index][0] == '>')
  {
    redirect_type = 2; // output
  }

  // create pipe
  if (pipe(pipefd) == -1)
  {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  leftpid = fork();
  if (-1 == leftpid)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  // left child process
  if (0 == leftpid)
  {
    if (close(pipefd[0]) == -1)
    { // close read end, since that will be used by the second program
      perror("close");
      exit(EXIT_FAILURE);
    }

    if (dup2(pipefd[1], STDOUT_FILENO) == -1) // Redirected Standard Out of Left Pipe Program
    {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    if (close(pipefd[1]) == -1)
    {
      perror("close");
      exit(EXIT_FAILURE);
    }

    if (redirect_type == 1 && redirect_index < pipe_index)
    {
      char *inputFile = commands[redirect_index] + 1;
      int fd = open(inputFile, O_RDONLY);
      if (fd == -1)
      {
        perror("open");
        exit(EXIT_FAILURE); // Input Redirect File Cannot Be Opened
      }
      if (dup2(fd, STDIN_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
      close(fd);
    }

    // seperating out left commnads (exclude redirection)
    char *left_cmd[MAX_COMMANDS];
    int j = 0;
    for (int i = 0; i < pipe_index; i++)
    {
      if (i == redirect_index)
        continue; // Skip redirection command
      left_cmd[j++] = commands[i];
    }
    left_cmd[j] = NULL;

    execvp(left_cmd[0], left_cmd);
    perror("execvp (left command)"); // Program cannot be executed
  }
  else
  {
    // in parent
  }

  rightpid = fork();
  if (-1 == rightpid)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (0 == rightpid)
  {
    // child
    if (close(pipefd[1]) == -1)
    { // close write, since that is being used by the first program
      perror("close");
      exit(EXIT_FAILURE);
    }

    if (dup2(pipefd[0], STDIN_FILENO) == -1) // Redirected Standard In of Right Pipe Program
    {
      perror("dup2");
      exit(EXIT_FAILURE);
    }

    if (close(pipefd[0]) == -1)
    {
      perror("close");
      exit(EXIT_FAILURE);
    }

    if (redirect_type == 2 && redirect_index > pipe_index)
    {
      char *outputFile = commands[redirect_index] + 1;
      int fd = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1)
      {
        perror("open"); // Output Redirect File Cannot Be Opened
        exit(EXIT_FAILURE);
      }
      if (dup2(fd, STDOUT_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
      close(fd);
    }

    // seperating out right commnads
    char *right_cmd[MAX_COMMANDS];
    int j = 0;
    for (int i = pipe_index + 1; i < ncommands; i++)
    {
      if (i == redirect_index)
        continue;
      right_cmd[j++] = commands[i];
    }
    right_cmd[j] = NULL;

    execvp(right_cmd[0], right_cmd);
    perror("execvp (right command)"); // Program cannot be executed
  }

  else
  {
    // in parent
  }

  // back in parent
  // close pipefds
  if (close(pipefd[0]) == -1)
  {
    perror("close");
    exit(EXIT_FAILURE);
  }

  if (close(pipefd[1]) == -1)
  {
    perror("close");
    exit(EXIT_FAILURE);
  }

  // Wait for all child processes using `ECHILD`
  do
  {
    while (wait(NULL) > 0)
      ;
  } while (errno != ECHILD);
}

void hasRedirection(char *commands[], int redirect_index, int ncommands)
{

  char *path = commands[redirect_index] + 1;
  int fd;
  pid_t pid = fork();

  if (pid == -1)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0)
  {
    // in child
    if (commands[redirect_index][0] == '>')
    {
      fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);

      if (fd == -1)
      {
        perror("open");
        exit(EXIT_FAILURE); // Output Redirect File Cannot Be Opened
      }

      if (dup2(fd, STDOUT_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
    }
    else if (commands[redirect_index][0] == '<')
    {

      fd = open(path, O_RDONLY);

      if (fd == -1)
      {
        perror("open"); // Input Redirect File Cannot Be Opened
        exit(EXIT_FAILURE);
      }

      if (dup2(fd, STDIN_FILENO) == -1)
      {
        perror("dup2");
        exit(EXIT_FAILURE);
      }
    }

    char *newCommands[MAX_COMMANDS];
    int j = 0;
    for (int i = 0; i < redirect_index; i++)
    {
      newCommands[j++] = commands[i];
    }
    newCommands[j] = NULL;

    execvp(newCommands[0], newCommands);
    perror("execvp"); // Program cannot be executed
  }

  // in parent
  do
  {
    while (wait(NULL) > 0)
      ;
  } while (errno != ECHILD);
}
