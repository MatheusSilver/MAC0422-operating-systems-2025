#define _GNU_SOURCE //Nescessário para poder usar o tryjoin e também o set_affinity
                    //Deve ser sempre colocado no topo.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/sysinfo.h>

//Não sei se pode usar <stdbool.h>
//Então estarei usando a gambiarra do Ernesto mesmo.
typedef int bool;
#define true  1
#define false 0
#define MAX_PRIORITY -20
#define MIN_PRIORITY 19

//Honestamente, acho que essa forma é bem mais legal do que usar um nome gigante.
#define P(mutex) pthread_mutex_lock(&(mutex))
#define V(mutex) pthread_mutex_unlock(&(mutex))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define MAX_CONTEXT_SWITCH_ORDER 4 //No pior caso teremos no máximo 119 trocas de contexto
                                   //Então podemos definir o print com 3 digitos + \0
                                   //Exemplo disso, é o caso onde dois processos chegam 
                                   //no instante 0 e tem ambos burstTime de 60 segundos.
#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 1000
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50

//Importante para definir os intervalos de checagem para SRTN e de rotação da prioridade.
#define MIN_QUANTUM 1
#define MAX_QUANTUM 5

#define FILE_SEPARATOR " "

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preemptionCounterMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t syncMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t syncCond = PTHREAD_COND_INITIALIZER;
bool isSwitchingContext = false;

//Acho que estou inventando muita moda...
//https://stackoverflow.com/questions/9410/how-do-you-pass-a-function-as-a-parameter-in-c
typedef void* (*SchedulerSimulationUnit)(void*); 
typedef enum {
    FIRST_COME_FIRST_SERVED = 1,
    SHORTEST_REMAINING_TIME_NEXT = 2,
    PRIORITY_SCHEDULING = 3
} SchedulerModel;

typedef struct {
    char name[MAX_PROCESS_NAME];
    int arrival;
    int burstTime;
    int deadline;
    int executionTime;
    int remainingTime;
    int priority;
} SimulatedProcessData;

typedef struct {
    int head;       
    int tail;        
    int length;      
    int maxSize;   
    int finished;
    SchedulerModel scheduler;
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t cond_running;
    
    bool *idleCores;
    SimulatedProcessData **readyQueue;
    SimulatedProcessData **runningQueue;
} CoreQueueManager;

//Lembrar de organizar isso depois.
int  open_file(FILE **, char*, char *);
int  read_next_line(FILE *, char*);
int  close_file(FILE *);
int  parse_line_data(char *, SimulatedProcessData *);
void set_process_priority(SimulatedProcessData *, const char*);
void set_process_name(SimulatedProcessData *, const char *);
void set_process_arrival_time(SimulatedProcessData *, const char *);
void set_process_burst_time(SimulatedProcessData *, const char *);
void set_process_deadline(SimulatedProcessData *, const char *);
int  get_array_of_processes(FILE *, SimulatedProcessData*[]);
void mergeSort(SimulatedProcessData *[], int, int);
void FCFS(SimulatedProcessData *[], int);
double get_elapsed_time_from(double startTime);
double get_current_time();
void join_threads(pthread_t *, int);
void busy_wait_until(double);
int is_running(double, double);
void* thread_unit_FCFS(void *);
void finish_process(SimulatedProcessData *, double);
void write_context_switches_quantity();
void allocate_cpu(long);
void init_core_manager(CoreQueueManager *, int, SchedulerModel);
int is_empty(CoreQueueManager *);
void destroy_manager(CoreQueueManager *);
void enqueue(CoreQueueManager *, SimulatedProcessData *);
void SRTN(SimulatedProcessData *[], int);
void* thread_unit_SRTN(void *);
SimulatedProcessData *dequeue(CoreQueueManager *);
void reset_execution_time(SimulatedProcessData *);
void update_process_duration(SimulatedProcessData *, int);
void sleep_until(int);
void update_idle_cores(CoreQueueManager *);
void *thread_unit_priority_scheduling(void *);
void priority_scheduling(SimulatedProcessData *[], int);
void init_scheduler_simulation(SchedulerModel, FILE *);
void handle_process_termination_status(CoreQueueManager *, SimulatedProcessData *);
void setup_thread(void *, CoreQueueManager **, long *);
pthread_t* initialize_simulation_units(CoreQueueManager *, int, SchedulerSimulationUnit);
int get_process_quantum(int);


