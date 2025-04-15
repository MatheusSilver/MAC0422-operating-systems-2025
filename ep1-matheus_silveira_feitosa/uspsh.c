#include "uspsh.h"

char IFS[4] = "\n\t ";
char machineName[MACHINE_NAME_MAX];
char currentDirectory[PATH_NAME_MAX];

int main(){
    char *commandLineInput = NULL;
    char *commandTokens[MAX_TOKEN_QTD];
    int numTokens;
    
    /* Espaço para a mensagem de prompt + \0 */
    char promptMSG[PATH_NAME_MAX + MACHINE_NAME_MAX + 6]; 
    
    /* Setup inicial: obtém o nome da máquina e o diretório em que o shell iniciou a execução */
    get_hostname(machineName, sizeof(machineName));
    get_current_directory(currentDirectory, sizeof(currentDirectory));
    
    while (1){
        get_shell_prompt(machineName, currentDirectory, promptMSG);
        get_user_command(&commandLineInput, promptMSG);
        numTokens = extract_tokens_from_line(commandLineInput, commandTokens);
        
        /* Verificando uma entrada válida para processar e não só um Enter ou algo do tipo. */
        if (numTokens > 0) command_handler(commandTokens[0], commandTokens);
        
        free(commandLineInput);
    }
    return 0;
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> GERENCIADOR DE COMANDOS <<<<<<<<<<<<<<<<<<<<<<<<< */

void command_handler(char* command, char* arguments[]){
    if (strcmp(command, "cd") == 0) {
        change_directory(arguments[1]);
        get_current_directory(currentDirectory, PATH_NAME_MAX);
    } else if (strcmp(command, "whoami") == 0){
        whoami();
    } else if (strcmp(command, "chmod") == 0){
        change_permissions(arguments[1], arguments[2]);
    } else if (strcmp(command, "exit") == 0){
        close_uspsh();
    }else{
        execute_external_command(command, arguments);
    }
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> COMANDOS INTERNOS <<<<<<<<<<<<<<<<<<<<<<<<< */

void change_directory(const char *new_directory) { chdir(new_directory); }

void whoami(){
    struct passwd *pw = getpwuid(getuid());
    printf("%s\n", (pw->pw_name));
}
void change_permissions(const char* mode, const char* pathname){
    int permissions = strtol(mode, NULL, 8);
    chmod(pathname, permissions);
}

/* Função não obrigatória que veio da experimentação de criar shells dentro de shells */
void close_uspsh(){
    printf("logout\n");
    exit(0);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> EXECUÇÃO DE COMANDOS <<<<<<<<<<<<<<<<<<<<<<<<< */

void execute_external_command(char* command, char* arguments[]){
    int pid = fork(); 
    int status;

    if (pid == 0){
        /* Usa o caminho absoluto do comando */
        execve(command, arguments, 0);         
        /* Informa o usuário se algo deu errado (provavelmente ele digitou o comando errado) */
        print_error(command);
        /* Encerra o processo filho se ele não puder ser executado. */
        exit(-1);   
    }else{
        waitpid(pid, &status, 0);
    }
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

void get_hostname(char *hostname, int size) { gethostname(hostname, size); }

void get_current_directory(char *currentDirectory, int size) { getcwd(currentDirectory, size); }

void get_shell_prompt(const char *hostname, const char *currentDirectory, char* msgBuffer) {
    sprintf(msgBuffer, "[%s:%s]$ ", hostname, currentDirectory);
}

void get_user_command(char **inputBuffer, char *inputMSG) {
    *inputBuffer = readline(inputMSG);
    /* Garante que apenas comandos válidos sejam salvos no histórico */
    /* Evita inputBuffer NULL e começando com \0 */
    if (*inputBuffer && **inputBuffer) add_history(*inputBuffer);
}

int extract_tokens_from_line(char* userInput, char* tokens[]){
    int numTokens = 0;
    char *token = strtok(userInput, IFS);  
    while (token != NULL) {
        tokens[numTokens++] = token;
        token = strtok(NULL, IFS);
    }
    
    /* Garante que o último elemento da array de argumentos seja NULL ou \0 como pedido pelo exec */
    tokens[numTokens] = NULL;
    
    return numTokens;
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> FEEDBACK DE ERROS <<<<<<<<<<<<<<<<<<<<<<<<< */

void print_error(char* commandName) { printf("O comando %s não foi encontrado\n", commandName); }