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
#define max(a, b) ((a) > (b) ? (a) : (b))

/* É bem improvável que o número de trocas de contexto supere 200, mas só por garantia. */
#define MAX_CONTEXT_SWITCH_ORDER 5 
#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 1000
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50
#define STANDARD_LATE 0.1

//Importante para definir os intervalos de checagem para SRTN e de rotação da prioridade.
#define MIN_QUANTUM 1
#define MAX_QUANTUM 5

#define FILE_SEPARATOR " "

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preemptionCounterMutex = PTHREAD_MUTEX_INITIALIZER;

//Acho que estou inventando muita moda...
//https://stackoverflow.com/questions/9410/how-do-you-pass-a-function-as-a-parameter-in-c
typedef enum {
    FIRST_COME_FIRST_SERVED = 1,
    SHORTEST_REMAINING_TIME_NEXT = 2,
    PRIORITY_SCHEDULING = 3
} SchedulerModel;

typedef enum {
    READY,
    RUNNING,
    TERMINATED
} ProcessStatus; //Teria um bloqueado, mas não é usado nesse caso.

typedef struct {
    char name[MAX_PROCESS_NAME];
    int arrival;
    int burstTime;
    int deadline;
    double executionTime;
    double remainingTime;
    double quantumTime;
    int priority;
    int ID;
    ProcessStatus status;
} SimulatedProcessData;


typedef struct {
    SimulatedProcessData *processData;
    pthread_t threadID;
    long cpuID;
    bool paused;
    pthread_mutex_t pauseMutex;
    pthread_cond_t pauseCond;
} SimulatedProcessUnit;

typedef struct {  
    int rear;
    int front;
    int length;      
    int maxSize;   
    bool allProcessesArrived;
    SchedulerModel scheduler;
    
    pthread_mutex_t readyQueueMutex;
    pthread_mutex_t runningQueueMutex;
    pthread_mutex_t schedulerMutex;

    pthread_cond_t cond;
    pthread_cond_t wakeUpScheduler;
    
    bool *idleCores;
    SimulatedProcessUnit **readyQueue;
    SimulatedProcessUnit **runningQueue;
} CoreQueueManager;

typedef void* (*TargetProcess)(void*); 
CoreQueueManager systemQueue;

//Lembrar de organizar isso depois.
void *execute_process(void *args);
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
void join_simulation_threads(SimulatedProcessUnit **, int);
void finish_process(SimulatedProcessData *, double);
void write_context_switches_quantity();
void allocate_cpu(pthread_t, long);
void init_core_manager(CoreQueueManager *, int, SchedulerModel);
bool is_empty(CoreQueueManager *);
void destroy_manager(CoreQueueManager *);
void enqueue(CoreQueueManager *, SimulatedProcessUnit *);
void SRTN(SimulatedProcessData *[], int);
SimulatedProcessUnit *dequeue(CoreQueueManager *);
void reset_execution_time(SimulatedProcessData *);
void update_process_duration(SimulatedProcessData *, double);
void suspend_until(int);
void PS(SimulatedProcessData *[], int);
void init_scheduler_simulation(SchedulerModel, const char *);
void handle_process_termination_status(SimulatedProcessData *, long);
int get_process_quantum(int);
void update_priority(SimulatedProcessData *, bool isInitial);
void finish_process_arrival();
void increment_context_switch_counter();
void clear_processes_list(SimulatedProcessData *[], int);
void clean_simulation_memory(SimulatedProcessUnit **, int, CoreQueueManager *);

double INITIAL_SIMULATION_TIME; //Variável global READ_ONLY
unsigned int contextSwitches = 0;
FILE *outputFile;

//Só pra não precisar adicionar math.h
int customRound(double value) { return (int)(value + 0.5); }

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
        // case PRIORITY_SCHEDULING:          PS(processList, numProcesses); break;
        default: printf("Modelo não configurado\n"); exit(-1);
    }
    return;
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

