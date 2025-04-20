#define _GNU_SOURCE //Nescessário para poder usar o tryjoin e também o set_affinity
                    //Deve ser sempre colocado no topo.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX_PRIORITY -20
#define MIN_PRIORITY 19

//Honestamente, acho que essa forma é bem mais legal do que usar um nome gigante.
#define P(mutex) pthread_mutex_lock(&(mutex))
#define V(mutex) pthread_mutex_unlock(&(mutex))
#define min(a, b) ((a) < (b) ? (a) : (b))

/* É bem improvável que o número de trocas de contexto supere 200, mas só por garantia. */
#define MAX_CONTEXT_SWITCH_ORDER 5 
#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 1000
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50

//Importante para definir os intervalos de checagem para SRTN e de rotação da prioridade.
#define MIN_QUANTUM 1
#define MAX_QUANTUM 3

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
    int rear;
    int front;
    int length;      
    int maxSize;   
    bool finished;
    SchedulerModel scheduler;
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t cond_running;
    
    bool *idleCores;
    SimulatedProcessData **readyQueue;
    SimulatedProcessData **runningQueue;
} CoreQueueManager;

CoreQueueManager systemQueue;

//Lembrar de organizar isso depois.
void start_simulation_time();
bool open_file(FILE **, char*, char *);
bool read_next_line(FILE *, char*);
int  close_file(FILE *);
bool parse_line_data(char *, SimulatedProcessData *);
void set_process_name(SimulatedProcessData *, const char *);
void set_process_arrival_time(SimulatedProcessData *, const char *);
void set_process_burst_time(SimulatedProcessData *, const char *);
void set_process_deadline(SimulatedProcessData *, const char *);
int  get_array_of_processes(FILE *, SimulatedProcessData*[]);
void sort(SimulatedProcessData **, int, int);
void FCFS(SimulatedProcessData *[], int);
double get_elapsed_time_from(double startTime);
double get_current_time();
void join_threads(pthread_t *, int);
void busy_wait_until(double);
bool is_running(double, double);
void* thread_unit_FCFS(void *);
void finish_process(SimulatedProcessData *, double);
void write_context_switches_quantity();
void allocate_cpu(long);
void init_core_manager(CoreQueueManager *, int, SchedulerModel);
bool is_empty(CoreQueueManager *);
void destroy_manager(CoreQueueManager *);
void enqueue(CoreQueueManager *, SimulatedProcessData *);
void SRTN(SimulatedProcessData *[], int);
void* thread_unit_SRTN(void *);
SimulatedProcessData *dequeue(CoreQueueManager *);
void reset_execution_time(SimulatedProcessData *);
void update_process_duration(SimulatedProcessData *, int);
void suspend_until(int);
void update_idle_cores(CoreQueueManager *);
void *thread_unit_PS(void *);
void PS(SimulatedProcessData *[], int);
void init_scheduler_simulation(SchedulerModel, const char *);
void handle_process_termination_status(SimulatedProcessData *);
void initialize_simulation_cores(pthread_t **, SchedulerSimulationUnit);
int get_process_quantum(int);
void update_priority(SimulatedProcessData *, bool isInitial);
void signal_end_simulation();
void increment_context_switch_counter();
void clear_processes_list(SimulatedProcessData *[], int);
void clean_simulation_memory(SimulatedProcessData **, int, pthread_t *, CoreQueueManager *);


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

    AVAILABLE_CORES = sysconf(_SC_NPROCESSORS_ONLN);

    if (open_file(&outputFile, argv[3], "w")) exit(-1);

    init_scheduler_simulation(model, argv[2]);
    
    close_file(outputFile);

    exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void init_scheduler_simulation(SchedulerModel model, const char *traceFilename){
    FILE *traceFile;
    if (open_file(&traceFile, (char*)traceFilename, "r"))  exit(-1);
    SimulatedProcessData *processList[MAX_SIMULATED_PROCESSES];
    int numProcesses = get_array_of_processes(traceFile, processList);
    close_file(traceFile);
    sort(processList, 0, numProcesses-1);
    switch (model) {
        case FIRST_COME_FIRST_SERVED:      FCFS(processList, numProcesses); break;
        case SHORTEST_REMAINING_TIME_NEXT: SRTN(processList, numProcesses); break;
        case PRIORITY_SCHEDULING:          PS(processList, numProcesses); break;
        default: printf("Modelo não configurado\n"); exit(-1);
    }
    return;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

//Denominamos esse cara como o produtor
//Ele irá verificar todos os processos que chegaram em dado instante
//Adicionar eles na fila, depois ser suspenso pra dar 100% do lugar
//Para as outras threads consumidoras.
void FCFS(SimulatedProcessData * processList[], int numProcesses){
    init_core_manager(&systemQueue, numProcesses, FIRST_COME_FIRST_SERVED);
    pthread_t *simulationUnits;
    initialize_simulation_cores(&simulationUnits, thread_unit_FCFS);
    
    start_simulation_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);

        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival){
            enqueue(&systemQueue, processList[i++]);
        }
    }

    signal_end_simulation();
    join_threads(simulationUnits, AVAILABLE_CORES);
    write_context_switches_quantity();
    clean_simulation_memory(processList, numProcesses, simulationUnits, &systemQueue);
}

