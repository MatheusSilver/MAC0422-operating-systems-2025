#include "ep1.h"

SimulatedProcessUnit **processUnits;
SimulatedProcessData **processList;

CoreQueueManager systemQueue;

FILE *outputFile;

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preemptionCounterMutex = PTHREAD_MUTEX_INITIALIZER;

double INITIAL_SIMULATION_TIME;

unsigned int contextSwitches = 0;

int numProcesses;
int AVAILABLE_CORES;

int main(int args, char* argv[]){
    check_startup(args, argv);
    init(argv);
    exit(0);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DO PROGRAMA <<<<<<<<<<<<<<<<<<<<<<<<< */

void check_startup(int args, char* argv[]){
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
}

void init(char* argv[]){
    AVAILABLE_CORES = sysconf(_SC_NPROCESSORS_ONLN);
    SchedulerModel schedulerID = atoi(argv[1]);
    init_scheduler_simulation(schedulerID, argv[2], argv[3]);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<< */

void init_scheduler_simulation(SchedulerModel model, const char *traceFilename, const char *outputFilename) {
    FILE *traceFile;
    
    
    if (open_file(&traceFile,  (char*)traceFilename, "r"))  exit(-1); 
    processList = malloc(MAX_SIMULATED_PROCESSES * sizeof(SimulatedProcessData*));;
    numProcesses = get_array_of_processes(traceFile, processList);
    close_file(traceFile);
    
    sort(processList, 0, numProcesses-1);
    
    if (open_file(&outputFile, (char*)outputFilename, "w")) exit(-1);    
    
    processUnits = malloc(numProcesses * sizeof(SimulatedProcessUnit*));   
    switch (model) {
        case FIRST_COME_FIRST_SERVED:      FCFS(); break;
        case SHORTEST_REMAINING_TIME_NEXT: SRTN(); break;
        case PRIORITY_SCHEDULING:          PS(); break;
        default: printf("Modelo não configurado\n"); exit(-1);
    }

    close_file(outputFile);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

void FCFS(){
    init_core_manager(&systemQueue, numProcesses, FIRST_COME_FIRST_SERVED);
    // Cria um vetor para armazenar as unidades de processo criadas
    
    // Cria a thread produtora que cria os processos conforme chegarem
    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, NULL);
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
            if (processUnit != NULL) { dispatch_process(processUnit); }
        }    
        V(systemQueue.schedulerMutex);
    }
    
    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

void SRTN() {
    init_core_manager(&systemQueue, numProcesses, SHORTEST_REMAINING_TIME_NEXT);

    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, NULL);
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
            if (!has_available_core()) wait_for_finish_iminence();

            bool shouldPreempt = should_preempt_SRTN();
            bool hasAvailableCore = has_available_core();
            processUnit = dequeue(&systemQueue);
            if (processUnit != NULL) {
                if (shouldPreempt && !hasAvailableCore) {
                    SimulatedProcessUnit *longestRunningProcess = get_longest_running_process(&systemQueue);
                    if (longestRunningProcess != NULL) { preempt_process(longestRunningProcess); }
                }
                dispatch_process(processUnit); 
            }
        }
        V(systemQueue.schedulerMutex);
    }
    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>>>> ESCALONAMENTO POR PRIORIDADE <<<<<<<<<<<<<<<<<<<<<<<<< */

