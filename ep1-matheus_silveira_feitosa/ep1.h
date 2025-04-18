#ifndef EP1_H
#define EP1_H


/* >>>>>>>>>>>>>>>>>>>>>>>>> DEPENDÊNCIAS EXTERNAS <<<<<<<<<<<<<<<<<<<<<<<<< */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Bibliotecas padrão */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Esse não era necessário, mas ajudou a organizar o código */
#include <stdbool.h>

/* Usado para a manipulação das threads */
#include <pthread.h>

/* Usado para a syscall sysconf(_SC_NPROCESSORS_ONLN) */
/* Que retorna o número de CPU's ativas na máquina */
#include <unistd.h>

/* Usada para o controle de tempo dos processos */
/* Já fica visualizável por padrão através de pthread.h */
/* indicamos apenas para evidenciar seu uso */
#include <time.h>

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEFINIÇÕES DE MACROS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Prioridade mínima e máxima padrão de sistemas Unix */
#define MAX_PRIORITY                    -20
#define MIN_PRIORITY                    19

/* Macros usadas para padronizar o uso dos mutexes como visto em aula */
#define P(mutex)                        pthread_mutex_lock(&(mutex))
#define V(mutex)                        pthread_mutex_unlock(&(mutex))

/* Simplificação da função min presente em bibliotecas como algorithm em C++ */
#define min(a, b)                       ((a) < (b) ? (a) : (b))

/* Número máximo de digítos usados para representar trocas de contexto */
#define MAX_CONTEXT_SWITCH_ORDER        5

/* Tamanho máximo para o nome de um processo simulado */
/* Tamanho máximo do nome do processo + \0 */
#define MAX_PROCESS_NAME                33


/* Tamanho máximo para o buffer de leitura de linhas */
/* Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45 */
/* Dobramos esta quantia pois não há restrições quanto a deadline e tempo de chegada */
#define MAX_LINE_SIZE                   90

/* Número máximo de processos simulados */
#define MAX_SIMULATED_PROCESSES         50

/* Separador de dados padrão das linhas do arquivo */
#define FILE_SEPARATOR                  " "


/* Quantuns mínimo e máximo de um processo no escalonamento por prioridade */
#define MIN_QUANTUM                     1
#define MAX_QUANTUM                     3

/* Número máximo de preempções até a promoção de um processo no escalonamento por prioridade */
#define MAX_PREEMPTIONS_UNTIL_PROMOTION 3

/* Modelos de escalonamento */
typedef enum {
    FIRST_COME_FIRST_SERVED = 1,
    SHORTEST_REMAINING_TIME_NEXT = 2,
    PRIORITY_SCHEDULING = 3
} SchedulerModel;

/* Status atual do processo */
typedef enum {
    READY,
    RUNNING,
    TERMINATED
} ProcessStatus;

/* Dados do processo simulado */
typedef struct {
    char name[MAX_PROCESS_NAME];    /* Nome do processo                                      */
    int arrival;                    /* Tempo de chegada do processo                          */
    int burstTime;                  /* Tempo de execução do processo                         */
    int deadline;                   /* Tempo limite desejável do processo                    */
    double executionTime;           /* Tempo de execução restante da rodada                  */
    double remainingTime;           /* Tempo de execução restante do processo                */
    double quantumTime;             /* Tempo de execução do processo na rodada               */
    int priority;                   /* Prioridade do processo (-20:19)                       */
    int numPreemptions;             /* Número de preempções do processo                      */
    ProcessStatus status;           /* Status atual do processo (pronto, rodando, terminado) */
} SimulatedProcessData;

/* Unidade controladora do processo */
typedef struct {
    /* Processo em execução */
    SimulatedProcessData *processData;
    
    /* CPU destinada ao processo */
    long cpuID;
    
    /* Flag que indica se a thread deve executar */
    bool paused;
    
    /* Controlador do acesso a flag de pausa */
    pthread_mutex_t pauseMutex;
    pthread_cond_t pauseCond;
    
    /* Thread do processo simulado */
    pthread_t threadID;
} SimulatedProcessUnit;