double INITIAL_SIMULATION_TIME; //Variável global READ_ONLY
unsigned int contextSwitches = 0;
FILE *outputFile;

//Essas variáveis são definidas uma vez e usadas apenas para leitura no decorrer do código
int AVAILABLE_CORES;

int main(int args, char* argv[]){
    if (args != 4) {
        printf("Uso: %s <id_escalonador> <arquivo_trace> <arquivo_saida>\n", argv[0]);
        exit(-1);
    }

    int schedulerID = atoi(argv[1]);
    if (schedulerID < 1 || schedulerID > 3) {
        printf("Escolha um escalonador válido:\n");
        printf("\t1 = FIRST_COME_FIRST_SERVED\n");
        printf("\t2 = SHORTEST_REMAINING_TIME_NEXT\n");
        printf("\t3 = Escalonamento por Prioridade\n");
        exit(-1);
    }

    SchedulerModel model = (SchedulerModel)schedulerID;

    AVAILABLE_CORES = get_nprocs();
    
    FILE *traceFile;
    if (open_file(&traceFile, argv[2], "r"))  exit(-1);
    if (open_file(&outputFile, argv[3], "w")) exit(-1);

    init_scheduler_simulation(model, traceFile);
    
    close_file(traceFile);
    close_file(outputFile);

    exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void init_scheduler_simulation(SchedulerModel model, FILE * traceFile){
    SimulatedProcessData *processList[MAX_SIMULATED_PROCESSES];
    int numProcesses = get_array_of_processes(traceFile, processList);
    mergeSort(processList, 0, numProcesses-1);
    switch (model) {
        case FIRST_COME_FIRST_SERVED:
            FCFS(processList, numProcesses); break;
        case SHORTEST_REMAINING_TIME_NEXT:
            SRTN(processList, numProcesses); break;
        case PRIORITY_SCHEDULING:
            priority_scheduling(processList, numProcesses); break;
        default:
            printf("Modelo não encontrado\n"); exit(-1);
    }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

//Denominamos esse cara como o produtor
//Ele irá verificar todos os processos que chegaram em dado instante
//Adicionar eles na fila, depois ser suspenso pra dar 100% do lugar
//Para as outras threads consumidoras.
void FCFS(SimulatedProcessData * processList[], int numProcesses){
    CoreQueueManager systemQueue;
    init_core_manager(&systemQueue, numProcesses, FIRST_COME_FIRST_SERVED);
    
    pthread_t *simulationUnits = initialize_simulation_units(&systemQueue, AVAILABLE_CORES, thread_unit_FCFS);
    
    INITIAL_SIMULATION_TIME = get_current_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) sleep_until(nextProcess->arrival);
        
        //Não é necessário se preocupar com sincronização aqui pois o próprio sistema de dequeue já irá cuidar disso.
        //Aqui só teremos problema se a queue estiver vazia, se for este o caso, o enqueue libera o dequeue
        //Se a queue não estiver vazia, então o dequeue não irá impedir que seja retirado um processo já existente.
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival){
            enqueue(&systemQueue, processList[i++]);
        }
    }
    
    P(systemQueue.mutex);
    systemQueue.finished = 1;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);

    join_threads(simulationUnits, AVAILABLE_CORES);

    free(simulationUnits);
    write_context_switches_quantity();
    destroy_manager(&systemQueue);
}

//Esses são os consumidores
void* thread_unit_FCFS(void *args) {
    CoreQueueManager *systemQueue; long chosenCPU;
    setup_thread(args, &systemQueue, &chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(systemQueue);
        if (currentProcess == NULL) break;
        double endTime = currentProcess->burstTime;
        busy_wait_until(endTime);
        update_process_duration(currentProcess, endTime);
        handle_process_termination_status(systemQueue, currentProcess);
    }
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void SRTN(SimulatedProcessData * processList[], int numProcesses) {
    CoreQueueManager systemQueue;
    init_core_manager(&systemQueue, numProcesses, SHORTEST_REMAINING_TIME_NEXT);    
    
    pthread_t *simulationUnits = initialize_simulation_units(&systemQueue, AVAILABLE_CORES, thread_unit_SRTN);
    
    INITIAL_SIMULATION_TIME = get_current_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) sleep_until(nextProcess->arrival);
        
        P(syncMutex);
        isSwitchingContext = true;
        V(syncMutex);
        
        update_idle_cores(&systemQueue);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival){
            enqueue(&systemQueue, processList[i++]);
        }

        P(syncMutex);
        isSwitchingContext = false;
        pthread_cond_broadcast(&syncCond);
        V(syncMutex);
    }
    
    P(systemQueue.mutex);
    systemQueue.finished = 1;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);
    
    join_threads(simulationUnits, AVAILABLE_CORES);
    
    free(simulationUnits);
    write_context_switches_quantity();
    destroy_manager(&systemQueue);
}

