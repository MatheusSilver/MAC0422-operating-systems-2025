#define _GNU_SOURCE //Nescessário para poder usar o tryjoin e também o set_affinity
                    //Deve ser sempre colocado no topo.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#define true  1
#define false 0

//Honestamente, acho que essa forma é bem mais legal do que usar um nome gigante.
#define P(mutex) pthread_mutex_lock(&(mutex))
#define V(mutex) pthread_mutex_unlock(&(mutex))

#define MAX_CONTEXT_SWITCH_ORDER 4 //No pior caso teremos no máximo 119 trocas de contexto
                                   //Então podemos definir o print com 3 digitos + \0
                                   //Exemplo disso, é o caso onde dois processos chegam 
                                   //no instante 0 e tem ambos burstTime de 60 segundos.
#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 1000
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50

#define FILE_SEPARATOR " "

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char name[MAX_PROCESS_NAME];
    int arrival;
    int burstTime;
    int deadline;
    int remainingTime; //Assumimos que o tempo "extra" do ponto flutuante vem da demora pra trocar
                       //contexto, escrever em arquivo, ou outras coisas extras que são feitas para cada processo.
} SimulatedProcessData;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int availableCores;
} CoreUnityController;
//Gambiarra adotada pois não pode usar semaphore.h
//A forma inicial que havia pensando era só inicializar um 
//sem com o número de availableCores e então, o P iria ser
//executado até que availableCores = 0 indicando que não
//tem mais cores a serem utilizados.

//Lembrar de organizar isso depois.
int  open_file(FILE **, char*, char *);
int  read_next_line(FILE *, char*);
int  close_file(FILE *);
int  parse_line_data(char *, SimulatedProcessData *);
void set_process_name(SimulatedProcessData *, const char *);
void set_process_arrival_time(SimulatedProcessData *, const char *);
void set_process_burst_time(SimulatedProcessData *, const char *);
void set_process_deadline(SimulatedProcessData *, const char *);
int  get_array_of_processes(FILE *, SimulatedProcessData*[]);
void mergeSort(SimulatedProcessData *[], int, int);
void FCFS(SimulatedProcessData *[], int);
double get_elapsed_time_from(double startTime);
double get_current_time();
void init_core_unity_controller(CoreUnityController *);
void join_threads(pthread_t *, int *, int, int);
int find_available_thread(int *, int);
void busy_wait_until(double);
int is_running(double, double);
void* simula_processo_FCFS(void *);
void finish_process(SimulatedProcessData *, double);
void write_context_switches_quantity();

double INITIAL_SIMULATION_TIME; //Variável global READ_ONLY
unsigned int contextSwitches = 0;
FILE *outputFile;

//Essas variáveis são definidas uma vez e usadas apenas para leitura no decorrer do código
int AVAILABLE_CORES;

