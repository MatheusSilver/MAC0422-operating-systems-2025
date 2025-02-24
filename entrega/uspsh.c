#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MACHINE_NAME_MAX 16
#define PATH_NAME_MAX 255
#define USER_INPUT_MAX 255
#define TRUE 1
#define FALSE 0


void get_hostname(char *hostname, int size) {
    gethostname(hostname, size);
}

void print_shell_prompt(const char *hostname, const char *currentDirectory) {
    printf("[%s:%s]$ ", hostname, currentDirectory);
}

void get_current_directory(char *currentDirectory, int size) {
    getcwd(currentDirectory, size);
}

void change_directory(const char *new_directory, char *currentDirectory, int size){
    chdir(new_directory);    
    get_current_directory(currentDirectory, size);
}

void get_user_command(char* input_buffer, int size){
    fgets(input_buffer, size, stdin);
    input_buffer[strcspn(input_buffer, "\n")] = '\0';
}

int main(){
    char machineName[MACHINE_NAME_MAX];
    char currentDirectory[PATH_NAME_MAX];
    char userInput[USER_INPUT_MAX];

    //Setup inicial
    get_hostname(machineName, sizeof(machineName));
    get_current_directory(currentDirectory, sizeof(currentDirectory));

    while (TRUE){
        print_shell_prompt(machineName, currentDirectory);
        get_user_command(userInput, sizeof(userInput));
    }
    change_directory("/tmp", currentDirectory, sizeof(currentDirectory));

    print_shell_prompt(machineName, currentDirectory);
    
    
    exit(0);

}