SimulatedProcessUnit* initialize_process_simulation_unit(SimulatedProcessData *processData) {
    SimulatedProcessUnit *processUnit = malloc(sizeof(SimulatedProcessUnit));
    pthread_mutex_init(&processUnit->pauseMutex, NULL);
    pthread_cond_init(&processUnit->pauseCond, NULL);
    processUnit->processData = processData;
    //Assim como falado em aula, todo processo começa como pronto
    //Vai pra RUNNING, depois se precisar de IO vai pra BLOCKED
    //Mas isso não é necessário nesse caso, então só deixamos como READY mesmo.
    processUnit->processData->status = READY;
    processUnit->cpuID = -1;
    return processUnit;
}

long get_available_core() {
    for (long i = 0; i < AVAILABLE_CORES; i++) {
        if (systemQueue.runningQueue[i] == NULL) {
            return i;
        }
    }
    return -1;
}

bool has_available_core() {
    for (int i = 0; i < AVAILABLE_CORES; i++) {
        if (systemQueue.runningQueue[i] == NULL) {
            return true;
        }
    }
    return false;
}

//Denominamos esse cara como o produtor
//Ele irá verificar todos os processos que chegaram em dado instante
//Adicionar eles na fila, depois ser suspenso pra dar 100% do lugar
//Para as outras threads consumidoras.

typedef struct {
    SimulatedProcessUnit **processUnits;
    SimulatedProcessData **processList;
    int numProcesses;
} ThreadArgs;

//EU NUNCA ACHEI QUE MINHA EXPERIÊNCIA DESENVOLVENDO O CODEC DE VIDEO DO HAXE FOSSE AJUDAR EM ALGUM MOMENTO.
//O coração do EP são esses dois caras
void pause_thread(SimulatedProcessUnit *unit) {
    P(unit->pauseMutex);
    unit->paused = true;
    unit->processData->status = READY;
    V(unit->pauseMutex);
}

void resume_thread(SimulatedProcessUnit *unit) {
    P(unit->pauseMutex);
    unit->processData->status = RUNNING;
    unit->paused = false;
    pthread_cond_signal(&unit->pauseCond);
    V(unit->pauseMutex);
}

// Thread produtora - cria e enfileira processos
void* process_creator(void *args) {
    ThreadArgs *threadArgs = (ThreadArgs *)args;
    SimulatedProcessData **processList = threadArgs->processList;
    int numProcesses = threadArgs->numProcesses;
    int i = 0;

    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        
        // Suspende até o próximo processo chegar
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);
    
        P(systemQueue.schedulerMutex);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival) {
            // Cria a unidade de processo e armazena no vetor para join posterior
            SimulatedProcessUnit *processUnit = initialize_process_simulation_unit(processList[i]);
            update_priority(processUnit->processData, true);
            threadArgs->processUnits[i] = processUnit;
            processUnit->cpuID = -1;
            i++;
            pause_thread(processUnit);
            pthread_create(&processUnit->threadID, NULL, execute_process, processUnit);
            enqueue(&systemQueue, processUnit);
        }
        V(systemQueue.schedulerMutex);

        //Se for o SRTN acorda o escalonador para verificar se tem algum processo que pode ser preemptado
        //Após adicionar uma nova batelada de processos.
        if(systemQueue.scheduler == SHORTEST_REMAINING_TIME_NEXT){
            P(systemQueue.readyQueueMutex);
            pthread_cond_signal(&systemQueue.wakeUpScheduler);
            V(systemQueue.readyQueueMutex);
        }
    }

    finish_process_arrival();
    pthread_exit(0);
}


void FCFS(SimulatedProcessData * processList[], int numProcesses){
    init_core_manager(&systemQueue, numProcesses, FIRST_COME_FIRST_SERVED);
    // Cria um vetor para armazenar as unidades de processo criadas
    SimulatedProcessUnit **processUnits = malloc(numProcesses * sizeof(SimulatedProcessUnit*));
    ThreadArgs args = { processUnits, processList, numProcesses };
    
    
    // Cria a thread produtora que cria os processos conforme chegarem
    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, &args);
    start_simulation_time();
    
    // Loop de escalonamento: enquanto a simulação não for finalizada, 
    // verifica a disponibilidade dos núcleos e sinaliza os processos
    while (!systemQueue.allProcessesArrived || !is_empty(&systemQueue)) {
        SimulatedProcessUnit *processUnit = NULL;
    
        P(systemQueue.runningQueueMutex);
        while (!has_available_core()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);
        
        P(systemQueue.schedulerMutex);
        if (!is_empty(&systemQueue) && has_available_core()) {
            processUnit = dequeue(&systemQueue);
            if (processUnit != NULL) {
                long chosenCPU = get_available_core();
                processUnit->cpuID = chosenCPU;
                systemQueue.runningQueue[chosenCPU] = processUnit;
                allocate_cpu(processUnit->threadID, chosenCPU);
                resume_thread(processUnit);
            }
        }    
        V(systemQueue.schedulerMutex);
    }
    
    // Aguarda a thread produtora encerrar
    pthread_join(creatorThread, NULL);
    join_simulation_threads(processUnits, numProcesses);
    write_context_switches_quantity();
    clean_simulation_memory(processUnits, numProcesses, &systemQueue); 
}

