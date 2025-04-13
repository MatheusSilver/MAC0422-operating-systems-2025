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
#define MAX_QUANTUM 15

#define FILE_SEPARATOR " "

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preemptionCounterMutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    char name[MAX_PROCESS_NAME];
    int arrival;
    int burstTime;
    int deadline;
    int executionTime;
    int remainingTime;
} SimulatedProcessData;

typedef struct {
    int head;       
    int tail;        
    int length;      
    int maxSize;   
    int finished;
    int isSRTN;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t cond_running;

    SimulatedProcessData **readyQueue;
    SimulatedProcessData **runningQueue;
} CoreQueueManager;

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
void join_threads(pthread_t *, int);
void busy_wait_until(double);
int is_running(double, double);
void* thread_unit_FCFS(void *);
void finish_process(SimulatedProcessData *, double);
void write_context_switches_quantity();
void allocate_cpu(long);
void init_core_manager(CoreQueueManager *, int, int);
int is_empty(CoreQueueManager *);
void destroy_queue(CoreQueueManager *);
void enqueue(CoreQueueManager *, SimulatedProcessData *);
void SRTN(SimulatedProcessData *[], int);
void* thread_unit_SRTN(void *);
SimulatedProcessData *dequeue(CoreQueueManager *);
void reset_execution_time(SimulatedProcessData *);
void update_process_duration(SimulatedProcessData *, int);


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
    
    SRTN(processList, numProcesses);
    
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

//Denominamos esse cara como o produtor
//Ele irá verificar todos os processos que chegaram em dado instante
//Adicionar eles na fila, depois ser suspenso pra dar 100% do lugar
//Para as outras threads consumidoras.
void FCFS(SimulatedProcessData * processList[], int numProcesses){
    CoreQueueManager systemQueue;
    init_core_manager(&systemQueue, numProcesses, false);

    //Esse é o tempo 0 da Simulação a ser adotado como referência.
    INITIAL_SIMULATION_TIME = get_current_time();
    
    pthread_t * consumerThreads = malloc(AVAILABLE_CORES * sizeof(pthread_t));
    for (long i = 0; i < AVAILABLE_CORES; i++) {
        // Prepara um array de 2 elementos para passar [CoreQueueManager*, core_id]
        void **args = malloc(2 * sizeof(void *));
        args[0] = &systemQueue;
        args[1] = (void*) i;  
        pthread_create(&consumerThreads[i], NULL, thread_unit_FCFS, args);
    }
    for (int i = 0; i < numProcesses; i++) {
        SimulatedProcessData *currentProcess = processList[i];
        while (currentProcess->arrival > get_elapsed_time_from(INITIAL_SIMULATION_TIME)) sleep(1);
        enqueue(&systemQueue, currentProcess);
    }
    
    P(systemQueue.mutex);
    systemQueue.finished = 1;
    //Funny que o professor disse que raramente usa broadcast
    //E no fim era justamente o que eu precisava k
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);

    join_threads(consumerThreads, AVAILABLE_CORES);

    free(consumerThreads);
    write_context_switches_quantity();
    destroy_queue(&systemQueue);
}

//Esses são os consumidores
void* thread_unit_FCFS(void *args) {
    CoreQueueManager *systemQueue = ((void**)args)[0];
    long chosenCPU = (long)((void**)args)[1];  //Lado bom de saber Assembly é economizar tempo com essas bizarrices...
    allocate_cpu(chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(systemQueue);
        //Só retorna null se a fila tiver acabado se não o consumidor vai esperar o produtor produzir o processo
        //Explicando em termos melhores, o produtor produzir é o mesmo que dizer "o processo chegar".
        if (currentProcess == NULL) break;
        double endTime = currentProcess->burstTime;
        busy_wait_until(endTime);
        endTime = get_current_time() - INITIAL_SIMULATION_TIME;
        finish_process(currentProcess, endTime);

    }
    free(args);
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void SRTN(SimulatedProcessData * processList[], int numProcesses) {
    CoreQueueManager systemQueue;
    init_core_manager(&systemQueue, numProcesses, true);
    
    INITIAL_SIMULATION_TIME = get_current_time();
    
    pthread_t *consumerThreads = malloc(AVAILABLE_CORES * sizeof(pthread_t));
    for (long i = 0; i < AVAILABLE_CORES; i++) {
        void **args = malloc(2 * sizeof(void *));
        args[0] = &systemQueue;
        args[1] = (void *) i;
        pthread_create(&consumerThreads[i], NULL, thread_unit_SRTN, args);
    }
    
    for (int i = 0; i < numProcesses; i++) {
        SimulatedProcessData *currentProcess = processList[i];
        while (currentProcess->arrival > get_elapsed_time_from(INITIAL_SIMULATION_TIME)) sleep(1);
        enqueue(&systemQueue, currentProcess);
    }
    
    P(systemQueue.mutex);
    systemQueue.finished = 1;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);
    
    join_threads(consumerThreads, AVAILABLE_CORES);
    
    free(consumerThreads);
    write_context_switches_quantity();
    destroy_queue(&systemQueue);
}

