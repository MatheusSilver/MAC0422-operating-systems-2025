#ifndef USPSH_H
#define USPSH_H

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEPENDÊNCIAS EXTERNAS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Bibliotecas padrão */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Usado para as syscalls */
#include <unistd.h>

/* Usado na captura de comandos e funcionalidade do histórico */
#include <readline/history.h>
#include <readline/readline.h>

/* Usado pelo comando builtin whoami para converter o ID do usuário no nome de usuário */
#include <pwd.h>

/* Usado pelo comando builtin chmod para alterar permissões de arquivos */
#include <sys/stat.h>

/* Usado pela função waitpid para esperar os processos filhos */
#include <sys/wait.h>

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEFINIÇÕES DE MACROS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Tamanho máximo para o nome da máquina */
/* Em Windows, o máximo é 16, mas no Linux aparentemente pode ser 64 (na prática 63 + \0) */
#define MACHINE_NAME_MAX 64

/* Tamanho máximo para o caminho do diretório atual */
/* Em Windows, o máximo é 256, mas no Linux é 4096 */
#define PATH_NAME_MAX 4096

/* Número máximo de argumentos que um dado comando (ou programa) pode receber */
#define MAX_TOKEN_QTD 20

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* IFS padrão do Bash é configurado como "\n\t " */
extern char IFS[4];

/* Variável que armazena o nome da máquina (hostname) */
extern char machineName[MACHINE_NAME_MAX];

/* Variável que armazena o diretório atual (PWD) */
extern char currentDirectory[PATH_NAME_MAX];

/* >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES GERAIS DE INTERAÇÃO DO SHELL <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Obtém o nome da máquina (hostname) e armazena na variável hostname.
 * 
 * @param hostname Buffer para armazenar o nome do host.
 * @param size Tamanho máximo do buffer.
 */
void get_hostname(char *hostname, int size);

/*!
 * @brief Obtém o diretório atual (PWD) e armazena na variável currentDirectory.
 * 
 * @param currentDirectory Buffer para armazenar o diretório atual.
 * @param size Tamanho máximo do buffer.
 */
void get_current_directory(char *currentDirectory, int size);

/*!
 * @brief Prepara a mensagem de prompt do shell utilizando o nome da máquina e o diretório atual.
 * 
 * @param hostname Nome da máquina.
 * @param currentDirectory Diretório atual.
 * @param msgBuffer Buffer para a mensagem do prompt.
 */
void get_shell_prompt(const char *hostname, const char *currentDirectory, char *msgBuffer);

/*!
 * @brief Lê o comando do usuário e, se for válido, adiciona-o ao histórico.
 * 
 * @param inputBuffer Ponteiro para o buffer de entrada.
 * @param inputMSG Mensagem do prompt a ser exibida.
 */
void get_user_command(char **inputBuffer, char *inputMSG);

/*!
 * @brief Separa os argumentos passados pela linha de comando usando os delimitadores definidos em IFS.
 * 
 * @param userInput Linha de comando digitada pelo usuário.
 * @param tokens Array de strings com os tokens extraídos.
 * @return Número inteiro com a quantidade de tokens extraídos.
 */
int extract_tokens_from_line(char *userInput, char *tokens[]);

/*!
 * @brief Interpreta o comando digitado, invocando a função específica de acordo com o comando.
 * 
 * @param command Primeiro token (comando) da linha de comando.
 * @param arguments Array com todos os tokens (argumentos do comando).
 */
void command_handler(char *command, char *arguments[]);




/* >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DOS COMANDOS INTERNOS E UTILITÁRIOS <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Altera o diretório atual para o especificado.
 * 
 * @param new_directory Caminho do novo diretório.
 */
void change_directory(const char *new_directory);

/*!
 * @brief Exibe o nome do usuário atual.
 */
void whoami();

/*!
 * @brief Altera as permissões de um arquivo.
 * 
 * @param mode Modo de permissão em formato octal (string).
 * @param pathname Caminho do arquivo.
 */
void change_permissions(const char *mode, const char *pathname);

/*!
 * @brief Executa um comando ou programa externo criando um novo processo e aguarda até ele terminar.
 * 
 * @param command Nome ou caminho do comando ou programa a ser executado.
 * @param arguments Array de argumentos para o comando ou programa.
 */
void execute_external_command(char *command, char *arguments[]);

/*!
 * @brief Encerra o shell, exibindo uma mensagem de logout.
 */
void close_uspsh();

/*!
 * @brief Exibe uma mensagem de erro quando um comando não é encontrado ou não pode ser executado.
 * 
 * @param commandName Nome do comando que gerou o erro.
 */
void print_error(char *commandName);

#endif /* USPSH_H */