void PS(){
    init_core_manager(&systemQueue, numProcesses, PRIORITY_SCHEDULING);
    
    
    pthread_t creatorThread;
    pthread_create(&creatorThread, NULL, process_creator, NULL);
    start_simulation_time();
    
    while (!systemQueue.allProcessesArrived || !is_empty(&systemQueue) || has_available_core()) {
        SimulatedProcessUnit *processUnit = NULL;
    
        P(systemQueue.runningQueueMutex);
        while (!has_available_core()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);
        
        P(systemQueue.schedulerMutex);
        while (!is_empty(&systemQueue) && (has_available_core())) {
            processUnit = dequeue(&systemQueue);
            if (processUnit != NULL) {
                double processQuantum = processUnit->processData->remainingTime;
                processQuantum = min(processQuantum, get_process_quantum(processUnit->processData->priority));
                set_quantum_time(processUnit->processData, processQuantum);
                dispatch_process(processUnit);
            }
        }    
        V(systemQueue.schedulerMutex);
    }
    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>> CRIADOR DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<<<< */

void* process_creator() {
    int i = 0;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        
        // Suspende até o próximo processo chegar
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);
    
        P(systemQueue.schedulerMutex);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival) {
            // Cria a unidade de processo e armazena no vetor para join posterior
            SimulatedProcessUnit *processUnit = init_process_simulation_unit(processList[i]);
            update_priority(processUnit->processData, true);
            //Define por padrão que o processo poderá rodar o seu burstTime inteiro
            //No caso do PRIORITY_SCHEDULING, o quantumTime é definido dentro da própria thread gerenciadora.
            double processQuantum = processUnit->processData->remainingTime;
            set_quantum_time(processUnit->processData, processQuantum);
            processUnits[i] = processUnit;
            processUnit->cpuID = -1;
            i++;
            pause_process(processUnit);
            pthread_create(&processUnit->threadID, NULL, execute_process, (void *)processUnit);
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

/* >>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<< */

void finish_scheduler_simulation(pthread_t creatorThread) {
    pthread_join(creatorThread, NULL);
    join_simulation_threads(processUnits, numProcesses);
    write_context_switches_quantity();
    clean_simulation_memory(processUnits, numProcesses, &systemQueue); 
}

void pause_process(SimulatedProcessUnit *unit) {
    P(unit->pauseMutex);
    unit->paused = true;
    unit->processData->status = READY;
    V(unit->pauseMutex);
}

void resume_process(SimulatedProcessUnit *unit) {
    P(unit->pauseMutex);
    unit->processData->status = RUNNING;
    unit->paused = false;
    pthread_cond_signal(&unit->pauseCond);
    V(unit->pauseMutex);
}


SimulatedProcessUnit* init_process_simulation_unit(SimulatedProcessData *processData) {
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
    double minRemaining = systemQueue.runningQueue[0]->processData->remainingTime;

    for (int i = 1; i < AVAILABLE_CORES; i++) {
        SimulatedProcessUnit *unit = systemQueue.runningQueue[i];
        if (unit != NULL) {
            double remaining = unit->processData->remainingTime;
            if (remaining < minRemaining && remaining <= 0.25) {
                shortest = unit;
                minRemaining = remaining;
            }
        }
    }

    if (shortest == NULL) return;

    P(systemQueue.runningQueueMutex);
    while (shortest->processData->status == RUNNING) {
        pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
    }
    V(systemQueue.runningQueueMutex);
}

long get_available_core() {
    for (long i = 0; i < AVAILABLE_CORES; i++) {
        if (systemQueue.runningQueue[i] == NULL) { return i; }
    }
    return -1;
}

bool has_available_core() { return get_available_core() != -1; }

void allocate_cpu(pthread_t targetProcess, long chosenCPU) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    
    pthread_setaffinity_np(targetProcess, sizeof(cpu_set_t), &cpuset);
}

void join_simulation_threads(SimulatedProcessUnit **simulationUnits, int numThreads) {
    for (int t = 0; t < numThreads; t++) { pthread_join(simulationUnits[t]->threadID, NULL); }
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
    return MIN_QUANTUM + customRound(angularCoefficient * QUANTUM_RANGE);
}

void handle_process_termination_status(SimulatedProcessUnit *currentUnit) {
    if (currentUnit->processData->remainingTime <= 0) {
        double endTime = get_elapsed_time_from(INITIAL_SIMULATION_TIME);
        finish_process(currentUnit->processData, endTime);
        P(systemQueue.runningQueueMutex);
        systemQueue.runningQueue[currentUnit->cpuID] = NULL;
        pthread_cond_signal(&systemQueue.wakeUpScheduler);   
        V(systemQueue.runningQueueMutex);
    } else {
        currentUnit->processData->numPreemptions++;
        update_priority(currentUnit->processData, false);
        increment_context_switch_counter();
        reset_execution_time(currentUnit->processData);

        if (systemQueue.scheduler == PRIORITY_SCHEDULING) {
            preempt_process(currentUnit);
            pthread_cond_signal(&systemQueue.wakeUpScheduler);
        }
    }
}