void* thread_unit_SRTN(void *args) {
    CoreQueueManager *systemQueue = ((void**)args)[0];
    long chosenCPU = (long)((void**)args)[1];
    allocate_cpu(chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(systemQueue);
        if (currentProcess == NULL) break;
        systemQueue->runningQueue[chosenCPU] = currentProcess;
        while (currentProcess->remainingTime > 0) {

            //Pega o mínimo entre o intervalo de verificação e o tempo restante.
            //Aqui é sempre MIN_QUANTUM, mas isso é readaptado lá na prioridade.
            if (currentProcess != systemQueue->runningQueue[chosenCPU]) break;

            double endTime = min(MIN_QUANTUM, currentProcess->remainingTime);
            busy_wait_until(endTime);
            update_process_duration(currentProcess, endTime);
            
        }

        if (currentProcess->remainingTime <= 0) {
            double endTime = get_current_time() - INITIAL_SIMULATION_TIME;
            finish_process(currentProcess, endTime);            
        }else{
            P(preemptionCounterMutex);
            contextSwitches++;
            V(preemptionCounterMutex);
            reset_execution_time(currentProcess);
            enqueue(systemQueue, currentProcess);
        }

    }
    free(args);
    return NULL;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>> Escalonamento por Prioridade <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void allocate_cpu(long chosenCPU) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void join_threads(pthread_t *consumerThreads, int num_threads) {
    for (int t = 0; t < num_threads; t++) {
        pthread_join(consumerThreads[t], NULL);
    }
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÃO DE ESPERA OCUPADA <<<<<<<<<<<<<<<<<<<<<<<<<
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

void set_process_name(SimulatedProcessData *process, const char* name){ strcpy(process->name, name); }

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){ process->arrival = atof(arrivalTime); }

void update_process_duration(SimulatedProcessData * process, int elapsedTime){ 
    process->executionTime -= elapsedTime;
    process->remainingTime -= elapsedTime; 
}

//Caso ele seja preemptado, então o seu executionTime é resetado para poder ser comparado depois.
void reset_execution_time(SimulatedProcessData * process){ process->executionTime = process->burstTime; }

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atof(deadline); }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime = atof(burstTime);
    process->remainingTime = atof(burstTime);
    process->executionTime = atof(burstTime);
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

/*
>>>>>>>>>>>>>>>>>>>>>>>>> Algoritmos e Estruturas de Dados <<<<<<<<<<<<<<<<<<<<<<<<<
*/

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MergeSort <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

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

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Queue Circular <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

//Utilizada em todos os escalonadores, mas de maneiras diferentes
//Para distribuir os processos entre os cores de processamento.
//No final, esse EP pareceu uma grande aplicação dos produtores e consumidores
//Mas com uma porrada de consumidor e só um produtor.

//Me pergunto o quão cursed é forçar um POO em C normal

void init_core_manager(CoreQueueManager *this, int maxSize, int isSRTN) {
    this->head = 0;
    this->tail = 0;
    this->length = 0;
    this->maxSize = maxSize;
    this->finished = 0;
    this->isSRTN = isSRTN;
    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->cond, NULL);
    pthread_cond_init(&this->cond_running, NULL)
    
    this->readyQueue   = calloc(maxSize, maxSize * sizeof(SimulatedProcessData *) );
    this->runningQueue = calloc(AVAILABLE_CORES, AVAILABLE_CORES * sizeof(SimulatedProcessData *));
}

int is_empty(CoreQueueManager *this){ return this->length == 0 ? true : false; }

void destroy_queue(CoreQueueManager *this) {
    free(this->readyQueue);
    free(this->runningQueue);
    pthread_mutex_destroy(&this->mutex);
    pthread_cond_destroy(&this->cond);
    pthread_cond_destroy(&this->cond_running);
}

void switch_largest_running_process(CoreQueueManager *this, SimulatedProcessData *newProcess){
    int pos = -1;
    int largestRemainingTime = newProcess->executionTime;
    for (int i = 0; i < AVAILABLE_CORES; i++){
        if (this->runningQueue[i] != NULL && this->runningQueue[i]->executionTime > largestRemainingTime) {
            pos = i;
            largestRemainingTime = this->runningQueue[i]->executionTime;
        }
    }
    if (pos != -1) this->runningQueue[pos] = newProcess;
}

void enqueue(CoreQueueManager *this, SimulatedProcessData *process) {
    P(this->mutex);
    if (this->isSRTN){
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
        switch_largest_running_process(this, process);
        int pos = (this->head + i + 1) % this->maxSize;
        this->readyQueue[pos] = process;
        this->tail = pos;
        this->length++;
    }
    else{
        this->tail = (this->tail + 1) % this->maxSize;
        this->readyQueue[this->tail] = process;
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
