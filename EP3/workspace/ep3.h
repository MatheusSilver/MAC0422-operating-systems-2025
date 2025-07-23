#ifndef EP3_H
#define EP3_H

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEPENDÊNCIAS EXTERNAS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Bibliotecas padrão */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Não era necessário, mas ajudou a organizar o código */
#include <stdbool.h>

/* Usado única e exclusivamente pelas funções iddigit() e isspace */
/* Também já vem por padrão numa instalação normal do GCC. */
#include <ctype.h>

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEFINIÇÕES DE MACROS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Número máximo de carácteres de cada linha lida do arquivo de trace */
#define MAX_TRACE_LINE_SIZE   64

/* Número máximo de carácteres da string que indica o comando a ser executado no arquivo de trace */
#define MAX_COMMAND_SIZE      32

/* Número máximo de carácteres que cada linha do arquivo PGM de entrada terá */
#define MAX_PGM_LINE_SIZE     256

/* Definição auxiliar, indica que cada registro do arquivo PGM terá n caractéres mais um espaçador (Pode ser " " ou "\n") */
#define SPACE_OFFSET          1

/* Indica quantas linhas formam o cabeçalho do arquivo PGM */
/* Estamos considerando que o PGM sempre terá no início, algo similar ao exemplo abaixo:
    P2
    256 256
    255
*/
#define PGM_HEADER_SIZE       3

/* Número esperado de caractéres que representam uma posição de memória */
/* O programa espera ou "  0", ou "255" em cada registro de memória do PGM. */
#define MAX_PIXEL_DIGITS      3

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEFINIÇÕES DE TIPOS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Definição auxiliar que representa o tipo de dado da posição de memória, isto é, cada posição de memória é um valor entre 0 ou 255 que é o máximo de um unsigned char */
typedef unsigned char MemoryStatus;

/* Definição de status que representa célula ocupada */
/* Representado por 000 */
extern const MemoryStatus FULL;

/* Definição de status que representa célula livre */
/* Representado por 255 */
extern const MemoryStatus EMPTY;

/* Enum Auxiliar que representa o modelo de alocação escolhido pelo usuário na linha de comando. */
typedef enum {
    FIRST_FIT = 1,  
    NEXT_FIT  = 2,
    BEST_FIT  = 3,
    WORST_FIT = 4
} AllocatorModel;

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Várival global auxiliar que serve como a "memória" do next fit, aponta sempre para a última posição a qual o algoritmo finalizou sua escrita. */
extern int  nextPos;

/* Variável global READ-ONLY que indica quantas posições de memória ao total, o arquivo PGM passado de entrada possui. */
extern int  memoryPositions;

/* Variável global READ-ONLY auxiliar que indica se o arquivo PGM de entrada termina com uma quebra de linha ou se a última célula termina abruptamente. */
extern bool isLastCharEnter;

/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DA SIMULAÇÃO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Verifica se o número de argumentos passados e suas informações condizem com o pedido pelo programa.
 * @note Se não for, imprime uma mensagem de erro e encerra o programa.
 * @param args Número de argumentos passados.
 * @param argv Array de argumentos passados.
 */
void check_startup(int args, char* argv[]);

/*!
 * @brief Lê os argumentos passados e faz a configuração inicial do simulador.
 * @param argv Array de argumentos passados.
 */
void init(char* argv[]);

/* >>>>>>>>>>>>>>>>>>>>>>>>> LEITURA E PARSE DO ARQUIVO DE TRACE <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Lê uma linha do arquivo de trace e separa o número da linha e o comando.
 * @param readLine Linha lida do arquivo de trace.
 * @param lineNumber Número da linha lido.
 * @param command Comando lido do arquivo de trace.
 * @note A linha lida deve estar no formato "<número da linha> <comando>".
 */
void get_data_from_line(char* readLine, int* lineNumber, char* command);

/*!
 * @brief Verifica se o comando lido do arquivo de trace é o de compactação.
 * @param command Comando lido do arquivo de trace.
 * @return Retorna verdadeiro se o comando for "COMPACTAR", falso caso contrário.
*/
bool is_compression_command(char* command);

/*!
 * @brief Lê o comando de alocação do arquivo de trace e retorna a quantidade de unidades de alocação requisitadas.
 * @param command Comando lido do arquivo de trace.
 * @return Retorna um inteiro com a quantidade de unidades requisitadas para alocação.
*/
int  get_requested_allocation_units(char* command);

/*!
 * @brief Lê a próxima linha do arquivo de trace e salva no buffer.
 * @param traceFile Arquivo de trace que contém os pedidos de alocação e\ou compactação a serem feitos na memória simulada.
 * @param buffer Buffer onde será salvo o conteúdo lido.
 * @return caso tenha lido uma linha, retorna verdadeiro, caso contrário (em geral se o arquivo acabou), retorna falso.
*/
bool get_next_trace_line(FILE* traceFile, char* buffer);