void* thread_unit_SRTN(void *args) {
    CoreQueueManager *systemQueue; long chosenCPU;
    setup_thread(args, &systemQueue, &chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(systemQueue);
        if (currentProcess == NULL) break;
        systemQueue->runningQueue[chosenCPU] = currentProcess;
        while (currentProcess->remainingTime > 0) {

            P(syncMutex);
            while (isSwitchingContext) pthread_cond_wait(&syncCond, &syncMutex);
            V(syncMutex);
            if (systemQueue->runningQueue[chosenCPU] == NULL) break;

            double endTime = min(MIN_QUANTUM, currentProcess->remainingTime);
            busy_wait_until(endTime);
            update_process_duration(currentProcess, endTime);
        }

        handle_process_termination_status(systemQueue, currentProcess);
    }
    free(args);
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>> Escalonamento por Prioridade <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void priority_scheduling(SimulatedProcessData * processList[], int numProcesses) {
    CoreQueueManager systemQueue;
    init_core_manager(&systemQueue, numProcesses, PRIORITY_SCHEDULING);
    
    pthread_t *simulationUnits = initialize_simulation_units(&systemQueue, AVAILABLE_CORES, thread_unit_priority_scheduling);
    
    INITIAL_SIMULATION_TIME = get_current_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) sleep_until(nextProcess->arrival);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival){
            enqueue(&systemQueue, processList[i++]);
        }
    }
    
    P(systemQueue.mutex);
    systemQueue.finished = 1;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);
    
    join_threads(simulationUnits, AVAILABLE_CORES);
    
    free(simulationUnits);
    write_context_switches_quantity();
    destroy_manager(&systemQueue);
}