int main(int args, char* argv[]){
    if (args != 3) {
        printf("Uso: %s <arquivo_trace> <arquivo_saida>\n", argv[0]);
        exit(-1);
    }

    AVAILABLE_CORES = get_nprocs();
    FILE *traceFile;
    SimulatedProcessData *processList[MAX_SIMULATED_PROCESSES];

    if (open_file(&traceFile, argv[1], "r")){exit(-1);}
    if (open_file(&outputFile, argv[2], "w")){exit(-1);}

    int numProcesses = get_array_of_processes(traceFile, processList);
    
    //mergeSort por tempos de chegada
    mergeSort(processList, 0, numProcesses-1);
    
    FCFS(processList, numProcesses);
    
    close_file(traceFile);
    close_file(outputFile);

    exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<<
*/

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void FCFS(SimulatedProcessData * processList[], int numProcesses){
    pthread_t * workThreads = malloc(AVAILABLE_CORES * sizeof(pthread_t));
    int *occupiedThreads   = calloc(AVAILABLE_CORES, sizeof(int));
    CoreUnityController threadController;
    init_core_unity_controller(&threadController);

    //Esse é o tempo 0 da Simulação a ser adotado como referência.
    INITIAL_SIMULATION_TIME = get_current_time();
    //Vai falar qual processo é o próximo a rodar na lista de processos.
    int nextProcessTracker = 0;
    
    while (nextProcessTracker < numProcesses){
        join_threads(workThreads, occupiedThreads, AVAILABLE_CORES, false);
        //Idealmente fariamos uma fila, mas como esse caso é o mais simples
        //Não precisamos criar uma estrutura a parte só pra isso.
        SimulatedProcessData *nextProcess = processList[nextProcessTracker];
        //Até dá pra travar ele aqui já que, em tese, não tem nenhum outro processo pra rodar no momento
        //Se o próximo da fila não estiver pronto, certamente não vai ser o cara depois dele que vai estar.
        while (nextProcess->arrival > get_elapsed_time_from(INITIAL_SIMULATION_TIME));
        //Espera que haja pelo menos uma thread livre pra poder executar

        P(threadController.mutex);
        while (threadController.availableCores <= 0) {
            pthread_cond_wait(&threadController.cond, &threadController.mutex);
        }
        threadController.availableCores--;
        V(threadController.mutex);

        int threadSlot = find_available_thread(occupiedThreads, AVAILABLE_CORES);
        if (threadSlot != -1){
            void **args = malloc(2 * sizeof(void*));
            args[0] = &threadController;
            args[1] = nextProcess;
            pthread_create(&workThreads[threadSlot], NULL, simula_processo_FCFS, args);
            occupiedThreads[threadSlot] = 1;
            nextProcessTracker++;
        }   
    }
    
    join_threads(workThreads, occupiedThreads, AVAILABLE_CORES, true);
    free(workThreads);
    free(occupiedThreads);

    write_context_switches_quantity();
}

void* simula_processo_FCFS(void *args) {
    CoreUnityController *threadController = ((void**)args)[0];
    SimulatedProcessData *currentProcess = ((void**)args)[1];
    
    double endTime = currentProcess->burstTime;
    busy_wait_until(endTime);
    endTime = get_current_time() - INITIAL_SIMULATION_TIME;
    finish_process(currentProcess, endTime);

    P(threadController->mutex);
    threadController->availableCores++;
    pthread_cond_signal(&threadController->cond);
    V(threadController->mutex);
    free(args);
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/



/*
>>>>>>>>>>>>>>>>>>>>>>>> Escalonamento por Prioridade <<<<<<<<<<<<<<<<<<<<<<<<<
*/

int find_available_thread(int *occupiedThreads, int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        if (occupiedThreads[i] == 0) {
            return i;
        }
    }
    return -1; // Nenhuma thread disponível
               // Se de alguma forma chegar nisso, 
               // É porque algo deu MUITO errado com os semáforos.
}

void join_threads(pthread_t *workThreads, int *occupiedThreads, int num_threads, int force) {
    for (int t = 0; t < num_threads; t++) {
        if (occupiedThreads[t]) {
            if (force) {
                pthread_join(workThreads[t], NULL);
                occupiedThreads[t] = 0;
            } else {
                if (pthread_tryjoin_np(workThreads[t], NULL) == 0) {
                    occupiedThreads[t] = 0;
                }
            }
        }
    }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÃO DE ESPERA OCUPADA <<<<<<<<<<<<<<<<<<<<<<<<<
Realiza a operação muito dificil de multiplicar 0*0.
*/

void busy_wait_until(double endTime) { 
    int importantCalculation = 0;
    double startTime = get_current_time();
    while (is_running(startTime, endTime)){ importantCalculation *=0; }
    return;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<<
*/


//Abre um arquivo e retorna o seu descritor.
int open_file(FILE **fileDescriptor, char *filepath, char *fileMode) {
    *fileDescriptor = fopen(filepath, fileMode);
    return (*fileDescriptor == NULL) ? -1 : 0;
}

//Lê a próxima linha do arquivo e retorna 0 se o arquivo tiver acabado
//Ou retorna 1 caso ele tenha lido uma linha.
int read_next_line(FILE * fileDescriptor, char* lineBuffer){ return fgets(lineBuffer, MAX_LINE_SIZE, fileDescriptor) == NULL ? 0 : 1; }

//Escreve uma linha em um arquivo de interesse e adiciona uma quebra no final.
int write_next_line(FILE *fileDescriptor, char *lineBuffer) { return (fprintf(fileDescriptor, "%s\n", lineBuffer) < 0) ? -1 : 0; }

//Fecha o arquivo, com um dado descritor.
int close_file(FILE *fileDescriptor) { return fclose(fileDescriptor); }

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO STRUCT <<<<<<<<<<<<<<<<<<<<<<<<<

Vulgo tentativa de lembrar o POO...
*/

void set_process_name(SimulatedProcessData *process, const char* name){
    strcpy(process->name, name);
}

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){
    process->arrival = atof(arrivalTime);
}

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime = atof(burstTime);
    process->remainingTime = atof(burstTime);
}

void set_process_deadline(SimulatedProcessData * process, const char * deadline){
    process->deadline = atof(deadline);
}

void init_core_unity_controller(CoreUnityController *controller) {
    pthread_mutex_init(&controller->mutex, NULL);
    pthread_cond_init(&controller->cond, NULL);
    controller->availableCores = AVAILABLE_CORES;
}


/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

int parse_line_data(char * line, SimulatedProcessData * process){
    int currentParsedData = 0;

    void (*set_process_values[])(SimulatedProcessData *, const char *) = {
        set_process_name,
        set_process_arrival_time,
        set_process_burst_time,
        set_process_deadline
    };
    char * parsed_data = strtok(line, FILE_SEPARATOR);  
    while (parsed_data != NULL) {
        if (currentParsedData < 4) {
            set_process_values[currentParsedData](process, parsed_data);
        }
        parsed_data = strtok(NULL, FILE_SEPARATOR);
        currentParsedData++;
    }

    return currentParsedData != 4; //Se diferente de 4, então alguma coisa deu bem errado.
}

int get_array_of_processes(FILE * fileDescriptor, SimulatedProcessData * processList[]){
    char lineBuffer[MAX_LINE_SIZE];
    int appendedProcesses = 0;

    while (read_next_line(fileDescriptor, lineBuffer)) {
        SimulatedProcessData *process = (SimulatedProcessData *)malloc(sizeof(SimulatedProcessData));
        
        if (parse_line_data(lineBuffer, process)) {
            printf("Há um erro na linha %d do arquivo de trace\n", (appendedProcesses+1));
            exit(-1);
        }

        processList[appendedProcesses++] = process;
    }

    return appendedProcesses;
}

double get_current_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}