/* Gerente da fila de prontos e rodando */
typedef struct {
    /* Informações comuns da queue */
    int rear;
    int front;
    int length;
    int maxSize;
    
    /* Mutexes controladores do acesso as Queues */
    pthread_mutex_t readyQueueMutex;
    pthread_mutex_t runningQueueMutex;
    pthread_mutex_t schedulerMutex;
    pthread_cond_t cond;
    pthread_cond_t wakeUpScheduler;

    /* Fila de prontos e rodando */
    
    /* A fila de prontos é uma fila circular */
    SimulatedProcessUnit **readyQueue;
    
    /* A fila de rodando é um vetor com tamanho igual ao número de CPU's utilizáveis */
    SimulatedProcessUnit **runningQueue;
    
    /* Modelos de escalonamento */
    SchedulerModel scheduler;

    /* Flag que indica que não serão criados novos processos */
    bool allProcessesArrived;
} CoreQueueManager;

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Controlador de escrita no arquivo (impede que duas threads escrevam seu output ao mesmo tempo) */
extern pthread_mutex_t writeFileMutex;

/* Controlador de acesso ao controlador de preempções */
/* Evita o problema clássico de duas threads executando x++ */
extern pthread_mutex_t preemptionCounterMutex;

/* Array de Unidades de processo */
extern SimulatedProcessUnit **processUnits;

/* Dados extraídos do arquivo de trace */
extern SimulatedProcessData **processList;
extern int numProcesses;

/* Controlador de acesso a Queue */
extern CoreQueueManager systemQueue;

/* Variável READ-ONLY que marca o começo da simulação */
extern double INITIAL_SIMULATION_TIME;

/* Variável READ-ONLY que registra o número de CPU's disponíveis */
extern int AVAILABLE_CORES;

/* Variável registradora do número de trocas de contexto feita na simulação */
extern unsigned int contextSwitches;

/* Arquivo de saída acessado por todas as threads */
extern FILE *outputFile;

/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DO SIMULADOR <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Verifica se o número de argumentos passados
 * @note Se não for, imprime uma mensagem de erro e encerra o programa.
 * @param args Número de argumentos passados.
 * @param argv Array de argumentos passados.
 */
void check_startup(int args, char *argv[]);

/*!
 * @brief Lê os argumentos passados e faz a configuração inicial do simulador.
 * @param args Número de argumentos passados.
 * @param argv Array de argumentos passados.
 */
void init(char *argv[]);


/* >>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Inicializa o simulador de escalonamento com o modelo especificado.
 * @param model Modelo de escalonamento a ser utilizado.
 * @param traceFilename Nome do arquivo de entrada com os dados dos processos.
 * @param outputFilename Nome do arquivo de saída para os resultados da simulação.
 */
void init_scheduler_simulation(SchedulerModel model, const char *traceFilename, const char *outputFilename);

/*!
 * @brief Executa o algoritmo de escalonamento FCFS (First Come First Served).
 */
void FCFS();

/*!
 * @brief Executa o algoritmo de escalonamento SRTN (Shortest Remaining Time Next).
 */
void SRTN();

/*!
 * @brief Executa o algoritmo de Escalonamento por Prioridade.
 */
void PS();

/* >>>>>>>>>>>>>>>>>>>>>>>>>> CRIADOR DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Função utilizada numa thread a parte para criar processos simulados conforme sua chegada é sinalizada.
 */
void *process_creator();

/*!
 * @brief Inicializa uma unidade de processo simulada.
 * @param processData Dados do processo a serem inicializados.
 * @return Ponteiro para a unidade de processo inicializada.
 */
SimulatedProcessUnit *init_process_simulation_unit(SimulatedProcessData *processData);

/*!
 * @brief Sinaliza que todos os processos chegaram e não haverão mais novos processos criados.
 */
void finish_process_arrival();

/* >>>>>>>>>>>>>>>>>>>>>>>>> EXECUÇÃO DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Atribui a execução de uma thread de processo simulada.
 * @param processToExecute Ponteiro para a unidade de processo a ser executada.
 */
void *execute_process(void * processToExecute);