/* >>>>>>>>>>>>>>>>>>>>>>>>> EXECUÇÃO E GERÊNCIA DE MEMÓRIA <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Tenta alocar uma quantidade específica de espaços na memória, usando algum dos modelos de alocação.
 * @param allocatorID Identifica qual o modelo de alocação que deverá ser utilizado @see AllocatorModel
 * @param memoryFile  Arquivo que contem as informações dos estados da memória.
 * @param requestedSpace Quantidade de espaços contíguos que serão alocados.
 * @return caso tenha conseguido alocar memória retorna verdadeiro, caso não haja espaço livre o suficiente, retorna falso.
 */
bool allocate_memory(AllocatorModel allocatorID, FILE* memoryFile, int requestedSpace);

/*!
 * @brief Imprimindo a linha do arquivo de trace e o espaço solicitado que não foi possível alocar na saida padrão.
 * @param lineNumber Linha do arquivo de trace onde ocorreu a falha.
 * @param requestedSpace Quantidade de espaço que foi pedida a alocação.
 */
void register_failure(int lineNumber, int requestedSpace);

/* >>>>>>>>>>>>>>>>>>>>>>>>> MANIPULAÇÃO DO ARQUIVO PGM <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Pula as linhas que configuram como a imagem do arquivo PGM será interpretada.
 * @param memoryFile Arquivo PGM que contém os dados a serem pulados.
 */
void skip_pgm_header(FILE* memoryFile);

/*!
 * @brief Salva o status da próxima posição de memória no buffer
 * @param memoryFile Arquivo que contém os status de cada posição de memória.
 * @param buffer ponteiro onde será salvo o status lido.
 * @return caso tenha algum conteudo lido retorna verdadeiro, caso o arquivo tenha acabado e não haja mais status a serem lidos, retorna falso.
 */
bool get_next_pgm_status(FILE* memoryFile, MemoryStatus* buffer);

/*!
 * @brief Reinicia o arquivo PGM de forma a fazer o seu apontador ir para a primeira posição de memória disponível.
 * @param memoryFile Arquivo PGM a ter seu apontador reiniciado.
 * @note Esta função utiliza @see skip_pgm_header e deve ser útilizada apenas com o arquivo pgm com 3 linhas de cabeçalho.
 */
void reset_pgm_reader(FILE* memoryFile);

/*!
 * @brief Escreve em posições subsequentes a partir de uma dada posição inicial da memória, se tal conjunto está vazio ou cheio @see MemoryStatus
 * @param memoryFile Ponteiro para um arquivo contendo as informações dos Status de cada posição de memória.
 * @param initialPos Ponteiro para a posição de onde devemos começar a escrever no arquivo
 * @note durante a execução a função registra e altera em qual posição terminou de escrever.
 * @param allocatedSpace Quantidade de espaços contíguos na memória que deverão ser escritos
 * @param value Valor que indica o que deve ser escrito em dadas posições de memória, em geral cheio ou vazio @see MemoryStatus
 */
void write_memory_block(FILE* memoryFile, int *initialPos, int allocatedSpace, MemoryStatus value);

/* >>>>>>>>>>>>>>>>>>>>>>>>> COMPACTAÇÃO DE MEMÓRIA <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Aloca todos as posições ocupadas na memória no começo deixando um único bloco livre no final na memória.
 * @param memoryFile Arquivo que contem as informações dos estados da memória.
 */
void compress_memory(FILE* memoryFile);

/* >>>>>>>>>>>>>>>>>>>>>>>>> SIMULAÇÃO PRINCIPAL <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Executa a simulação dos pedidos para cada linha do traceFile até que este tenha sido totalmente lido.
 * @param allocatorID Identifica qual o modelo de alocação que deverá ser utilizado @see AllocatorModel
 * @param memoryFile  Arquivo que contem as informações dos estados da memória.
 * @param traceFile   Arquivo que contém os pedidos de alocação e\ou compactação a serem feitos na memória simulada.
*/
void simulate(AllocatorModel allocatorID, FILE* memoryFile, FILE* traceFile);

/* >>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ALOCAÇÃO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Tenta alocar uma quantidade específica de espaços na memória, usando do algoritmo First Fit.
 * @param memoryFile     Arquivo contendo os dados das posições de memória simuladas.
 * @param requestedSpace quantidade de espaços contíguos que serão alocados.
 * @return verdadeiro se foi possível alocar o espaço pedido, falso caso contrario.
*/
bool first_fit(FILE* memoryFile, int requestedSpace);

