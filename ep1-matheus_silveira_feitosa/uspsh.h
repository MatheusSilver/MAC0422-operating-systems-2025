#ifndef USP_SH_H
#define USP_SH_H

// Definindo os limites para o nome da máquina e diretórios
#define MACHINE_NAME_MAX 16
#define PATH_NAME_MAX 4096
#define MAX_TOKEN_QTD 20 // Improvável, mas vai que né?

// IFS padrão (isso define os delimitadores para a separação dos tokens)
#define IFS "\n\t " 

// Variáveis globais (externas)
extern char machineName[MACHINE_NAME_MAX];    // Nome da máquina
extern char currentDirectory[PATH_NAME_MAX];  // Diretório atual

// Funções
void get_hostname(char *hostname, int size);                   // Obtém o nome da máquina
void get_shell_prompt(const char *hostname, const char *currentDirectory, char *msgBuffer, int bufferSize); // Gera o prompt do shell
void get_current_directory(char *currentDirectory, int size); // Obtém o diretório atual
void get_user_command(char **inputBuffer, char *inputMSG);    // Captura o comando do usuário
int  extract_tokens_from_line(char *userInput, char *tokens[]); // Extrai os tokens de uma linha de comando
void command_handler(char *command, char *arguments[]);       // Gerencia a execução do comando

// Comandos internos do shell
void change_directory(const char *new_directory);            // Muda o diretório
void whoami();                                              // Exibe o nome do usuário atual
void change_permissions(const char *mode, const char *pathname); // Modifica permissões de arquivo
void close_uspsh();                                          // Fecha o shell
void print_error(char *commandName);                          // Exibe erro de comando não encontrado

// Execução de comandos externos
int execute_external_command(char *command, char *arguments[]); // Executa comandos externos

#endif // USP_SH_H