double get_elapsed_time_from(double startTime) { return get_current_time() - startTime; }

int is_running(double startTime, double endTime)  { return get_elapsed_time_from(startTime) < endTime; } 

void finish_process(SimulatedProcessData * process, double finishTime){
    char outputLine[MAX_LINE_SIZE];

    //Quase 100% das vezes, arredonda pra baixo, mas só pra garantir né.
    int roundedFinishTime = (int)(finishTime + 0.5);
    
    int deadlineMet = process->deadline >= roundedFinishTime ? true : false;
    int clockTime = roundedFinishTime - process->arrival;
    snprintf(outputLine, MAX_LINE_SIZE, "%s %d %d %d", process->name, clockTime, roundedFinishTime, deadlineMet);
    
    //Só pra garantir que nenhum par de threads tente escrever no arquivo ao mesmo tempo.
    P(writeFileMutex);
    write_next_line(outputFile, outputLine);
    V(writeFileMutex);
}

void write_context_switches_quantity(){
    char outputLine[MAX_CONTEXT_SWITCH_ORDER];
    snprintf(outputLine, MAX_CONTEXT_SWITCH_ORDER, "%d", contextSwitches);

    //Não precisa de mutex porque esse cara só roda depois de todas as threads terem sido terminadas.
    write_next_line(outputFile, outputLine);
}




//O objetivo é ordenar o arquivo por ordem de chegada, e assim, o FCFS
//Já fica praticamente pronto, e também facilita a verificação dos demais também.

//Usa-se o MergeSort pra manter a estabilidade nos casos de dois processos terem o mesmo 
//Arrival time, mas um ter sido colocado primeiro que o outro no arquivo trace

void merge(SimulatedProcessData* arr[], int left, int mid, int right) {
    int leftNum = mid - left + 1;
    int rightNum = right - mid;

    SimulatedProcessData** leftArr = (SimulatedProcessData**)malloc(leftNum * sizeof(SimulatedProcessData*));
    SimulatedProcessData** rightArr = (SimulatedProcessData**)malloc(rightNum * sizeof(SimulatedProcessData*));

    for (int i = 0; i < leftNum; i++) {
        leftArr[i] = arr[left + i];
    }
    for (int j = 0; j < rightNum; j++) {
        rightArr[j] = arr[mid + 1 + j];
    }

    int i = 0, j = 0, k = left;
    while (i < leftNum && j < rightNum) {
        if (leftArr[i]->arrival <= rightArr[j]->arrival) {
            arr[k] = leftArr[i++];
        } else {
            arr[k] = rightArr[j++];
        }
        k++;
    }

    while (i < leftNum) {
        arr[k++] = leftArr[i++];
    }

    while (j < rightNum) {
        arr[k++] = rightArr[j++];
    }

    // A única diferença disso pra versão em Java é que o c não tem coletor de lixo.
    // As vezes isso é bom, mas as vezes é um saco...
    free(leftArr);
    free(rightArr);
}

void mergeSort(SimulatedProcessData* arr[], int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(arr, left, mid);          
        mergeSort(arr, mid + 1, right);
        merge(arr, left, mid, right);
    }
}

    