void *thread_unit_priority_scheduling(void * args){
    CoreQueueManager *systemQueue = ((void**)args)[0];
    long chosenCPU = (long)((void**)args)[1];
    allocate_cpu(chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(systemQueue);
        if (currentProcess == NULL) break;
        systemQueue->runningQueue[chosenCPU] = currentProcess;
        double endTime = min(currentProcess->remainingTime, get_process_quantum(currentProcess->priority));
        busy_wait_until(endTime);
        update_process_duration(currentProcess, endTime);
        handle_process_termination_status(systemQueue, currentProcess);
    }
    free(args);
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>> Funções Comuns dos escalonadores <<<<<<<<<<<<<<<<<<<<<<<
*/

pthread_t* initialize_simulation_units(CoreQueueManager *systemQueue, int numThreads, SchedulerSimulationUnit functionModel) {
    pthread_t *simulationUnits = malloc(numThreads * sizeof(pthread_t));    
    for (long i = 0; i < numThreads; i++) {
        void **args = malloc(2 * sizeof(void *));
        args[0] = systemQueue;
        args[1] = (void *) i;
        pthread_create(&simulationUnits[i], NULL, functionModel, args);
    }
    
    return simulationUnits;
}

void allocate_cpu(long chosenCPU) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void join_threads(pthread_t *simulationUnits, int num_threads) {
    for (int t = 0; t < num_threads; t++) {
        pthread_join(simulationUnits[t], NULL);
    }
}

int get_process_quantum(int priority){
    const int PRIORITY_RANGE = MIN_PRIORITY - MAX_PRIORITY;
    const int QUANTUM_RANGE = MAX_QUANTUM - MIN_QUANTUM;
    double angularCoefficient = (double)(priority - MAX_PRIORITY) / PRIORITY_RANGE;
    //Agora entendi porque falam que SO é de 5º período,
    //Se eu não estivesse adiantado em LabNum teria perdido um bom tempo aqui.
    return MIN_QUANTUM + (int)(angularCoefficient * QUANTUM_RANGE);
}

void handle_process_termination_status(CoreQueueManager *systemQueue, SimulatedProcessData *currentProcess) {
    if (currentProcess->remainingTime <= 0) {
        double endTime = get_current_time() - INITIAL_SIMULATION_TIME;
        finish_process(currentProcess, endTime);
    } else {
        P(preemptionCounterMutex);
        contextSwitches++;
        V(preemptionCounterMutex);
        reset_execution_time(currentProcess);
        enqueue(systemQueue, currentProcess);
    }
}

void setup_thread(void *args, CoreQueueManager **systemQueue, long *chosenCPU) {
    *systemQueue = ((void**)args)[0];
    *chosenCPU = (long)((void**)args)[1];
    allocate_cpu(*chosenCPU);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÃO DE ESPERA OCUPADA <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void busy_wait_until(double endTime) { 
    int importantCalculation = 42;
    //Essa resposta é importante!!!
    double startTime = get_current_time();
    while (is_running(startTime, endTime)){ importantCalculation *=1; }
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

void set_process_priority(SimulatedProcessData *process, const char* priority){ process->priority = atoi(priority); }

void set_process_name(SimulatedProcessData *process, const char* name){ strcpy(process->name, name); }

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){ process->arrival = atof(arrivalTime); }

void update_process_duration(SimulatedProcessData * process, int elapsedTime){ 
    process->executionTime -= elapsedTime;
    process->remainingTime -= elapsedTime; 
}

//Caso ele seja preemptado, então o seu executionTime é resetado para poder ser comparado depois.
void reset_execution_time(SimulatedProcessData * process){ process->executionTime = process->burstTime; }

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atoi(deadline); }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime = atoi(burstTime);
    process->remainingTime = atoi(burstTime);
    process->executionTime = atoi(burstTime);
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
    
    bool deadlineMet = process->deadline >= roundedFinishTime ? true : false;
    int clockTime = roundedFinishTime - process->arrival;
    snprintf(outputLine, MAX_LINE_SIZE, "%s %d %d %d", process->name, clockTime, roundedFinishTime, deadlineMet);
    
    //Só pra garantir que nenhum par de threads tente escrever no arquivo ao mesmo tempo.
    P(writeFileMutex);
    write_next_line(outputFile, outputLine);
    V(writeFileMutex);
}

void sleep_until(int nextArrival){
    struct timespec req;
    double targetTime = INITIAL_SIMULATION_TIME + nextArrival;
    req.tv_sec  = (time_t)targetTime;
    req.tv_nsec = (long)((targetTime - req.tv_sec) * 1e9);

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL);
}

void write_context_switches_quantity(){
    char outputLine[MAX_CONTEXT_SWITCH_ORDER];
    snprintf(outputLine, MAX_CONTEXT_SWITCH_ORDER, "%d", contextSwitches);

    //Não precisa de mutex porque esse cara só roda depois de todas as threads terem sido terminadas.
    write_next_line(outputFile, outputLine);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> Algoritmos e Estruturas de Dados <<<<<<<<<<<<<<<<<<<<<<<<<
*/

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MergeSort <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

//O objetivo é ordenar o arquivo por ordem de chegada, e assim, o FCFS
//Já fica praticamente pronto, e facilita a verificação dos demais também.

//Usa-se o MergeSort pra manter a estabilidade nos casos de dois processos terem o mesmo 
//Arrival time, mas um ter sido colocado primeiro que o outro no arquivo trace só pra garantir previsibilidade

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

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Queue Circular <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

//Utilizada em todos os escalonadores, mas de maneiras diferentes
//Para distribuir os processos entre os cores de processamento.
//No final, esse EP pareceu uma grande aplicação dos produtores e consumidores
//Mas com uma porrada de consumidor e só um produtor.

//Me pergunto o quão cursed é forçar um POO em C normal

void init_core_manager(CoreQueueManager *this, int maxSize, SchedulerModel model) {
    this->head = 0;
    this->tail = 0;
    this->length = 0;
    this->maxSize = maxSize;
    this->finished = 0;
    this->scheduler = model;
    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->cond, NULL);
    pthread_cond_init(&this->cond_running, NULL);
    
    this->idleCores    = calloc(AVAILABLE_CORES, AVAILABLE_CORES * sizeof(bool));
    this->readyQueue   = calloc(maxSize, maxSize * sizeof(SimulatedProcessData *) );
    this->runningQueue = calloc(AVAILABLE_CORES, AVAILABLE_CORES * sizeof(SimulatedProcessData *));
}