//Esses são os consumidores
void* thread_unit_FCFS(void *args) {
    long chosenCPU = (long)args;
    allocate_cpu(chosenCPU);

    while(true) {
        SimulatedProcessData *currentProcess = dequeue(&systemQueue);
        if (currentProcess == NULL) break;
        double endTime = currentProcess->burstTime;
        busy_wait_until(endTime);
        update_process_duration(currentProcess, endTime);
        handle_process_termination_status(currentProcess);
    }
    pthread_exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void SRTN(SimulatedProcessData * processList[], int numProcesses) {
    init_core_manager(&systemQueue, numProcesses, SHORTEST_REMAINING_TIME_NEXT);    
    pthread_t *simulationUnits;
    initialize_simulation_cores(&simulationUnits, thread_unit_SRTN);
    
    start_simulation_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);
        
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
    
    signal_end_simulation();
    join_threads(simulationUnits, AVAILABLE_CORES);
    write_context_switches_quantity();
    clean_simulation_memory(processList, numProcesses, simulationUnits, &systemQueue);
}

void* thread_unit_SRTN(void *args) {
    long chosenCPU = (long)args;
    allocate_cpu(chosenCPU);


    while(true) {
        SimulatedProcessData *currentProcess = dequeue(&systemQueue);
        if (currentProcess == NULL) break;
        systemQueue.runningQueue[chosenCPU] = currentProcess;
        while (currentProcess->remainingTime > 0) {

            P(syncMutex);
            while (isSwitchingContext) pthread_cond_wait(&syncCond, &syncMutex);
            V(syncMutex);
            if (systemQueue.runningQueue[chosenCPU] == NULL) break;

            double endTime = min(MIN_QUANTUM, currentProcess->remainingTime);
            busy_wait_until(endTime);
            update_process_duration(currentProcess, endTime);
        }

        handle_process_termination_status(currentProcess);
    }
    pthread_exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>> Escalonamento por Prioridade <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void PS(SimulatedProcessData * processList[], int numProcesses) {
    init_core_manager(&systemQueue, numProcesses, PRIORITY_SCHEDULING);
    pthread_t *simulationUnits;
    initialize_simulation_cores(&simulationUnits, thread_unit_PS);
    
    start_simulation_time();
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival){
            enqueue(&systemQueue, processList[i++]);
        }
    }
    
    signal_end_simulation();
    join_threads(simulationUnits, AVAILABLE_CORES);
    write_context_switches_quantity();
    clean_simulation_memory(processList, numProcesses, simulationUnits, &systemQueue);
}