SimulatedProcessUnit* peek_queue_front(CoreQueueManager *this) {
    if (!is_empty(this) && this->readyQueue[this->front] != NULL) {
        return this->readyQueue[this->front];
    }
    return NULL;
}

SimulatedProcessUnit* get_longest_running_process(CoreQueueManager *this) {
    SimulatedProcessUnit *longest = NULL;
    double largestExecutionTime = -1;
    
    for (int i = 0; i < AVAILABLE_CORES; i++) {
        if (this->runningQueue[i] != NULL && this->runningQueue[i]->processData->executionTime > largestExecutionTime) {
            longest = this->runningQueue[i];
            largestExecutionTime = longest->processData->executionTime;
        }
    }
    return longest;
}

bool should_preempt_SRTN() {
    SimulatedProcessUnit *newProcess = peek_queue_front(&systemQueue); 
    if (newProcess == NULL) return false;
    
    SimulatedProcessUnit *longest = get_longest_running_process(&systemQueue);
    if (longest == NULL) return false;
    
    return newProcess->processData->executionTime < longest->processData->executionTime;
}

void wait_for_finish_iminence() {
    SimulatedProcessUnit *shortest = NULL;
    double minRemaining = 1e9;

    for (int i = 0; i < AVAILABLE_CORES; i++) {
        SimulatedProcessUnit *unit = systemQueue.runningQueue[i];
        if (unit != NULL) {
            double remaining = unit->processData->remainingTime;
            if (remaining < minRemaining && remaining <= STANDARD_LATE) {
                shortest = unit;
                minRemaining = remaining;
            }
        }
    }

    // Se não houver processos com tempo restante menor que o atraso esperado, retorna
    if (shortest == NULL) return;

    //Caso contrário, verifica se o processo mais curto ainda está prestes a terminar
    //E se sim, espera até ele sinalizar que terminou.
    P(systemQueue.runningQueueMutex);
    while (shortest->processData->status == RUNNING) {
        pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
    }
    V(systemQueue.runningQueueMutex);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void SRTN(SimulatedProcessData *processList[], int numProcesses) {
    init_core_manager(&systemQueue, numProcesses, SHORTEST_REMAINING_TIME_NEXT);
    SimulatedProcessUnit **processUnits = malloc(numProcesses * sizeof(SimulatedProcessUnit*));
    ThreadArgs args = { processUnits, processList, numProcesses };

    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, &args);
    start_simulation_time();

    while (!systemQueue.allProcessesArrived || !is_empty(&systemQueue)) {
        SimulatedProcessUnit *processUnit = NULL;

        P(systemQueue.runningQueueMutex);
        //Se não tem cores disponíveis, ou não deve preemptar
        //Então o SRTN espera que o criador de processos crie processos mais curtos OU
        //Que algum processo libere um core.
        while (!has_available_core() && !should_preempt_SRTN()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);

        P(systemQueue.schedulerMutex);
        while (!is_empty(&systemQueue) && (should_preempt_SRTN() || has_available_core())) {
            //Se não houverem cores disponíveis, mas houverem processos EXTREMAMENTE próximos de terminar
            //Aguardamos eles terminarem antes de continuar pra evitar trocas de contexto desnecessárias.
            if (!has_available_core()) wait_for_finish_iminence();

            bool shouldPreempt = should_preempt_SRTN();
            bool hasAvailableCore = has_available_core();
            //Caso o Ivan esteja lendo isso... Eu perdi 2 dias inteiros porque não reparei
            //Que após dar dequeue, o processo que estava no topo da fila de execução não era mais o mesmo.
            //Consequentemente a shouldPreempt não iria retornar que o topo da fila deveria começar a rodar...
            processUnit = dequeue(&systemQueue);
            if (processUnit != NULL) {
                if (shouldPreempt && !hasAvailableCore) {
                    SimulatedProcessUnit *longestRunningProcess = get_longest_running_process(&systemQueue);
                    if (longestRunningProcess != NULL) {
                        pause_thread(longestRunningProcess);
                        systemQueue.runningQueue[longestRunningProcess->cpuID] = NULL;
                        longestRunningProcess->cpuID = -1;
                        enqueue(&systemQueue, longestRunningProcess);
                    }
                }
                long chosenCPU = get_available_core();
                processUnit->cpuID = chosenCPU;
                systemQueue.runningQueue[chosenCPU] = processUnit;
                allocate_cpu(processUnit->threadID, chosenCPU);
                resume_thread(processUnit);
            }
        }
        V(systemQueue.schedulerMutex);
    }

    pthread_join(creatorThread, NULL);
    join_simulation_threads(processUnits, numProcesses);
    write_context_switches_quantity();
    clean_simulation_memory(processUnits, numProcesses, &systemQueue);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>> Escalonamento por Prioridade <<<<<<<<<<<<<<<<<<<<<<<<<
*/

void PS(SimulatedProcessData *processList[], int numProcesses) {
    init_core_manager(&systemQueue, numProcesses, PRIORITY_SCHEDULING);
    SimulatedProcessUnit **processUnits = malloc(numProcesses * sizeof(SimulatedProcessUnit *));
    ThreadArgs args = { processUnits, processList, numProcesses };

    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, &args);
    start_simulation_time();

    while (!systemQueue.allProcessesArrived || !is_empty(&systemQueue)) {
        SimulatedProcessUnit *processUnit = NULL;

        P(systemQueue.runningQueueMutex);
        while (!has_available_core()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);

        P(systemQueue.isAddingMutex);
        while (!is_empty(&systemQueue) && has_available_core()) {
            processUnit = dequeue(&systemQueue);
            if (processUnit != NULL) {
                long chosenCPU = get_available_core();
                processUnit->cpuID = chosenCPU;
                systemQueue.runningQueue[chosenCPU] = processUnit;
                allocate_cpu(processUnit->threadID, chosenCPU);
                resume_thread(processUnit);
            }
        }
        V(systemQueue.isAddingMutex);
    }

    pthread_join(creatorThread, NULL);
    join_simulation_threads(processUnits, numProcesses);
    write_context_switches_quantity();
    clean_simulation_memory(processUnits, numProcesses, &systemQueue);
}

