#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>

#define PORT 9002
#define MIRROR1_PORT 9003
#define MIRROR2_PORT 9004

#define DIRLIST_A_PIPE 3
#define DIRLIST_T_PIPE 4

char *extract_columns(char *buffer)
{
    char *result = malloc(strlen(buffer) + 1); // Allocate memory for the result
    if (result == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    char *token;
    char *delimiter = " ";
    char *saveptr;

    // Tokenize first line of the buffer
    token = strtok_r(buffer, "\n", &saveptr);
    // Create an array of the first line extracted
    char *columns[8];
    int col_num = 0;

    // Tokenize each column of the line
    char *col_token = strtok(token, delimiter);
    while (col_token != NULL)
    {
        columns[col_num++] = col_token;
        col_token = strtok(NULL, delimiter);
    }

    // Split the buffer by '/' to extract the filename
    char *filename = strrchr(columns[7], '/');
    if (filename == NULL)
    {
        // If no '/' found, do nothing
    }
    else
    {
        columns[7] = filename + 1;
    }

    // Extract the desired columns and concatenate them into the result string
    strcat(result, columns[7]); // 8th column
    strcat(result, " ");
    strcat(result, columns[4]); // 5th column
    strcat(result, " ");
    strcat(result, columns[5]); // 6th column
    strcat(result, " ");
    strcat(result, columns[0]); // 1st column
    strcat(result, "\0");

    return result;
}

char *find_file(char *fname)
{
    char buffer[1024];
    int pipe_fd[2];

    if (pipe(pipe_fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Connect input to previous pipe
        if (dup2(pipe_fd[0], STDIN_FILENO) == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        // Connect output to next pipe
        if (dup2(pipe_fd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }

        // Close all pipe descriptors
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        // Execute commands
        char *home_dir = getenv("HOME");
        if (home_dir == NULL)
        {
            perror("getenv");
            exit(EXIT_FAILURE);
        }
        char *args[] = {"find", home_dir, "!", "-path", "*/.*", "-name", fname, "-exec", "ls", "-l", "--time-style=long-iso", "{}", ";", NULL};
        execvp("find", args);

        perror("execvp");
        exit(EXIT_FAILURE); // Ensure child process terminates after executing command
    }
    else
    {

        // Wait for the child to finish its execution
        int status;
        if (waitpid(pid, &status, 0) == -1)
        {
            perror("waitpid");
            exit(EXIT_FAILURE);
        }

        // Close the write end of the pipe
        close(pipe_fd[1]);

        // Read result from the last process
        ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer));
        if (bytes_read == -1)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }
        else if (bytes_read == 0)
        {
            return ("File not found\0");
        }
        buffer[bytes_read] = '\0'; // Null-terminate the string

        // Close unused pipe descriptors in parent
        close(pipe_fd[0]);

        return (extract_columns(buffer));
    }
}

// Function to tokenize a string and return a list of strings
char **tokenizeString(const char *input, int *count)
{
    // Initialize the count of tokens
    *count = 0;

    // Tokenize the input string to count the number of tokens
    char *temp_input = strdup(input); // Make a copy of the input string
    char *token = strtok(temp_input, " ");
    while (token != NULL)
    {
        (*count)++;
        token = strtok(NULL, " ");
    }
    free(temp_input); // Free the memory allocated for the temporary copy

    // Allocate memory for an array of pointers to strings
    char **arguments = malloc(sizeof(char *) * (*count));
    if (arguments == NULL)
    {
        *count = 0;
        return NULL; // Memory allocation failed
    }

    // Tokenize the input string again to fill the array of pointers
    token = strtok((char *)input, " ");
    for (int i = 0; i < *count; i++)
    {
        // Allocate memory for the token
        arguments[i] = malloc(strlen(token) + 1);
        if (arguments[i] == NULL)
        {
            // Memory allocation failed, free previously allocated memory
            for (int j = 0; j < i; j++)
            {
                free(arguments[j]);
            }
            free(arguments);
            *count = 0;
            return NULL;
        }

        // Copy the token into the array
        strcpy(arguments[i], token);

        // Move to the next token
        token = strtok(NULL, " ");
    }

    return arguments;
}

char *execute_commands(char ***allArgs, int allArgsSize)
{
    char buffer[5000];
    int pipes[allArgsSize][2];

    // Initialize pipes
    for (int i = 0; i < allArgsSize; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Execute commands
    for (int i = 0; i < allArgsSize; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            // Child process
            if (i > 0)
            {
                // Connect input to previous pipe
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            if (i < allArgsSize)
            {
                // Connect output to next pipe
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }

            // Close all pipe descriptors
            for (int j = 0; j < allArgsSize; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Execute commands
            execvp(allArgs[i][0], allArgs[i]);
            perror("execvp");
            exit(EXIT_FAILURE); // Ensure child process terminates after executing command
        }
    }

    // Close unused pipe descriptors in parent
    for (int i = 0; i < allArgsSize - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < allArgsSize; i++)
    {
        wait(NULL);
    }

    // Close write end of the last pipe
    close(pipes[allArgsSize - 1][1]);

    // Read result from the last process
    ssize_t bytes_read = read(pipes[allArgsSize - 1][0], buffer, sizeof(buffer));

    // Close read end of the last pipe
    close(pipes[allArgsSize - 1][0]);

    if (bytes_read == -1)
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    buffer[bytes_read] = '\0'; // Null-terminate the string

    // Allocate memory for the result
    char *result = malloc(bytes_read + 1);
    if (result == NULL)
    {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    // Copy result and return, replacing newline characters with spaces
    for (ssize_t i = 0; i < bytes_read; i++)
    {
        if (buffer[i] == '\n')
        {
            result[i] = ' ';
        }
        else
        {
            result[i] = buffer[i];
        }
    }
    result[bytes_read] = '\0'; // Null-terminate the string
    return result;
}

void send_tar_to_client(int client_socket, char *temp_file_name)
{
    char file_buffer[256] = {0};
    FILE *file;
    ssize_t bytes_read;
    struct stat file_stat;
    char file_size[256];
    int remain_data;

    // Open file
    file = fopen(temp_file_name, "rb");
    if (file == NULL)
    {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Get file descriptor from FILE pointer
    int file_descriptor = fileno(file);

    // Get file stats
    if (fstat(file_descriptor, &file_stat) < 0)
    {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    sprintf(file_size, "%d", file_stat.st_size);
    write(client_socket, file_size, sizeof(file_size));
    remain_data = file_stat.st_size;
    // Read file content into buffer and send over socket
    while (((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) && (remain_data > 0))
    {
        // printf("%s_%d\n",file_buffer,bytes_read);
        if (write(client_socket, file_buffer, bytes_read) != bytes_read)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        remain_data -= bytes_read;
    }
    printf("File sent successfully\n");
    // char* message = "END_OF_FILE\0";
    // write(client_socket,message, strlen(message)+1);

    fclose(file);
}

int tar_creation_using_commands(int client_socket, char *command, char *home_dir, char ***allArgs, int allArgsSize)
{
    int count;

    char *result = execute_commands(allArgs, allArgsSize);
    printf("Result : %s %d\n", result, strlen(result));
    // Check if result is null or empty and return failure
    char *message;
    if (result == NULL || *result == '\0' || strlen(result) == 0)
    {
        message = "No file found\0";
        write(client_socket, message, strlen(message) + 1);
        return 0;
    }
    else
    {
        message = "File found\0";
        write(client_socket, message, strlen(message) + 1);
    }

    // Create temporary tar file for the result
    // Get the process ID (PID)
    pid_t pid = getpid();
    // Create a buffer to hold the filename
    char tempFileName[50]; // Adjust the size as needed
    // Format the filename string using sprintf
    sprintf(tempFileName, "temp_tar_%d.tar.gz", (int)pid);

    // Tokenize the input string
    char **resultsArr = tokenizeString(result, &count);
    // Constructing the arguments for tar command
    char *tarArgs[count + 6]; // Additional arguments plus count of files
    tarArgs[0] = "tar";
    tarArgs[1] = "-czf";
    tarArgs[2] = tempFileName;
    for (int i = 0; i < count; i++)
    {
        tarArgs[i + 3] = resultsArr[i];
        // fflush(stdout); // Ensure output is immediately visible
    }
    tarArgs[count + 3] = "--transform";
    tarArgs[count + 4] = "s|.*/||";
    tarArgs[count + 5] = NULL;

    // Combine all arrays into one array
    char **allArgsTar[] = {tarArgs};

    // Execute and create the temp tar file
    result = execute_commands(allArgsTar, 1);
    // Send the tar file to the user
    send_tar_to_client(client_socket, tempFileName);

    // Remove the temp tar file once used
    remove(tempFileName);
    return 1;
}

// Function to convert date string to Unix timestamp
time_t convertDateToTimestamp(const char *dateString, int hr, int min, int sec)
{
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(struct tm)); // Initialize to all zeros to avoid garbage values

    if (strptime(dateString, "%Y-%m-%d", &timeinfo) == NULL)
    {
        // Parsing failed
        perror("strptime");
        exit(EXIT_FAILURE);
    }

    // Set time fields
    timeinfo.tm_hour = hr;
    timeinfo.tm_min = min;
    timeinfo.tm_sec = sec;

    // Convert to Unix timestamp
    time_t timestamp;
    if (setenv("TZ", "UTC", 1) != 0)
    { // Set time zone to UTC
        perror("setenv");
        exit(EXIT_FAILURE);
    }
    timestamp = mktime(&timeinfo);
    if (timestamp == -1)
    {
        // Conversion failed
        perror("mktime");
        exit(EXIT_FAILURE);
    }

    return timestamp;
}

void crequest(int client_socket)
{
    char *home_dir = getenv("HOME");
    if (home_dir == NULL)
    {
        perror("getenv");
        exit(EXIT_FAILURE);
    }
    while (1)
    {
        // Server message to the client
        char command[512];

        read(client_socket, &command, sizeof(command));
        printf("Command: %s %d\n", command, strlen(command));

        // Compare command against different strings
        if (strcmp(command, "dirlist -a") == 0)
        {
            char *args1[] = {"find", home_dir, "-mindepth", "1", "-maxdepth", "1", "-type", "d", "!", "-name", ".*", NULL};
            char *args2[] = {"sort", NULL};
            char *args3[] = {"xargs", "-n", "1", "basename", NULL};
            // Combine all arrays into one array
            char **allArgs[] = {args1, args2, args3};
            char *result = execute_commands(allArgs, 3);
            printf("Result : %s\n", result);
            // Send a message to the client
            write(client_socket, result, strlen(result) + 1);
        }
        else if (strcmp(command, "dirlist -t") == 0)
        {
            char *args1[] = {"find", home_dir, "-mindepth", "1", "-maxdepth", "1", "-type", "d", "!", "-name", ".*", "-exec", "stat", "--format=%W %n", "{}", ";", NULL};
            char *args2[] = {"sort", "-n", NULL};
            char *args3[] = {"cut", "-d", " ", "-f2", NULL};
            char *args4[] = {"xargs", "-n1", "basename", NULL};
            // Combine all arrays into one array
            char **allArgs[] = {args1, args2, args3, args4};
            char *result = execute_commands(allArgs, 4);
            printf("Result : %s\n", result);
            // Send a message to the client
            write(client_socket, result, strlen(result) + 1);
        }
        else if (strncmp(command, "w24fn", strlen("w24fn")) == 0)
        {

            // Tokenize the command to extract the filename
            char *token = strtok(command, " "); // Get the first token
            token = strtok(NULL, " ");          // Get the second token

            // Check if the second token (filename) exists
            if (token != NULL)
            {
                char *result = find_file(token);
                printf("Result : %s\n", result);
                // Send a message to the client
                write(client_socket, result, strlen(result) + 1);
            }
            else
            {
                char *message = "Filename not provided\0";
                write(client_socket, message, strlen(message));
                // Handle the case where filename is not provided
            }
        }
        else if (strncmp(command, "w24fz", strlen("w24fz")) == 0)
        {
            // Variables to store the list of strings and count of tokens
            char **userArgs;
            int count;

            // Tokenize the input string
            userArgs = tokenizeString(command, &count);
            // Constructing the arguments for the find command
            char sizeArg1[strlen(userArgs[1]) + 3]; // Buffer to hold the concatenated size argument
            sprintf(sizeArg1, "+%sc", userArgs[1]);

            char sizeArg2[strlen(userArgs[2]) + 3]; // Buffer to hold the concatenated size argument
            sprintf(sizeArg2, "-%sc", userArgs[2]);
            printf("\n%s %s\n", sizeArg1, sizeArg2);
            // Constructing the arguments for find command
            char *args1[] = {"find", home_dir, "-type", "f", "-size", sizeArg1, "-size", sizeArg2, "-not", "-path", "*/.*/*", "!", "-name", ".*", NULL};

            // Combine all arrays into one array
            char **allArgs[] = {args1};

            int success = tar_creation_using_commands(client_socket, command, home_dir, allArgs, 1);

            // Free memory allocated for the list of strings
            for (int i = 0; i < count; ++i)
            {
                free(userArgs[i]);
            }
            free(userArgs);
        }
        else if (strncmp(command, "w24ft", strlen("w24ft")) == 0)
        {
            // Allocate memory for file extensions
            char *fileExt1 = NULL;
            char *fileExt = NULL;

            // Variables to store the list of strings and count of tokens
            char **userArgs;
            int count;

            // Tokenize the input string
            userArgs = tokenizeString(command, &count);

            // Constructing the arguments for find command
            // char *args1[] = {"find", "/home/deon", "-type", "f", "(", "-name", "*.txt", "-o", "-name", "*.pdf", "-o", "-name", "*.docx", ")", "-not", "-path", "*/.*/*", NULL};
            char *args1[18]; // Maximum number of elements considering all possibilities
            int argCount = 0;
            args1[argCount++] = "find";
            args1[argCount++] = "/home/deon";
            args1[argCount++] = "-type";
            args1[argCount++] = "f";
            args1[argCount++] = "(";

            // Add file extensions based on user input

            if (userArgs[1] != NULL)
            {
                fileExt1 = malloc(strlen(userArgs[1]) + 3); // +3 for the wildcard (*) and dot (.)
                sprintf(fileExt1, "*.%s", userArgs[1]);
                args1[argCount++] = "-name";
                args1[argCount++] = fileExt1;
            }
            for (int n = 2; n < count; n++)
            {
                if (userArgs[n] != NULL)
                {
                    fileExt = malloc(strlen(userArgs[n]) + 3); // +3 for the wildcard (*) and dot (.)
                    sprintf(fileExt, "*.%s", userArgs[n]);
                    args1[argCount++] = "-o";
                    args1[argCount++] = "-name";
                    args1[argCount++] = fileExt;
                }
            }
            args1[argCount++] = ")";
            args1[argCount++] = "-not";
            args1[argCount++] = "-path";
            args1[argCount++] = "*/.*/*";
            args1[argCount++] = NULL;

            for (int x = 0; x < argCount; x++)
            {
                printf("%s ", args1[x]);
            }
            printf("\n");

            // Combine all arrays into one array
            char **allArgs[] = {args1};

            int success = tar_creation_using_commands(client_socket, command, home_dir, allArgs, 1);

            // Free memory allocated for the list of strings
            for (int i = 0; i < count; ++i)
            {
                free(userArgs[i]);
            }
            free(userArgs);
        }
        else if (strncmp(command, "w24fdb", strlen("w24fdb")) == 0)
        {
            // Variables to store the list of strings and count of tokens
            char **userArgs;
            int count;

            // Tokenize the input string
            userArgs = tokenizeString(command, &count);
            // Constructing the arguments for the find command
            // Convert date string to Unix timestamp
            long timestamp = convertDateToTimestamp(userArgs[1], 23, 59, 59);
            // Convert long integer to string
            char dateArg1[20];
            sprintf(dateArg1, "d=%ld", timestamp);

            printf("\ndate-%s\n", dateArg1);
            // Constructing the arguments for find command
            // Command 1
            char *args1[] = {"find", home_dir, "-type", "f", "-not", "-path", "*/.*/*", "!", "-name", ".*", "-exec", "stat", "--format", "%W %n", "{}", "+", NULL};

            // Command 2
            char *args2[] = {"awk", "-v", dateArg1, "$1 <= d", NULL};

            // Command 3
            char *args3[] = {"cut", "-d", " ", "-f2-", NULL};

            // Combine all arrays into one array
            char **allArgs[] = {args1, args2, args3};

            int success = tar_creation_using_commands(client_socket, command, home_dir, allArgs, 3);

            // Free memory allocated for the list of strings
            for (int i = 0; i < count; ++i)
            {
                free(userArgs[i]);
            }
            free(userArgs);
        }
        else if (strncmp(command, "w24fda", strlen("w24fda")) == 0)
        {
            // Variables to store the list of strings and count of tokens
            char **userArgs;
            int count;

            // Tokenize the input string
            userArgs = tokenizeString(command, &count);
            // Constructing the arguments for the find command
            // Convert date string to Unix timestamp
            long timestamp = convertDateToTimestamp(userArgs[1], 0, 0, 0);
            // Convert long integer to string
            char dateArg1[20];
            sprintf(dateArg1, "d=%ld", timestamp);

            printf("date-%s\n", dateArg1);
            // Constructing the arguments for find command
            // Command 1
            char *args1[] = {"find", home_dir, "-type", "f", "-not", "-path", "*/.*/*", "!", "-name", ".*", "-exec", "stat", "--format", "%W %n", "{}", "+", NULL};

            // Command 2
            char *args2[] = {"awk", "-v", dateArg1, "$1 >= d", NULL};

            // Command 3
            char *args3[] = {"cut", "-d", " ", "-f2-", NULL};

            // Combine all arrays into one array
            char **allArgs[] = {args1, args2, args3};

            int success = tar_creation_using_commands(client_socket, command, home_dir, allArgs, 3);

            // Free memory allocated for the list of strings
            for (int i = 0; i < count; ++i)
            {
                free(userArgs[i]);
            }
            free(userArgs);
        }
        else if (strncmp(command, "quitc", strlen("quitc")) == 0)
        {
            break;
        }
        else
        {
            char *message = "Command Not Valid\0";
            write(client_socket, message, strlen(message) + 1);
        }
    }
}

int main()
{
    int count = 1;
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create socket file descriptor | SOCK_STREAM = TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    // htonl(INADDR_ANY) (htonl = host to network long) used by prof -> Network represents the ip address as network byte order
    address.sin_addr.s_addr = htonl(INADDR_ANY); // Added htonl
    address.sin_port = htons((uint16_t)PORT);    // Added unit16_t

    // Bind the socket to localhost port 9002
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        // Accept incoming connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        // Fork a child process to handle client request
        if (fork() == 0)
        {
            close(server_fd); // Close server socket in child process

            // Determine which process should process the request
            char str_number[20]; // Assuming a maximum of 20 characters for the string representation of the number
            // Loadbalancer
            if ((count <= 3) || (count > 9 && (count % 3 == 1)))
            {
                // Request to be processed by server itself
                // Convert integer to string
                sprintf(str_number, "%d", PORT);
                // Add null terminator
                strcat(str_number, "\0");
                // Send the string over the socket
                write(client_socket, str_number, strlen(str_number));
                crequest(client_socket); // Handle client request in the server itself
            }
            else if ((count <= 6) || (count > 9 && (count % 3 == 2)))
            {
                // Request to be processed by mirror1
                // Convert integer to string
                sprintf(str_number, "%d", MIRROR1_PORT);
                // Add null terminator
                strcat(str_number, "\0");
                // Send the string over the socket
                write(client_socket, str_number, strlen(str_number));
            }
            else if ((count <= 9) || (count > 9 && count % 3 == 0))
            {
                // Request to be processed by mirror2
                // Convert integer to string
                sprintf(str_number, "%d", MIRROR2_PORT);
                // Add null terminator
                strcat(str_number, "\0");
                // Send the string over the socket
                write(client_socket, str_number, strlen(str_number));
            }

            printf("Close client connection\n");
            close(client_socket); // Close client socket in child process
            exit(0);              // Exit child process after handling request
        }
        else
        {
            // Increase the counter in the parent
            count++;
            close(client_socket); // Close socket in parent process
            // printf("Closed the Client socket in the parent\n");
        }
    }

    close(server_fd);
    return 0;
}