void *thread_unit_PS(void * args){
    long chosenCPU = (long)args;
    allocate_cpu(chosenCPU);


    while(true) {
        SimulatedProcessData *currentProcess = dequeue(&systemQueue);
        if (currentProcess == NULL) break;
        systemQueue.runningQueue[chosenCPU] = currentProcess;
        int processQuantum = get_process_quantum(currentProcess->priority);
        double endTime = min(currentProcess->remainingTime, processQuantum);
        busy_wait_until(endTime);
        update_process_duration(currentProcess, endTime);
        handle_process_termination_status(currentProcess);
    }
    pthread_exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>> Funções Comuns dos escalonadores <<<<<<<<<<<<<<<<<<<<<<<
*/

void initialize_simulation_cores(pthread_t **simulationUnits, SchedulerSimulationUnit functionModel) {
    *simulationUnits = malloc(AVAILABLE_CORES * sizeof(pthread_t));
    for (long i = 0; i < AVAILABLE_CORES; i++) {
        pthread_create(&(*simulationUnits)[i], NULL, functionModel, (void *) i);
    }
}

void allocate_cpu(long chosenCPU) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    

    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void join_threads(pthread_t *simulationUnits, int num_threads) {
    for (int t = 0; t < num_threads; t++) { pthread_join(simulationUnits[t], NULL); }
}

int get_priority_range(){ return MIN_PRIORITY - MAX_PRIORITY; }
int get_quantum_range() { return MAX_QUANTUM - MIN_QUANTUM; }

void clear_processes_list(SimulatedProcessData *processList[], int numProcesses) {
    for (int i = 0; i < numProcesses; i++) { free(processList[i]); }
}

int get_process_quantum(int priority){
    const int PRIORITY_RANGE = get_priority_range();
    const int QUANTUM_RANGE = MAX_QUANTUM - MIN_QUANTUM;
    double angularCoefficient = (double)(priority - MAX_PRIORITY) / PRIORITY_RANGE;
    //Agora entendi porque falam que SO é de 5º período,
    //Se eu não estivesse adiantado em LabNum teria perdido um bom tempo aqui.
    return MIN_QUANTUM + (int)(angularCoefficient * QUANTUM_RANGE);
}

void handle_process_termination_status(SimulatedProcessData *currentProcess) {
    if (currentProcess->remainingTime <= 0) {
        double endTime = get_elapsed_time_from(INITIAL_SIMULATION_TIME);
        finish_process(currentProcess, endTime);
    } else {
        increment_context_switch_counter();
        reset_execution_time(currentProcess);
        update_priority(currentProcess, false);
        enqueue(&systemQueue, currentProcess);
    }
}

void increment_context_switch_counter(){
    P(preemptionCounterMutex);
    contextSwitches++;
    V(preemptionCounterMutex);
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
bool open_file(FILE **fileDescriptor, char *filepath, char *fileMode) {
    *fileDescriptor = fopen(filepath, fileMode);
    return (*fileDescriptor == NULL);
}

//Lê a próxima linha do arquivo e retorna 0 se o arquivo tiver acabado
//Ou retorna 1 caso ele tenha lido uma linha.
bool read_next_line(FILE * fileDescriptor, char* lineBuffer){ return fgets(lineBuffer, MAX_LINE_SIZE, fileDescriptor) != NULL;  }

//Escreve uma linha em um arquivo de interesse e adiciona uma quebra no final.
bool write_next_line(FILE *fileDescriptor, char *lineBuffer) { return fprintf(fileDescriptor, "%s\n", lineBuffer) >= 0;  }

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

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atoi(deadline); }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime = atoi(burstTime);
    process->remainingTime = atoi(burstTime);
    process->executionTime = atoi(burstTime);
}


/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

bool parse_line_data(char * line, SimulatedProcessData * process){
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

bool is_running(double startTime, double endTime)  { return get_elapsed_time_from(startTime) < endTime; } 

void finish_process(SimulatedProcessData * process, double finishTime){
    char outputLine[MAX_LINE_SIZE];

    //Quase 100% das vezes, arredonda pra baixo, mas só pra garantir né.
    int roundedFinishTime = (int)(finishTime + 0.5);
    
    bool deadlineMet = process->deadline >= roundedFinishTime;
    int clockTime = roundedFinishTime - process->arrival;
    snprintf(outputLine, MAX_LINE_SIZE, "%s %d %d %d", process->name, clockTime, roundedFinishTime, deadlineMet);
    
    P(writeFileMutex);
    write_next_line(outputFile, outputLine);
    V(writeFileMutex);
}

void suspend_until(int nextArrival){
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

void signal_end_simulation(){
    P(systemQueue.mutex);
    systemQueue.finished = true;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.mutex);
}

void start_simulation_time(){ INITIAL_SIMULATION_TIME = get_current_time(); }

void clean_simulation_memory(SimulatedProcessData **processList, int numProcesses, pthread_t *simulationUnits, CoreQueueManager *systemQueue) {
    clear_processes_list(processList, numProcesses);
    free(simulationUnits);
    destroy_manager(systemQueue);
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

void merge(SimulatedProcessData* arr[], SimulatedProcessData* temp[], int left, int mid, int right) {
    int i = left;
    int j = mid + 1;
    int k = left;

    while (i <= mid && j <= right) {
        if (arr[i]->arrival <= arr[j]->arrival) temp[k++] = arr[i++];
        else                                    temp[k++] = arr[j++];
    }

    while (i <= mid) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];

    for (int l = left; l <= right; l++) arr[l] = temp[l]; 
}

void mergeSort(SimulatedProcessData* arr[], SimulatedProcessData* temp[], int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(arr, temp, left, mid);     
        mergeSort(arr, temp, mid + 1, right);
        merge(arr, temp, left, mid, right);
    }
}

