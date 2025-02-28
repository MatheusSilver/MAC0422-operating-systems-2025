#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#define MACHINE_NAME_MAX 16
#define PATH_NAME_MAX 1024


#define IFS "\n\t " //IFS padrão (talvez num shell de verdade isso seria uma variável...)


void get_hostname(char *hostname, int size) {
    gethostname(hostname, size);
}

void print_shell_prompt(const char *hostname, const char *currentDirectory) {
    printf("[%s:%s]$ ", hostname, currentDirectory);
}

void get_current_directory(char *currentDirectory, int size) {
    getcwd(currentDirectory, size);
}


char* get_user_command(){
    char* inputBuffer;
    size_t inputBufferSize;
    getline(&inputBuffer, &inputBufferSize, stdin);
    
    printf("%s", inputBuffer);
    return inputBuffer;
}

char** extract_tokens_from_line(char* userInput){
    char** tokens[];
    
    return tokens;
}

int main(){
    char machineName[MACHINE_NAME_MAX];
    char currentDirectory[PATH_NAME_MAX];
    
    //Setup inicial
    get_hostname(machineName, sizeof(machineName));
    get_current_directory(currentDirectory, sizeof(currentDirectory));
    
    while (1){
        print_shell_prompt(machineName, currentDirectory);
        get_user_command();
    }
    change_directory("/tmp", currentDirectory, sizeof(currentDirectory));
    
    print_shell_prompt(machineName, currentDirectory);
    
    
    exit(0);
    
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> COMANDOS INTERNOS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void change_directory(const char *new_directory, char *currentDirectory, int size){
    chdir(new_directory);    
    get_current_directory(currentDirectory, size);
}

void whoami(){
    struct passwd *pw = getpwuid(getuid());
    printf("%s\n", pw->pw_name);
}

/*
chmod espera um mode_t, mas estou assumindo que é responsabilidade do próprio comando tratar 
do formato que ele recebe seus dados, neste caso, como padrão, todos sendo uma string.
*/
void change_permissions(const char* pathname, const char* mode){
    int permissions = strtol(mode, NULL, 8);
    chmod(pathname, permissions);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