/*!
 * @brief Consome o tempo de execução de um processo simulado.
 * @param process Ponteiro para a unidade de processo a ser executada.
 */
void consume_execution_time(SimulatedProcessData *process);

/*!
 * @brief Verifica e rearranja o processo simulado no momento em que este tem sua execução finalizada ou interrompida.
 * @param currentUnit Ponteiro para a unidade de processo a ser verificada.
 */
void handle_process_termination_status(SimulatedProcessUnit *currentUnit);

/* >>>>>>>>>>>>>>>>>> UTILIDADES DE GERÊNCIA DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Congela temporariamente a execução de uma unidade até que ela seja despausada.
 * @param unit Ponteiro para a unidade de simulação pausada.
 */
void wait_unpause(SimulatedProcessUnit *unit);

/*!
 * @brief pausa a execução de um processo simulado.
 * @param unit Ponteiro para a unidade de processo a ser pausada.
 */
void pause_process(SimulatedProcessUnit *unit);

/*!
 * @brief retoma a execução de um processo simulado.
 * @param unit Ponteiro para a unidade de processo a ser retomada.
 */
void resume_process(SimulatedProcessUnit *unit);

/*!
 * @brief Organiza a entrada de um processo na fila de rodando.
 * @param unit Ponteiro para a unidade de processo a ser organizada.
 */
void dispatch_process(SimulatedProcessUnit *unit);

/*!
 * @brief Organiza a saida forçada de um processo da fila de rodando. 
 * @param unit Ponteiro para a unidade de processo a ser interrompida.
 */
void preempt_process(SimulatedProcessUnit *unit);

/*!
 * @brief Sinaliza que o processo foi finalizado e atualiza o status do processo.
 * @param process Ponteiro para a unidade de processo a ser finalizada.
 * @param finishTime Tempo em que o processo foi finalizado.
 * @note Também registra o seu status de finalização no arquivo de saída.
 */
void finish_process(SimulatedProcessData *process, double finishTime);

/*!
 * @brief Verifica se as condições de preempção do SRTN foram atendidas.
 * @return true se há processos a serem preemptados, false caso contrário.
 */
bool should_preempt_SRTN();

/*!
 * @brief Obtem o processo com maior tempo de execução na fila de rodando.
 * @param this Ponteiro para o gerenciador de filas.
 * @return Ponteiro para a unidade de processo com maior tempo de execução.
 * @note Retorna NULL se não houver processos na fila de rodando.
 */
SimulatedProcessUnit *get_longest_running_process(CoreQueueManager *this);

/*!
 * @brief Verifica se há algum processo prestes a terminar na fila de rodando.
 * @note Se houver, então ele irá segurar a thread até que tal processo termine.abort
 * @note Dizemos que um processo está prestes a terminar quando seu tempo restante for menor que 0.25 segundos.
 */
void wait_for_finish_iminence();

/*!
 * @brief Verifica se há núcleos disponíveis para alocar um processo.
 * @return true se houver núcleos disponíveis, false caso contrário.
 */
bool has_available_core();

/*!
 * @brief Obtem um ID de núcleo disponível para alocar um processo.
 * @return ID do núcleo disponível na fila de rodando.
 */
long get_available_core();

/*!
 * @brief Destina um núcleo específico para um processo simulado.
 * @param targetProcess ID da thread do processo a ser alocado.
 * @param chosenCPU ID do núcleo a ser alocado.
 */
void allocate_cpu(pthread_t targetProcess, long chosenCPU);

/*!
 * @brief Incrementa de maneira protegida, o contador de preempções de um processo simulado.
 */
void increment_context_switch_counter();

/*!
 * @brief Aguarda que todas as threads de simulação terminem.
 * @param simulationUnits Array de unidades de processo simuladas.
 * @param num_threads Número de threads que devem ser finalizadas.
 */
void join_simulation_threads(SimulatedProcessUnit **simulationUnits, int numThreads);

/* >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DO CORE MANAGER <<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Inicializa o gerenciador de filas de prontos e rodando.
 * @param this Ponteiro para o gerenciador de filas.
 * @param maxSize Tamanho máximo da fila de prontos.
 * @param model Modelo de escalonamento a ser utilizado.
 */