void dispatch_process(SimulatedProcessUnit *unit) {
    long cpu = get_available_core();
    unit->cpuID = cpu;
    systemQueue.runningQueue[cpu] = unit;
    allocate_cpu(unit->threadID, cpu);
    resume_process(unit);
}

void preempt_process(SimulatedProcessUnit *unit) {
    long cpu = unit->cpuID;
    pause_process(unit);
    enqueue(&systemQueue, unit);
    systemQueue.runningQueue[cpu] = NULL;
    unit->cpuID = -1;
}

void increment_context_switch_counter(){
    P(preemptionCounterMutex);
    contextSwitches++;
    V(preemptionCounterMutex);
}

void wait_unpause(SimulatedProcessUnit *unit) {
    P(unit->pauseMutex);
    while (unit->paused) {
        pthread_cond_wait(&unit->pauseCond, &unit->pauseMutex);
    }
    V(unit->pauseMutex);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÃO DE ESPERA OCUPADA <<<<<<<<<<<<<<<<<<<<<<<<< */

void *execute_process(void * processToExecute) {
    SimulatedProcessUnit *processUnit = (SimulatedProcessUnit *)processToExecute;
    while (processUnit->processData->remainingTime > 0) {
        wait_unpause(processUnit);
        consume_execution_time(processUnit->processData);
        handle_process_termination_status(processUnit);
        
    }
    pthread_exit(0);
}

void consume_execution_time(SimulatedProcessData *process) { 
    double startTime = get_current_time();
    double lastTime = startTime;
    double elapsedTime = 0;
    double fullExecutionTime = process->quantumTime;

    while (elapsedTime < fullExecutionTime) {
        double currentTime = get_current_time();
        double interval = currentTime - lastTime;
        lastTime = currentTime;
        
        elapsedTime += interval;
        update_process_duration(process, interval);
        
        if (process->status != RUNNING) break;
    }
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<< */


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
bool close_file(FILE *fileDescriptor) { return fclose(fileDescriptor); }

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO STRUCT <<<<<<<<<<<<<<<<<<<<<<<<< */

void set_process_name(SimulatedProcessData *process, const char* name){ strcpy(process->name, name); }

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){ process->arrival = atoi(arrivalTime); }

void update_process_duration(SimulatedProcessData * process, double elapsedTime){ 
    process->executionTime    -= elapsedTime;
    process->remainingTime    -= elapsedTime;
}

//Caso ele seja preemptado, então o seu executionTime é resetado para poder ser comparado depois.
void reset_execution_time(SimulatedProcessData * process){ process->executionTime = process->burstTime; }

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atoi(deadline); }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime     = atof(burstTime);
    process->remainingTime = atof(burstTime);
    process->executionTime = atof(burstTime);
}

void set_quantum_time(SimulatedProcessData * process, double quantumTime){ process->quantumTime = quantumTime; }

void set_process_priority(SimulatedProcessData * process, int priority){ process->priority = priority; }


/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */


//Só pra não precisar adicionar mais uma dependência no código.
int customRound(double value) { return (int)(value + 0.5); }

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

void finish_process(SimulatedProcessData * process, double finishTime){
    process->status = TERMINATED;
    
    char outputLine[MAX_LINE_SIZE];
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
    write_next_line(outputFile, outputLine);
}

void finish_process_arrival(){
    P(systemQueue.readyQueueMutex);
    systemQueue.allProcessesArrived = true;
    pthread_cond_signal(&systemQueue.cond);
    V(systemQueue.readyQueueMutex);
}

void start_simulation_time(){ INITIAL_SIMULATION_TIME = get_current_time(); }

