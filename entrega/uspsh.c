#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>

#define MACHINE_NAME_MAX 16
#define PATH_NAME_MAX 255
#define USER_INPUT_MAX 255
#define TRUE 1
#define FALSE 0


void get_hostname(char *hostname, int size) {
    gethostname(hostname, size);
}

char* get_curdir() {
    return getenv("PWD");
}

void print_shell_prompt(const char *hostname) {
    printf("[%s:%s]$ ", hostname, get_curdir());
}

void change_directory(const char *new_directory){
    chdir(new_directory);
}

void get_user_command(char* input_buffer, int size){
    fgets(input_buffer, size, stdin);
    input_buffer[strcspn(input_buffer, "\n")] = '\0';
}

void print_whoami(){
    struct passwd *pw = getpwuid(getuid());
    printf("%s\n", pw->pw_name);
}

void change_mode(){}

int main(){
    char machineName[MACHINE_NAME_MAX];
    char userInput[USER_INPUT_MAX];

    //Setup inicial
    get_hostname(machineName, sizeof(machineName));

    while (TRUE){
        print_whoami();
        print_shell_prompt(machineName);
        get_user_command(userInput, sizeof(userInput));
    }
    
    
    exit(0);

}