void init_core_manager(CoreQueueManager *this, int maxSize, SchedulerModel model);

/*!
 * @brief Verifica se a fila de prontos está vazia.
 * @param this Ponteiro para o gerenciador de filas.
 * @return true se a fila estiver vazia, false caso contrário.
 */
bool is_empty(CoreQueueManager *this);

/*!
 * @brief Adiciona uma unidade de processo à fila de prontos.
 * @param this Ponteiro para o gerenciador de filas.
 * @param processUnit Unidade de processo a ser adicionada na fila.
 */
void enqueue(CoreQueueManager *this, SimulatedProcessUnit *processUnit);

/*!
 * @brief Remove uma unidade de processo da fila de prontos.
 * @param this Ponteiro para o gerenciador de filas.
 * @return Unidade ponteiro do processo removido da fila.
 */
SimulatedProcessUnit *dequeue(CoreQueueManager *this);

/*!
 * @brief Retorna o elemento na frente da fila de prontos sem removê-lo.
 * @param this Ponteiro para o gerenciador de filas.
 * @return Unidade de processo na frente da fila.
 */
SimulatedProcessUnit *peek_queue_front(CoreQueueManager *this);

/*!
 * @brief Libera a memória alocada para o gerenciador de filas e seus mutexes.
 * @param this Ponteiro para o gerenciador de filas.
 */
void destroy_manager(CoreQueueManager *this);


/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO STRUCT <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Define o nome do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param name Nome a ser atribuído ao processo.
 */
void set_process_name(SimulatedProcessData *process, const char* name);

/*!
 * @brief Define o tempo de chegada do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param arrivalTime Tempo de chegada a ser atribuído ao processo.
 */
void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime);

/*!
 * @brief Atualiza a duração de execução do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param elapsedTime Tempo passado desde a última atualização.
 */
void update_process_duration(SimulatedProcessData * process, double elapsedTime);

/*!
 * @brief Reinicia o tempo de execução de um processo para o tempo total de execução.
 * @param process Ponteiro para a unidade de processo a ser reiniciada.
 * @note Usado quando o processo é preemptado e não finalizado para ser reoordenado corretamente na fila de prontos.
 */
void reset_execution_time(SimulatedProcessData *process);

/*!
 * @brief Define o tempo limite desejado do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param deadline Tempo limite desejado a ser atribuído ao processo.
 */
void set_process_deadline(SimulatedProcessData * process, const char * deadline);

/*!
 * @brief Define o tempo de execução do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param burstTime Tempo de execução a ser atribuído ao processo.
 * @note Também define o tempo restante e o tempo de execução total do processo que inicialmente são essecialmente iguais.
 */
void set_process_burst_time(SimulatedProcessData * process, const char * burstTime);

/*!
 * @brief Define o quantum de execução do processo simulado. 
 * @param process Ponteiro para o processo simulado.
 * @param quantumTime Tempo de execução a ser atribuído ao processo.
 * @note Por padrão é definido como o tempo total de execução, mas caso seja o escalonamento por prioridade,
 *       o quantumTime é definido dentro da própria thread gerenciadora.
 */
void set_quantum_time(SimulatedProcessData * process, double quantumTime);

/*!
 * @brief Define a prioridade do processo simulado.
 * @param process Ponteiro para o processo simulado.
 * @param priority Prioridade a ser atribuída ao processo.
 */
void set_process_priority(SimulatedProcessData * process, int priority);

/*!
 * @brief Atualiza a prioridade de um processo simulado, ou define a prioridade inicial.
 * @param process Ponteiro para a unidade de processo a ser atualizada.
 * @param isInitial Flag que indica se primeira vez que definimos a prioridade de um processo. 
 */
void update_priority(SimulatedProcessData *process, bool isInitial);

/*!
 * @brief obtem o quantum de um processo a partir da sua prioridade
 * @param priority prioridade do processo
 * @return quantum do processo
 */