/*
>>>>>>>>>>>>>>>>>>>>>> Funções Comuns dos escalonadores <<<<<<<<<<<<<<<<<<<<<<<
*/

void allocate_cpu(pthread_t targetProcess, long chosenCPU) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    
    pthread_setaffinity_np(targetProcess, sizeof(cpu_set_t), &cpuset);
}

void join_simulation_threads(SimulatedProcessUnit **simulationUnits, int num_threads) {
    for (int t = 0; t < num_threads; t++) { pthread_join(simulationUnits[t]->threadID, NULL); }
}

int get_priority_range(){ return MIN_PRIORITY - MAX_PRIORITY; }
int get_quantum_range() { return MAX_QUANTUM - MIN_QUANTUM; }

void destroy_process_unit(SimulatedProcessUnit *unit) {
    pthread_mutex_destroy(&unit->pauseMutex);
    pthread_cond_destroy(&unit->pauseCond);
    free(unit->processData);
    free(unit);
}

int get_process_quantum(int priority){
    const int PRIORITY_RANGE = get_priority_range();
    const int QUANTUM_RANGE = get_quantum_range();
    double angularCoefficient = (double)(priority - MAX_PRIORITY) / PRIORITY_RANGE;
    //Agora entendi porque falam que SO é de 5º período,
    //Se eu não estivesse adiantado em LabNum teria perdido um bom tempo aqui.
    return MIN_QUANTUM + (int)(angularCoefficient * QUANTUM_RANGE);
}

