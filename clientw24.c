#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 9002

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

char *mergeStrings(char **strings, int count)
{
    int totalLength = 0;
    for (int i = 0; i < count; i++)
    {
        totalLength += strlen(strings[i]);
    }

    // Add space for separators (spaces) between words
    totalLength += count - 1;

    // Allocate memory for the merged string
    char *mergedString = malloc(totalLength + 1); // Add 1 for null terminator
    if (mergedString == NULL)
    {
        return NULL; // Memory allocation failed
    }

    // Merge the strings
    mergedString[0] = '\0'; // Initialize the merged string with null terminator
    for (int i = 0; i < count; i++)
    {
        strcat(mergedString, strings[i]);
        if (i < count - 1)
        {
            strcat(mergedString, " "); // Add space between words
        }
    }

    return mergedString;
}

int receive_tar_file(int sock)
{
    char file_buffer[256];
    char file_size_buff[256];
    char is_found_file[256];
    FILE *file;
    ssize_t bytes_read;
    long file_size;
    int remain_data = 0;

    // Receiving file found or not
    read(sock, &is_found_file, sizeof(is_found_file));
    if (strncmp(is_found_file, "File found", strlen("File found")) != 0)
    {
        printf("%s\n", is_found_file);
        return 0;
    }

    // Receiving file found or not
    read(sock, &file_size_buff, sizeof(file_size_buff));
    file_size = atoi(file_size_buff);
    printf("File size %ld\n", file_size);

    // Open file for writing
    file = fopen("temp.tar.gz", "wb");
    if (file == NULL)
    {
        perror("file opening error");
        exit(EXIT_FAILURE);
    }

    remain_data = file_size;
    // Receive data and write to file (remain_data > 0) &&
    while ((remain_data > 0) && ((bytes_read = read(sock, &file_buffer, sizeof(file_buffer))) > 0))
    {
        remain_data -= bytes_read;
        fwrite(file_buffer, 1, bytes_read, file);
    }

    fclose(file);
    return 1;
}

int isValidInteger(const char *str)
{
    // Check if each character in the string is a digit
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (!isdigit(str[i]))
        {
            return 0; // Not an integer
        }
    }
    return 1; // All characters are digits, hence an integer
}

int validate_command(char **command_array, int count)
{
    if (strcmp(command_array[0], "dirlist") == 0)
    {
        if (count < 2)
        {
            printf("Enter the second argument for dirlist as -a or -t\n");
            return 0;
        }
        else if (count > 2)
        {
            printf("dirlist cannot have more than 2 arguments\n");
            return 0;
        }
        else if (!((strcmp(command_array[1], "-a") == 0) || (strcmp(command_array[1], "-t") == 0)))
        {
            printf("Enter the second argument for dirlist as -a or -t\n");
            return 0;
        }
    }
    else if (strcmp(command_array[0], "w24fn") == 0)
    {
        if (count < 2)
        {
            printf("Enter the second argument for w24fn as filename\n");
            return 0;
        }
        else if (count > 2)
        {
            printf("w24fn cannot have more than 2 arguments\n");
            return 0;
        }
    }
    else if (strcmp(command_array[0], "w24fz") == 0)
    {
        if (count < 3)
        {
            printf("Enter the second and third argument for w24fz as size1 and size2\n");
            return 0;
        }
        else if (count > 3)
        {
            printf("w24fz cannot have more than 3 arguments\n");
            return 0;
        }
        else if (!(isValidInteger(command_array[1]) && isValidInteger(command_array[2])))
        {
            printf("size1 and size2 must be integers\n");
            return 0;
        }
        else
        {
            // Convert size1 and size2 to integers
            int size1 = atoi(command_array[1]);
            int size2 = atoi(command_array[2]);
            // Check if size1 and size2 are non-negative
            if (size1 >= 0 && size2 >= 0)
            {
                // Check if size1 is less than or equal to size2
                if (!(size1 <= size2))
                {
                    printf("Size1 must be less than or equal to Size2\n");
                    return 0;
                }
            }
        }
    }
    else if (strcmp(command_array[0], "w24ft") == 0)
    {
        if (count < 2 || count > 4)
        {
            printf("w24ft must have 2 to 4 arguments\n");
            return 0;
        }
    }
    else if (strcmp(command_array[0], "w24fdb") == 0 || strcmp(command_array[0], "w24fda") == 0)
    {
        if (count < 2)
        {
            printf("Enter the second argument for %s as date\n", command_array[0]);
            return 0;
        }
        else if (count > 2)
        {
            printf("%s cannot have more than 2 arguments\n", command_array[0]);
            return 0;
        }
        else
        {
            struct tm timeinfo;
            if (strptime(command_array[1], "%Y-%m-%d", &timeinfo) == NULL)
            {
                printf("Date must be in the format YYYY-MM-DD\n");
                return 0; // Parsing failed
            }
        }
    }
    else if (strcmp(command_array[0], "quitc") == 0)
    {
        if (count > 1)
        {
            printf("%s cannot have more than 1 arguments\n", command_array[0]);
            return 0;
        }
    }
    else
    {
        printf("Invalid command\n");
        return 0;
    }
    return 1;
}

int connect_to_server(int port)
{
    int sock = 0;
    struct sockaddr_in serv_addr;

    // Create a socket | SOCK_STREAM type is a tcp
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    // Specify the address where we are going to connect
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Define localhost (INADDR_ANY : Any ip address on the local machine)
    serv_addr.sin_port = htons((uint16_t)port);    // htons will convert the port number to the proper format

    // Connect to the serverw24
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("\nConnection Failed \n");
        return -1;
    }

    return sock;
}

int main()
{
    int sock = connect_to_server(PORT);
    char command[1024] = {0};
    char server_response[1024];

    // Get the port number
    read(sock, &server_response, sizeof(server_response));
    // Convert the port number to integer
    int port_number = atoi(server_response);

    if (port_number != PORT)
    {
        close(sock);
        sock = connect_to_server(port_number);
    }

    while (1)
    {
        printf("clientw24$ ");
        // Read input from user
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = '\0'; // Remove the newline character

        // Check if the command is empty or contains only spaces
        int is_empty = 1;
        for (int i = 0; command[i] != '\0'; i++)
        {
            if (!isspace(command[i]))
            {
                is_empty = 0;
                break;
            }
        }

        if (is_empty)
        {
            continue;
        }
        // Tokenize the input string
        int count;
        char **token_arr = tokenizeString(command, &count);

        // Validate the command
        if (!validate_command(token_arr, count))
        {
            continue;
        }

        // Merge the tokenized strings to remove spaces
        char *processed_command = mergeStrings(token_arr, count);
        // Send command
        write(sock, processed_command, strlen(processed_command) + 1);
        if (strncmp(processed_command, "w24fz", strlen("w24fz")) == 0)
        {
            receive_tar_file(sock);
        }
        else if (strncmp(processed_command, "w24ft", strlen("w24ft")) == 0)
        {
            receive_tar_file(sock);
        }
        else if (strncmp(processed_command, "w24fdb", strlen("w24fdb")) == 0)
        {
            receive_tar_file(sock);
        }
        else if (strncmp(processed_command, "w24fda", strlen("w24fda")) == 0)
        {
            receive_tar_file(sock);
        }
        else if (strncmp(processed_command, "quitc", strlen("quitc")) == 0)
        {
            break;
        }
        else
        {
            // Handle server response
            read(sock, &server_response, sizeof(server_response));

            printf("%s\n", server_response);
        }
    }
    printf("Close server connection\n"); // Close the socket connection
    close(sock);
    return 0;
}