int get_process_quantum(int priority);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Abre um arquivo com o modo especificado.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @param filepath Caminho do arquivo a ser aberto.
 * @param fileMode Modo de abertura do arquivo (ex: "r", "w", "a").
 * @return true se o arquivo foi aberto, false caso tenha dado erro.
 */
bool open_file(FILE **fileDescriptor, char *filepath, char *fileMode);

/*!
 * @brief Lê a próxima linha do arquivo e armazena no buffer.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @param lineBuffer Buffer onde a linha lida será armazenada.
 * @return true se uma linha foi lida, false caso tenha dado erro ou o arquivo tenha terminado.
 */
bool read_next_line(FILE *fileDescriptor, char *lineBuffer);

/*!
 * @brief Fecha o arquivo especificado.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @return true se o arquivo foi fechado, false caso tenha ocorrido um erro.
 */
bool close_file(FILE *fileDescriptor);

/*!
 * @brief Escreve uma linha no arquivo especificado.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @param lineBuffer Buffer contendo a linha a ser escrita.
 * @return true se a linha foi escrita, false caso tenha ocorrido um erro.
 */
bool write_next_line(FILE *fileDescriptor, char *lineBuffer);

/*!
 * @brief Lê os dados de um arquivo e armazena em uma estrutura de processo.
 * @param line Linha lida do arquivo.
 * @param process Ponteiro para a estrutura de processo onde os dados serão armazenados.
 * @return true se a linha foi lida corretamente, false caso contrário.
 */
bool parse_line_data(char *line, SimulatedProcessData *process);

/*!
 * @brief Aloca memória para o array de processos e lê os dados do arquivo.
 * @param fileDescriptor Ponteiro para o descritor de arquivo.
 * @param processList Array onde os processos serão armazenados.
 * @return Número de processos lidos do arquivo.
 */
int get_array_of_processes(FILE *fileDescriptor, SimulatedProcessData *processList[]);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Arredonda um valor double para o inteiro mais próximo.
 * @param value Valor a ser arredondado.
 * @return Valor arredondado.
 */
int customRound(double value);

/*!
 * @brief obtem o intervalo de prioridades a partir de MAX_PRIORITY e MIN_PRIORITY
 * @return intervalo de prioridades
 */
int get_priority_range();

/*!
 * @brief obtem o intervalo de quantuns a partir de MIN_QUANTUM e MAX_QUANTUM
 * @return intervalo de quantuns
 */
int get_quantum_range();

/*!
 * @brief Inicializa a variável INITIAL_SIMULATION_TIME com o tempo atual do sistema.
 */
void start_simulation_time();


/*!
 * @brief Escreve num arquivo de saida pré-definido, o número de trocas de contexto.
 */
void write_context_switches_quantity();

/*!
 * @brief Obtém o tempo atual do sistema.
 * @return Tempo atual em segundos desde a época (epoch).
 */
double get_current_time();

/*!
 * @brief Calcula o tempo passado desde um determinado ponto.
 * @param startTime Tempo inicial a partir do qual o tempo decorrido será calculado.
 * @return Tempo decorrido desde o tempo inicial.
 */
double get_elapsed_time_from(double startTime);

/*!
 * @brief Suspende a thread até que o tempo indicado seja atingido.
 * @param nextArrival Tempo em que a thread deve ser acordada.
 */
void suspend_until(int nextArrival);

/*!
 * @brief Ordena um array de processos simulados com base no tempo de chegada.
 * @note Utiliza o algoritmo MergeSort.
 * @param arr Array de processos a serem ordenados.
 * @param left Índice esquerdo do array.
 * @param right Índice direito do array.
 */
void sort(SimulatedProcessData **arr, int left, int right);

/* >>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DE LIMPEZA <<<<<<<<<<<<<<<<<<<<<<<< */

/*! 
 * @brief Libera memória e destrói mutexes 
 */
void clean_simulation_memory(SimulatedProcessUnit **processUnities, int numProcesses, CoreQueueManager *systemQueue);

/*! 
 * @brief Finaliza simulação e threads auxiliares 
 */
void finish_scheduler_simulation(pthread_t creatorThread);


#endif /* EP1_H */