void handle_process_termination_status(SimulatedProcessData *currentProcess, long cpuSlot) {
    if (currentProcess->remainingTime <= 0) {
        double endTime = get_elapsed_time_from(INITIAL_SIMULATION_TIME);
        finish_process(currentProcess, endTime);
        currentProcess->status = TERMINATED;
        
        P(systemQueue.runningQueueMutex);
        systemQueue.runningQueue[cpuSlot] = NULL;
        pthread_cond_signal(&systemQueue.wakeUpScheduler);   
        V(systemQueue.runningQueueMutex);
        
    } else {
        increment_context_switch_counter();
        reset_execution_time(currentProcess);
        update_priority(currentProcess, false);
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

void consume_execution_time(SimulatedProcessData *process) { 
    double startTime = get_current_time();
    double lastTime = startTime;
    double elapsedTime = 0;
    double fullExecutionTime = process->remainingTime;

    while (elapsedTime < fullExecutionTime) {
        double currentTime = get_current_time();
        double interval = currentTime - lastTime;
        lastTime = currentTime;
        
        elapsedTime += interval;
        update_process_duration(process, interval);
        
        if (process->status != RUNNING) break;
    }
}

void *execute_process(void *args) {
    SimulatedProcessUnit *processUnit = (SimulatedProcessUnit *)args;
    SimulatedProcessData *currentProcess = processUnit->processData;
    while (currentProcess->remainingTime > 0) {
        // Se estiver pausado, espera
        P(processUnit->pauseMutex);
        while (processUnit->paused || currentProcess->status != RUNNING) {
            pthread_cond_wait(&processUnit->pauseCond, &processUnit->pauseMutex);
        }
        V(processUnit->pauseMutex);
        
        consume_execution_time(currentProcess);
        
        handle_process_termination_status(currentProcess, processUnit->cpuID);
    }
    pthread_exit(0);
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

void update_process_duration(SimulatedProcessData * process, double elapsedTime){ 
    process->executionTime -= elapsedTime;
    process->remainingTime -= elapsedTime; 
}

//Caso ele seja preemptado, então o seu executionTime é resetado para poder ser comparado depois.
void reset_execution_time(SimulatedProcessData * process){ process->executionTime = process->burstTime; }

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atoi(deadline); }

void set_process_id(SimulatedProcessData * process, int ID){ process->ID = ID; }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime     = atof(burstTime);
    process->remainingTime = atof(burstTime);
    process->executionTime = atof(burstTime);
}

void set_quantum_time(SimulatedProcessData * process, int quantumTime){ process->quantumTime = quantumTime; }

void set_process_priority(SimulatedProcessData * process, int priority){ process->priority = priority; }


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
    int processID = 1;
    char * parsed_data = strtok(line, FILE_SEPARATOR);  
    while (parsed_data != NULL) {
        if (currentParsedData < 4) {
            set_process_values[currentParsedData](process, parsed_data);
        }
        set_process_id(process, processID++);
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

void finish_process(SimulatedProcessData * process, double finishTime){
    char outputLine[MAX_LINE_SIZE];

    //Quase 100% das vezes, arredonda pra baixo, mas só pra garantir né.
    int roundedFinishTime = customRound(finishTime);
    
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

void finish_process_arrival(){
    P(systemQueue.readyQueueMutex);
    systemQueue.allProcessesArrived = true;
    pthread_cond_broadcast(&systemQueue.cond);
    V(systemQueue.readyQueueMutex);
}

void start_simulation_time(){ INITIAL_SIMULATION_TIME = get_current_time(); }

void clean_simulation_memory(SimulatedProcessUnit **processUnities, int numProcesses, CoreQueueManager *systemQueue) {
    for (int i = 0; i < numProcesses; i++) { destroy_process_unit(processUnities[i]); }
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
    this->allProcessesArrived = false;
    this->scheduler = model;
    pthread_mutex_init(&this->readyQueueMutex, NULL);
    pthread_mutex_init(&this->runningQueueMutex, NULL);
    pthread_mutex_init(&this->schedulerMutex, NULL);
    pthread_cond_init(&this->cond, NULL);
    pthread_cond_init(&this->wakeUpScheduler, NULL);
    
    this->idleCores    = calloc(AVAILABLE_CORES, sizeof(bool));
    this->readyQueue   = calloc(maxSize, sizeof(SimulatedProcessUnit *));
    this->runningQueue = calloc(AVAILABLE_CORES, sizeof(SimulatedProcessUnit *));
}

bool is_empty(CoreQueueManager *this){ return this->length == 0; }

void destroy_manager(CoreQueueManager *this) {
    free(this->readyQueue);
    free(this->runningQueue);
    free(this->idleCores);

    pthread_mutex_destroy(&this->readyQueueMutex);
    pthread_mutex_destroy(&this->runningQueueMutex);
    pthread_mutex_destroy(&this->schedulerMutex);
    pthread_cond_destroy(&this->cond);
    pthread_cond_destroy(&this->wakeUpScheduler);
}

void update_priority(SimulatedProcessData *process, bool isInitial) {
    int currentTime = customRound(get_elapsed_time_from(INITIAL_SIMULATION_TIME));
    int overTime = process->deadline - currentTime - process->remainingTime;
    if (overTime < 0) set_process_priority(process, MIN_PRIORITY);
    else {
        if (isInitial) {
            const int PRIORITY_RANGE = get_priority_range();
            if (overTime == 0)                  set_process_priority(process, MAX_PRIORITY);
            else if (overTime > PRIORITY_RANGE) set_process_priority(process, MIN_PRIORITY);
            else                                set_process_priority(process, MAX_PRIORITY + overTime);
        } else {
            process->priority++;
            if (process->priority > MIN_PRIORITY) set_process_priority(process, MIN_PRIORITY);
        }
    }
}

int compareProcessUnits(const SimulatedProcessUnit *p1, const SimulatedProcessUnit *p2, SchedulerModel scheduler) {
    switch(scheduler) {
        case FIRST_COME_FIRST_SERVED:      return p1->processData->arrival - p2->processData->arrival;
        case SHORTEST_REMAINING_TIME_NEXT: return p1->processData->executionTime - p2->processData->executionTime;
        case PRIORITY_SCHEDULING:{
            int priorityDiff = p2->processData->priority - p1->processData->priority;
            return (priorityDiff != 0) ? priorityDiff : (p1->processData->executionTime - p2->processData->executionTime);            
        }
        default: return 0;
    }
}

int circular_index(CoreQueueManager *this, int idx) {
    return (this->front + idx) % this->maxSize;
}

// Função para inserir mantendo a ordenação
// Usa insertionSort, pode não ser o mais eficiente, mas dado que 
// o número de processos é pequeno, ainda é mais eficiente do que o MergeSort.
// Mantendo ainda a estabilidade, o que é importante para este modelo.
void ordered_insert(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    int insertPos = this->length;
    
    for(int i = 0; i < this->length; i++) {
        int idx = circular_index(this, i);
        if(compareProcessUnits(processUnit, this->readyQueue[idx], this->scheduler) < 0) {
            insertPos = i;
            break;
        }
    }
    
    for(int i = this->length; i > insertPos; i--) {
        int destIdx = circular_index(this, i);
        int srcIdx = circular_index(this, i-1);
        this->readyQueue[destIdx] = this->readyQueue[srcIdx];
    }
    
    this->readyQueue[circular_index(this, insertPos)] = processUnit;
    this->length++;
    this->rear = circular_index(this, this->length);
}

void fifo_enqueue(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    this->readyQueue[this->rear] = processUnit;
    this->rear = (this->rear + 1) % this->maxSize;
    this->length++;
}

void enqueue(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    P(this->readyQueueMutex);
    if (this->scheduler == FIRST_COME_FIRST_SERVED) {
        fifo_enqueue(this, processUnit);    
    } else {
        ordered_insert(this, processUnit);
    }
    pthread_cond_signal(&this->cond);
    V(this->readyQueueMutex);
}

SimulatedProcessUnit *dequeue(CoreQueueManager *this) {
    SimulatedProcessUnit *processUnit = NULL;
    P(this->readyQueueMutex);
    
    while (is_empty(this) && !this->allProcessesArrived) {
        pthread_cond_wait(&this->cond, &this->readyQueueMutex);
    }
    
    if (!is_empty(this)) {
        processUnit = this->readyQueue[this->front];
        this->front = (this->front + 1) % this->maxSize;
        this->length--;
    }
    
    V(this->readyQueueMutex);
    return processUnit;
}