/*!
 * @brief Tenta alocar uma quantidade específica de espaços na memória, usando do algoritmo Next Fit.
 * @param memoryFile     Arquivo contendo os dados das posições de memória simuladas.
 * @param requestedSpace quantidade de espaços contíguos que serão alocados.
 * @return verdadeiro se foi possível alocar o espaço pedido, falso caso contrario.
*/
bool next_fit (FILE* memoryFile, int requestedSpace);

/*!
 * @brief Tenta alocar uma quantidade específica de espaços na memória, usando do algoritmo Best Fit.
 * @param memoryFile     Arquivo contendo os dados das posições de memória simuladas.
 * @param requestedSpace quantidade de espaços contíguos que serão alocados.
 * @return verdadeiro se foi possível alocar o espaço pedido, falso caso contrario.
*/
bool best_fit (FILE* memoryFile, int requestedSpace);

/*!
 * @brief Tenta alocar uma quantidade específica de espaços na memória, usando do algoritmo Worst Fit.
 * @param memoryFile     Arquivo contendo os dados das posições de memória simuladas.
 * @param requestedSpace quantidade de espaços contíguos que serão alocados.
 * @return verdadeiro se foi possível alocar o espaço pedido, falso caso contrario.
*/
bool worst_fit(FILE* memoryFile, int requestedSpace);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILITÁRIOS DE ALOCAÇÃO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Verifica se uma dada posição de memória lida está vazia ou não.
 * @param status Posição de memória lida.
 * @return verdadeiro se o status é VAZIO falso caso contrário.
*/
bool isStatusEmpty(MemoryStatus status);

/*!
 * @brief Procura espaços contíguos na memória de um tamanho especificado a partir de uma dada posição inicial.
 * @param memoryFile    Arquivo contendo os dados dos status da memória.
 * @param startPos      Ponteiro para a posição inicial rastreada.
 * @param endPos        Ponteiro para uma posição final qualquer. (Não é um limitante, é apenas usado como referência)
 * @param requiredSpace Quantidade de blocos contiguos a serem procurados.
 * @param getFullSpace  booleano que verifica se devemos procurar em toda a memória, ou se paramos ao encontrar o primeiro espaço suficientemente grande.
 * @note startPos e endPos são passados como ponteiros para poderem ser alterados pela função e serem usados como referência por @see move_back_file_descriptor
 */
bool get_next_empty_block(FILE* memoryFile, int* startPos, int* endPos, int requiredSpace, bool getFullSpace);

/*!
 * @brief Move o apontador do caractére do arquivo de uma posição atual para uma posição objetivo.
 * @param memoryFile Arquivo cujo queremos alterar o apontador.
 * @param targetPos  Posição objetivo que queremos deixar o apontador.
 * @param currentPos Posição atual que o apontador do arquivo está.
*/
void move_back_file_descriptor(FILE* memoryFile, int targetPos, int currentPos);

/*!
 * @brief percorre o estado inicial da memória buscando verificar onde a última alocação foi finalizada
 * @param memoryFile Arquivo contendo os dados da memória.
 * @note Esta função modifica o parâmetro @see nextPos que é global uma única vez na inicialização do simulador ou após comprimir a memória.
*/
void init_next_fit_pointer(FILE* memoryFile);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILITÁRIOS DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Abre um arquivo com o modo especificado.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @param filepath Caminho do arquivo a ser aberto.
 * @param fileMode Modo de abertura do arquivo (ex: "r", "w", "a").
 * @return true se o arquivo foi aberto, false caso tenha dado erro.
 */
bool open_file(FILE** fileDescriptor, char* filepath, char* fileMode);

/*!
 * @brief Fecha o arquivo especificado.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @return true se o arquivo foi fechado, false caso tenha ocorrido um erro.
 */
bool close_file(FILE* fileDescriptor);

/*!
 * @brief Copia o conteúdo de um arquivo para outro
 * @param src Descritor do arquivo de origem (a ser copiado)
 * @param dest Descritor do arquivo destino (contendo o conteúdo copiado)
 */
void copy_content(FILE* src, FILE* dest);

/*!
 * @brief Verifica o último caractére de um arquivo
 * @param fileDescriptor Ponteiro para o descritor de arquivo a ser analisado.
 * @param charToVerify   Caractére a ser verificado.
 * @note   Certifique-se de passar o caractére a ser verificado com aspas simples.
 * @return true se o último caráctere do arquivo condiz com o pedido, false caso contrário.
 */
bool verify_last_char_on_file(FILE* fileDescriptor, char charToVerify);

/*!
 * @brief Percorre todo o arquivo PGM, e conta quantas posições de memória estão representadas por ele.
 * @param memoryFile Ponteiro para o descritor de arquivo que guarda o PGM sendo trabalhado.
 * @return número inteiro contendo o número de posições de memória lidas.
 */
int  get_memory_slots_quantity(FILE* memoryFile);

#endif /* EP3_H */