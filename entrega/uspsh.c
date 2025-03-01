#include <stdio.h>
#include <stdlib.h>

//Usado para as syscalls
#include <unistd.h>

//Usado para separação de tokens e conversão do input para chmod
#include <string.h>

//Usado na captura de commandos e funcionalidade do histórico.
#include <readline/history.h>
#include <readline/readline.h>

// Usado pelo whoami (Converter ID em nome de usuário)
#include <pwd.h>

//Usado pelo chmod
#include <sys/stat.h>

#define MACHINE_NAME_MAX 16
#define PATH_NAME_MAX 1024
#define MAX_TOKEN_QTD 25 //Improvável, mas vai que né?

#define IFS "\n\t " //IFS padrão (talvez num shell de verdade isso seria uma variável...)


char machineName[MACHINE_NAME_MAX];
char currentDirectory[PATH_NAME_MAX];

void get_hostname(char *, int);
void get_shell_prompt(const char *, const char *, char *, int);
void get_current_directory(char *, int);
void get_user_command(char **, char *);
int extract_tokens_from_line(char *, char *[]);
void command_handler(char *, char *[]);


int main(){
    char *commandLineInput = NULL;
    char *commandTokens[MAX_TOKEN_QTD];
    char promptMSG[PATH_NAME_MAX + MACHINE_NAME_MAX + 6]; //Na pratica são 5, mas estamos contando o \0 no final.
    
    //Setup inicial
    get_hostname(machineName, sizeof(machineName));
    get_current_directory(currentDirectory, sizeof(currentDirectory));
    
    while (1){
        get_shell_prompt(machineName, currentDirectory, promptMSG, sizeof(promptMSG));
        get_user_command(&commandLineInput, promptMSG);
        int numTokens = extract_tokens_from_line(commandLineInput, commandTokens);
        if (numTokens > 0) { //Provavelmente o usuário só digitou Enter
            command_handler(commandTokens[0], commandTokens);
        }
        
        free(commandLineInput);
    }
    
    return 0;
    
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> GERENCIADOR DE COMANDOS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void change_directory(const char*);
void whoami();
void change_permissions(const char*, const char*);
int execute_external_command(char*, char*[]);
void print_error(char *);

void command_handler(char* command, char* arguments[]){
    if (strcmp(command, "cd") == 0) {
        change_directory(arguments[1]);
        get_current_directory(currentDirectory, PATH_NAME_MAX);
    } else if (strcmp(command, "whoami") == 0){
        whoami();
    } else if (strcmp(command, "chmod") == 0){
        change_permissions(arguments[1], arguments[2]);
    } else {
        int executionResult = execute_external_command(command, arguments);
        if (executionResult != 0) { print_error(command); }
    }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> COMANDOS INTERNOS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void change_directory(const char *new_directory) {
    chdir(new_directory);    
}

void whoami(){
    struct passwd *pw = getpwuid(getuid());
    printf("%s\n", (pw->pw_name));
}
void change_permissions(const char* pathname, const char* mode){
    int permissions = strtol(mode, NULL, 8);
    chmod(pathname, permissions);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> EXECUÇÃO DE COMANDOS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

int execute_external_command(char* command, char* arguments[]){
    //Aqui terá a implementação do fork exec e wait
    return 0;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void get_hostname(char *hostname, int size) {
    gethostname(hostname, size);
}

void get_shell_prompt(const char *hostname, const char *currentDirectory, char* msgBuffer, int bufferSize) {
    snprintf(msgBuffer, bufferSize, "[%s:%s]$ ", hostname, currentDirectory);
}

void get_current_directory(char *currentDirectory, int size) {
    getcwd(currentDirectory, size);
}

void get_user_command(char **inputBuffer, char *inputMSG) {
    *inputBuffer = readline(inputMSG);
    if (*inputBuffer && **inputBuffer) { //Impedir que um input vazio seja salvo no history
        add_history(*inputBuffer);
    }
}

int extract_tokens_from_line(char* userInput, char* tokens[]){
    int numTokens = 0;
    
    
    char *token = strtok(userInput, IFS);  
    while (token != NULL) {
        tokens[numTokens++] = token;
        token = strtok(NULL, IFS);
    }
    
    tokens[numTokens] = NULL;  // Garantir que o último token seja NULL, pra demarcar onde o exec deve parar.
    // Talvez isso devesse ser responsabilidade de outra função...
    
    return numTokens; //Se zero, alguma coisa deu errado e não foi possível capturar tokens.
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> FEEDBACK DE ERROS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void print_error(char* commandName) {
    printf("O comando %s não foi reconhecido\n", commandName);
}