//Estrutura copiada do Livro do SedgeWick (era pra Java, mas a ideia é a mesma)
void sort(SimulatedProcessData **arr, int left, int right){
    int size = right - left + 1;
    SimulatedProcessData** temp = malloc(size * sizeof(SimulatedProcessData*));
    mergeSort(arr, temp, left, right);
    free(temp);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Queue Circular <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void init_core_manager(CoreQueueManager *this, int maxSize, SchedulerModel model) {
    this->front = 0;
    this->rear = 0;
    this->length = 0;
    this->maxSize = maxSize;
    this->finished = false;
    this->scheduler = model;
    pthread_mutex_init(&this->mutex, NULL);
    pthread_cond_init(&this->cond, NULL);
    pthread_cond_init(&this->cond_running, NULL);
    
    this->idleCores    = calloc(AVAILABLE_CORES, sizeof(bool));
    this->readyQueue   = calloc(maxSize, sizeof(SimulatedProcessData *));
    this->runningQueue = calloc(AVAILABLE_CORES, sizeof(SimulatedProcessData *));
}

bool is_empty(CoreQueueManager *this){ return this->length == 0; }

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

bool exist_idle_cores(CoreQueueManager * this){
    for (int i = 0; i < AVAILABLE_CORES; i++){
        if (this->idleCores[i]){
            this->idleCores[i] = false;
            return true;
        }
    }
    return false;
}

void update_priority(SimulatedProcessData *process, bool isInitial) {
    int currentTime = (int)(get_elapsed_time_from(INITIAL_SIMULATION_TIME) + 0.5);
    int overTime = process->deadline - currentTime - process->remainingTime;
    if (overTime < 0) process->priority = MIN_PRIORITY;
    else {
        if (isInitial) {
            const int PRIORITY_RANGE = get_priority_range();
            if (overTime == 0)                  
                process->priority = MAX_PRIORITY;
            else if (overTime > PRIORITY_RANGE) 
                process->priority = MIN_PRIORITY;
            else                                
                process->priority = MAX_PRIORITY + overTime;
        } else {
            process->priority++;
            if (process->priority > MIN_PRIORITY)
                process->priority = MIN_PRIORITY;
        }
    }
}

int compareProcesses(const SimulatedProcessData *p1, const SimulatedProcessData *p2, SchedulerModel scheduler) {
    switch(scheduler) {
        case FIRST_COME_FIRST_SERVED:      return p1->arrival - p2->arrival;
        case SHORTEST_REMAINING_TIME_NEXT: return p1->executionTime - p2->executionTime;
        case PRIORITY_SCHEDULING:{
            int priorityDiff = p2->priority - p1->priority;
            return (priorityDiff != 0) ? priorityDiff : (p1->executionTime - p2->executionTime);            
        }
        default:
            return 0;
    }
}

void swap(SimulatedProcessData **a, SimulatedProcessData **b) {
    SimulatedProcessData *temp = *a;
    *a = *b;
    *b = temp;
}

int circular_index(CoreQueueManager *this, int idx) {
    return (this->front + idx) % this->maxSize;
}

// Função para inserir mantendo a ordenação
// Usa insertionSort, pode não ser o mais eficiente, mas dado que 
// o número de processos é pequeno, ainda é mais eficiente do que o MergeSort.
// Mantendo ainda a estabilidade, o que é importante para este modelo.
void ordered_insert(CoreQueueManager *this, SimulatedProcessData *process) {
    int insertPos = this->length;
    
    // Encontra a posição correta para inserção
    for(int i = 0; i < this->length; i++) {
        int idx = circular_index(this, i);
        if(compareProcesses(process, this->readyQueue[idx], this->scheduler) < 0) {
            insertPos = i;
            break;
        }
    }
    
    // Desloca os elementos para abrir espaço
    for(int i = this->length; i > insertPos; i--) {
        int destIdx = circular_index(this, i);
        int srcIdx = circular_index(this, i-1);
        this->readyQueue[destIdx] = this->readyQueue[srcIdx];
    }
    
    // Insere o processo na posição correta
    this->readyQueue[circular_index(this, insertPos)] = process;
    this->length++;
    this->rear = circular_index(this, this->length);
}

void fifo_enqueue(CoreQueueManager *this, SimulatedProcessData *process) {
    this->readyQueue[this->rear] = process;
    this->rear = (this->rear + 1) % this->maxSize;
    this->length++;
}

void enqueue(CoreQueueManager *this, SimulatedProcessData *process) {
    P(this->mutex);
    if (this->scheduler == FIRST_COME_FIRST_SERVED) {
        //Shortcut pra simplificar o FCFS que não precisa de ordenação adicional.
        fifo_enqueue(this, process);    
    } else {
        if (process->priority == 0) {
            update_priority(process, true);
        }

        ordered_insert(this, process);
        if (this->scheduler == SHORTEST_REMAINING_TIME_NEXT && !exist_idle_cores(this)) {
            preempt_longest_running_process(this, process);
        }
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
        process = this->readyQueue[this->front];
        this->front = (this->front + 1) % this->maxSize;
        this->length--;
    }
    
    V(this->mutex);
    return process;
}