void clean_simulation_memory(SimulatedProcessUnit **processUnities, int numProcesses, CoreQueueManager *systemQueue) {
    for (int i = 0; i < numProcesses; i++) { destroy_process_unit(processUnities[i]); }
    destroy_manager(systemQueue);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> Algoritmos e Estruturas de Dados <<<<<<<<<<<<<<<<<<<<<<<<< */

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> MergeSort <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

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

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Queue Circular <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

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
    
    this->readyQueue   = calloc(maxSize, sizeof(SimulatedProcessUnit *));
    this->runningQueue = calloc(AVAILABLE_CORES, sizeof(SimulatedProcessUnit *));
}

bool is_empty(CoreQueueManager *this){ return this->length == 0; }

void destroy_manager(CoreQueueManager *this) {
    free(this->readyQueue);
    free(this->runningQueue);

    pthread_mutex_destroy(&this->readyQueueMutex);
    pthread_mutex_destroy(&this->runningQueueMutex);
    pthread_mutex_destroy(&this->schedulerMutex);
    pthread_cond_destroy(&this->cond);
    pthread_cond_destroy(&this->wakeUpScheduler);
}

void update_priority(SimulatedProcessData *process, bool isInitial) {
    int currentTime = customRound(get_elapsed_time_from(INITIAL_SIMULATION_TIME));
    int overTime = process->deadline - currentTime - customRound(process->remainingTime);
    if (overTime < 0) set_process_priority(process, MIN_PRIORITY);
    else {
        if (isInitial) {
            const int PRIORITY_RANGE = get_priority_range();
            if (overTime == 0)                  set_process_priority(process, MAX_PRIORITY);
            else if (overTime > PRIORITY_RANGE) set_process_priority(process, MIN_PRIORITY);
            else                                set_process_priority(process, overTime  + MAX_PRIORITY);                             
        } else {
            if (process->numPreemptions >= MAX_PREEMPTIONS_UNTIL_PROMOTION){
                process->priority--;
                process->numPreemptions = 0;
                if (process->priority < MAX_PRIORITY) process->priority = MAX_PRIORITY;
            }
        }
    }
}

int compareProcessUnits(const SimulatedProcessUnit *p1, const SimulatedProcessUnit *p2, SchedulerModel scheduler) {
    switch(scheduler) {
        case FIRST_COME_FIRST_SERVED:      return p1->processData->arrival - p2->processData->arrival;
        case SHORTEST_REMAINING_TIME_NEXT: return customRound(p1->processData->executionTime - p2->processData->executionTime);
        case PRIORITY_SCHEDULING:{
            int priorityDiff = p1->processData->priority - p2->processData->priority ;
            return (priorityDiff != 0) ? priorityDiff : customRound(p1->processData->executionTime - p2->processData->executionTime);            
        }
        default: return 0;
    }
}

int circular_index(CoreQueueManager *this, int index) { return (this->front + index) % this->maxSize; }

void swap(CoreQueueManager *this, int sourceIndex, int targetIndex) { this->readyQueue[targetIndex] = this->readyQueue[sourceIndex]; }

// Função para inserir mantendo a ordenação
// Usa insertionSort, pode não ser o mais eficiente, mas dado que 
// o número de processos é pequeno, ainda é mais eficiente do que o MergeSort.
// Mantendo ainda a estabilidade, o que é importante para este modelo.
void ordered_insert(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    int insertPos = this->length;
    for(int i = 0; i < this->length; i++) {
        int index = circular_index(this, i);
        if(compareProcessUnits(processUnit, this->readyQueue[index], this->scheduler) < 0) {
            insertPos = i;
            break;
        }
    }
    
    for(int i = this->length; i > insertPos; i--) { swap(this, circular_index(this, i-1), circular_index(this, i)); }
    
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
        this->readyQueue[this->front] = NULL;
        this->front = (this->front + 1) % this->maxSize;
        this->length--;
    }
    
    V(this->readyQueueMutex);
    return processUnit;
}

SimulatedProcessUnit* peek_queue_front(CoreQueueManager *this) {
    if (!is_empty(this) && this->readyQueue[this->front] != NULL) {
        return this->readyQueue[this->front];
    }
    return NULL;
}