bool is_empty(CoreQueueManager *this){ return this->length == 0 ? true : false; }

void destroy_manager(CoreQueueManager *this) {
    free(this->readyQueue);
    free(this->runningQueue);
    free(this->idleCores);

    pthread_mutex_destroy(&this->mutex);
    pthread_cond_destroy(&this->cond);
    pthread_cond_destroy(&this->cond_running);
}

void preempt_longest_running_process(CoreQueueManager *this, SimulatedProcessData *newProcess){    
    int pos = -1;
    int largestRemainingTime = newProcess->executionTime;
    for (int i = 0; i < AVAILABLE_CORES; i++){
        if (this->runningQueue[i] != NULL && this->runningQueue[i]->executionTime > largestRemainingTime) {
            pos = i;
            largestRemainingTime = this->runningQueue[i]->executionTime;
        }
    }
    if (pos != -1) this->runningQueue[pos] = NULL;
}

void update_idle_cores(CoreQueueManager * this){
    for (int i = 0; i < AVAILABLE_CORES; i++){
        if (this->runningQueue[i] == NULL || this->runningQueue[i]->executionTime <= 1){
            this->idleCores[i] = true;
        }
    }
}

bool has_idle_cores(CoreQueueManager * this){
    for (int i = 0; i < AVAILABLE_CORES; i++){
        if (this->idleCores[i]){
            this->idleCores[i] = false;
            return true;
        }
    }
    return false;
}

void calculate_priority(SimulatedProcessData *process) {
    int currentTime = (int)(get_elapsed_time_from(INITIAL_SIMULATION_TIME)+0.5);
    int overTime = process->deadline - currentTime - process->remainingTime;

    //Se é impossível que um processo termine, então sua prioridade é reduzida ao mínimo.
    //Pra dar chance para os outros mais críticos conseguirem.
    //Talvez isso seja bem injusto... Mas é a vida né?
    int calculatedPriority;

    const int PRIORITY_RANGE = MIN_PRIORITY - MAX_PRIORITY;
    if (overTime < 0)       calculatedPriority = MIN_PRIORITY; //Processo impossível de ter sua deadline atendida, Prioridade Mínima.
    else if (overTime == 0) calculatedPriority = MAX_PRIORITY; //Se não for feito na hora, então não vai dar.      Prioridade Máxima
    else if (overTime > PRIORITY_RANGE) calculatedPriority = MIN_PRIORITY; //Se o processo tem tempo sobrando até a deadline, pode só deixar ele com prioridade mínima.
    else calculatedPriority = MAX_PRIORITY + overTime; //Se o valor está entre as prioridades, então só somamos o tempo restante pela prioridade máxima.
    
    //A definição de prioridades é linear constante.

    process->priority = calculatedPriority;
}

void enqueue(CoreQueueManager *this, SimulatedProcessData *process) {
    P(this->mutex);
    if (this->scheduler == FIRST_COME_FIRST_SERVED || this->scheduler == PRIORITY_SCHEDULING){
        this->readyQueue[this->tail] = process;
        this->tail = (this->tail + 1) % this->maxSize;
        this->length++;
    }
    else if (this->scheduler == SHORTEST_REMAINING_TIME_NEXT){
        int i = this->length - 1;
        while (i >= 0)
        {
            int idx = (this->head + i) % this->maxSize;
            if (this->readyQueue[idx]->executionTime > process->executionTime) {
                int nextIdx = (idx + 1) % this->maxSize;
                this->readyQueue[nextIdx] = this->readyQueue[idx];
                i--;
            } else {
                break;
            }
        }
        if(!has_idle_cores(this)) preempt_longest_running_process(this, process);
        int pos = (this->head + i + 1) % this->maxSize;
        this->readyQueue[pos] = process;
        this->tail = pos;
        this->length++;
    }
    pthread_cond_signal(&this->cond);
    V(this->mutex);
}


SimulatedProcessData *dequeue(CoreQueueManager *this) {
    SimulatedProcessData *process = NULL;
    P(this->mutex);
    while (is_empty(this) && !this->finished) {
        pthread_cond_wait(&this->cond, &this->mutex);
    }
    if (!is_empty(this)) {
        process = this->readyQueue[this->head];
        this->head = (this->head + 1) % this->maxSize;
        this->length--;
    }
    V(this->mutex);
    return process;
}