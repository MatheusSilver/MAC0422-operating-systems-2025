#include "ep1.h"

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

SimulatedProcessUnit **processUnits;
SimulatedProcessData **processList;

CoreQueueManager systemQueue;

FILE *outputFile;

pthread_mutex_t writeFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t preemptionCounterMutex = PTHREAD_MUTEX_INITIALIZER;

double INITIAL_SIMULATION_TIME;

unsigned int contextSwitches = 0;

int numProcesses;

int finishedProcesses = 0;

int AVAILABLE_CORES;

int main(int args, char* argv[]){
    check_startup(args, argv);
    init(argv);
    exit(0);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DO SIMULADOR <<<<<<<<<<<<<<<<<<<<<<<<< */

void check_startup(int args, char* argv[]){
    int schedulerID;
    if (args != 4) {
        printf("Uso: %s <id_escalonador> <arquivo_trace> <arquivo_saida>\n", argv[0]);
        exit(-1);
    }
    schedulerID = atoi(argv[1]);
    if (schedulerID < 1 || schedulerID > 3) {
        printf("Escolha um escalonador válido:\n");
        printf("\t1 = FIRST_COME_FIRST_SERVED\n");
        printf("\t2 = SHORTEST_REMAINING_TIME_NEXT\n");
        printf("\t3 = Escalonamento por Prioridade\n");
        exit(-1);
    }
}

void init(char* argv[]){
    SchedulerModel schedulerID = atoi(argv[1]);

    /* existia uma função de sys/sysinfo.h entretanto, acho que seja preferível usar a syscall diretamente. */
    AVAILABLE_CORES = sysconf(_SC_NPROCESSORS_ONLN);
    init_scheduler_simulation(schedulerID, argv[2], argv[3]);
}

                    /* >>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<< */

/*              A ideia geral do programa girou em torno da interpretação do problema do escalonamento como                 */
/*             uma aplicação do problema dos produtores e consumidores só que com um intermediário, por isso,               */
/*  veja o process_creator como um produtor e todos os demais como consumidores gerenciados pelo modelo de escalonamento    */

void init_scheduler_simulation(SchedulerModel model, const char *traceFilename, const char *outputFilename) {
    FILE *traceFile;
    
    /* Abrimos o arquivo com os dados de entrada, extraidos eles e salvamos em processList, depois fechamos o arquivo.*/
    if (open_file(&traceFile,  (char*)traceFilename, "r"))  exit(-1); 
    processList = malloc(MAX_SIMULATED_PROCESSES * sizeof(SimulatedProcessData*));;
    numProcesses = get_array_of_processes(traceFile, processList);
    close_file(traceFile);
    
    sort(processList, 0, numProcesses-1);
    
    /* O mesmo se aplica aqui, abrimos o arquivo de output, começamos a seção que irá propriamente escrever nele*/
    /* Após a seção ter sido finalizada, significa que nada mais será escrito no arquivo e então o fechamos.    */
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
    pthread_t creatorThread;
    
    /* Este padrão irá se repetir em todos os escalonadores*/
    /* Primeiro, iniciamos o gerenciador de Queue com o modelo que estamos trabalhando. */
    init_core_manager(&systemQueue, numProcesses, FIRST_COME_FIRST_SERVED);

    /* Depois, iniciamos a thread produtora (Ou a que ficará responsável por inserir os processos conforme eles forem chegando)*/
    pthread_create(&creatorThread, NULL, process_creator, NULL);
    start_simulation_time();
    
    /* Para este caso, como nenhum processo será removido da fila */
    /* Escalonamos os processos até que todos os processos tenham sido inseridos e a fila de prontos esteja vazia */
    /* Isto é, todos os processos já rodaram. */
    while (!systemQueue.allProcessesArrived || !all_processes_finished()) {
        SimulatedProcessUnit *processUnit = NULL;
    
        /* Enquanto não houver CPUs disponíveis, suspende o escalonador até que */
        /* Algum processo em execução sinalize que terminou sua execução e liberou a CPU*/
        P(systemQueue.runningQueueMutex);
        while (!has_available_core()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);
        
        /* Evitando que processos sejam inseridos na Queue ao mesmo tempo que são tirados, evitando assim */
        /* Que o escalonador pare se ele tentar procurar um processo momentos antes de o produtor registrar ele.*/
        P(systemQueue.schedulerMutex);
        if (!is_empty(&systemQueue) && has_available_core()) {
            processUnit = dequeue(&systemQueue);
            
            if (processUnit == NULL) break; 
            
            dispatch_process(processUnit); 
        }    
        V(systemQueue.schedulerMutex);
    }
    /* Limpa os recursos consumidos pela simulação e salva o número de preempções do escalonador. */
    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SRTN <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

void SRTN() {
    pthread_t creatorThread;
    bool hasAvailableCore;
    bool shouldPreempt;

    /* Similarmente para o caso do FCFS começamos iniciando os mecanismos utilizados pelo escalonador */
    init_core_manager(&systemQueue, numProcesses, SHORTEST_REMAINING_TIME_NEXT);

    pthread_create(&creatorThread, NULL, process_creator, NULL);
    start_simulation_time();

    while (!systemQueue.allProcessesArrived || !all_processes_finished()) {
        SimulatedProcessUnit *processUnit = NULL;

        P(systemQueue.runningQueueMutex);
        /* Diferentemente do FCFS, o SRTN pode sair deste loop se algum processo terminar sua execução */
        /* OU se o criador de processos indicar que chegou alguém mais curto */
        while (!has_available_core() && !should_preempt_SRTN()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        V(systemQueue.runningQueueMutex);

        P(systemQueue.schedulerMutex);
        /* Com isso, aguardamos o produtor salvar todos os processos que chegaram naquele instante */
        /* E avaliamos se podemos modificar a fila de rodando através de uma preempção, ou se há algum espaço livre. */
        while (!is_empty(&systemQueue) && (should_preempt_SRTN() || has_available_core())) {
            /* Essa condição a mais foi por motivos de que, eventualmente por motivos de concorrência da CPU */
            /* Alguns processos não estavam conseguindo terminar no instante exato, mas sim alguns milésimos de segundo depois*/
            /* Com isso, a função abaixo aguarda estes processos na iminência de terminarem para não fazer uma preempção indevida. */
            shouldPreempt = should_preempt_SRTN();
            hasAvailableCore = has_available_core();
            if (shouldPreempt && !hasAvailableCore) { hasAvailableCore = wait_for_finish_iminence(); }
            processUnit = dequeue(&systemQueue);
            if (processUnit == NULL) break;

            /* Se uma preempção for pedida, mas houver cores livres, então simplesmente inserimos o processo */
            /* Na CPU livre mesmo. */
            P(systemQueue.runningQueueMutex);
            if (!hasAvailableCore && shouldPreempt) {
                SimulatedProcessUnit *longestRunningProcess = get_longest_running_process(&systemQueue);
                if (longestRunningProcess != NULL) { 
                    preempt_process(longestRunningProcess);
                }
            }
            dispatch_process(processUnit); 
            V(systemQueue.runningQueueMutex);
        }
        V(systemQueue.schedulerMutex);
    }
    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>>>> ESCALONAMENTO POR PRIORIDADE <<<<<<<<<<<<<<<<<<<<<<<<< */

void PS(){
    double processQuantum;
    pthread_t creatorThread;

    /* Inicializando assim como nos casos anteriores, os mecanismos de gerenciamento. */
    init_core_manager(&systemQueue, numProcesses, PRIORITY_SCHEDULING);
    pthread_create(&creatorThread, NULL, process_creator, NULL);
    start_simulation_time();

    while (!systemQueue.allProcessesArrived || !all_processes_finished()) {
        SimulatedProcessUnit *processUnit = NULL;
        
        P(systemQueue.runningQueueMutex);
        while (!has_available_core()) {
            pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
        }
        
        P(systemQueue.schedulerMutex);
        /* Similarmente ao FCFS, o Escalonamento por prioridade também espera que o core esteja livre  */
        /* Entretanto, ele define para cada processo, uma quantidade quantums especifica, e o processo */
        /* É executado enquanto houverem quantums para ele, assim que seus quantums acabam, acorda novamente o escalonador. */
        while (!is_empty(&systemQueue) && has_available_core()) {
            processUnit = dequeue(&systemQueue);
            if (processUnit == NULL) break;
            
            processQuantum = processUnit->processData->remainingTime;
            processQuantum = min(processQuantum, get_process_quantum(processUnit->processData->priority));
            set_quantum_time(processUnit->processData, processQuantum);
            dispatch_process(processUnit);
        }    
        V(systemQueue.schedulerMutex);
        V(systemQueue.runningQueueMutex);
    }

    finish_scheduler_simulation(creatorThread);
}

/* >>>>>>>>>>>>>>>>>>>>>> CRIADOR DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<<<< */

void* process_creator() {
    int i = 0;
    double processQuantum;
    while (i < numProcesses) {
        SimulatedProcessData *nextProcess = processList[i];
        
        /* Suspende o produtor até que um novo processo chegue. */
        /* Assim evitando que uma parte do escalonador fique ativo o tempo todo no lugar de algum processo */
        /* Na prática, seria como se eu tivesse colocado uma espécie de cond_wait olhando pra um arquivo de processos chegando. */
        if(get_elapsed_time_from(INITIAL_SIMULATION_TIME) < nextProcess->arrival) suspend_until(nextProcess->arrival);
    
        P(systemQueue.schedulerMutex);
        while (i < numProcesses && get_elapsed_time_from(INITIAL_SIMULATION_TIME) >= processList[i]->arrival) {
            SimulatedProcessUnit *processUnit = init_process_simulation_unit(processList[i]);
            update_priority(processUnit->processData, true);
            /* Define por padrão que o processo poderá rodar o seu burstTime inteiro */
            /* No caso do PRIORITY_SCHEDULING, o quantumTime é definido dentro da própria thread gerenciadora.*/
            processQuantum = processUnit->processData->remainingTime;
            set_quantum_time(processUnit->processData, processQuantum);
            
            /* Armazena a unidade de processo no vetor para join posterior */
            processUnits[i] = processUnit;
            processUnit->cpuID = -1;
            i++;

            /* Inicia o processo pausado, para que ele seja liberado apenas pelo escalonador. */
            pause_process(processUnit);
            pthread_create(&processUnit->threadID, NULL, execute_process, (void *)processUnit);

            /* Adiciona o processo na fila de prontos, para que ele possa ser escalonado. */
            enqueue(&systemQueue, processUnit);
        }
        V(systemQueue.schedulerMutex);

        /*Se for o SRTN acorda o escalonador para verificar se tem algum processo que pode ser preemptado */
        if(systemQueue.scheduler == SHORTEST_REMAINING_TIME_NEXT){
            P(systemQueue.readyQueueMutex);
            pthread_cond_signal(&systemQueue.wakeUpScheduler);
            V(systemQueue.readyQueueMutex);
        }
    }

    finish_process_arrival();
    pthread_exit(0);
}

SimulatedProcessUnit* init_process_simulation_unit(SimulatedProcessData *processData) {
    SimulatedProcessUnit *processUnit = malloc(sizeof(SimulatedProcessUnit));
    pthread_mutex_init(&processUnit->pauseMutex, NULL);
    pthread_cond_init(&processUnit->pauseCond, NULL);
    processUnit->processData = processData;

    /* A ideia aqui é simular o que foi visto em aula (e inclusive simplifica o código), quando um processo é criado */
    /* Ele técnicamente está READY, se ele precisar de entrada, vai pra BLOCKED se não, fica em RUNNING */
    processUnit->processData->status = READY;
    processUnit->cpuID = -1;
    return processUnit;
}

void finish_process_arrival(){
    P(systemQueue.readyQueueMutex);
    systemQueue.allProcessesArrived = true;
    pthread_cond_signal(&systemQueue.cond);
    V(systemQueue.readyQueueMutex);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> EXECUÇÃO DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

void *execute_process(void * processToExecute) {
    SimulatedProcessUnit *processUnit = (SimulatedProcessUnit *)processToExecute;
        /* Enquanto o próprio processo não falar que encerrou, ou então, alguém forçar o término do processo, ele irá continuar rodando. */
                    /* Com isso, ele espera a ordem do Escalonador, depois executa sua função no "consume_execution_time" */
    /* Dependendo, do que ocorrer na sua execução e ele sair, verificamos como proceder com o processo em "handle_process_termination_status" */
    while (processUnit->processData->status != TERMINATED) {
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

    /* Este seria o loop de execução, dizemos que o progresso do processo é dado pela redução ou do seu remainingTime */
        /* Ou do quantum destinado a si próprio, optamos por isso pois não foi bem definida qual seria a operação */
                /* E esta parecia ser a forma mais "natural" de marcar como o processo estava progredindo. */
    while (elapsedTime < fullExecutionTime) {
        double currentTime = get_current_time();
        double interval = currentTime - lastTime;
        lastTime = currentTime;
        
        elapsedTime += interval;
        update_process_duration(process, interval);
        
        if (process->status != RUNNING) break;
    }
}

void handle_process_termination_status(SimulatedProcessUnit *currentUnit) {
    long cpu;
    /* Se o processo sair do seu loop de execução, vemos se foi por zerarmos o tempo restante*/
    /* Ou caso contrário, atualizamos seu status e informamos ao escalonador que o processo foi interrompido. */
    if (currentUnit->processData->remainingTime <= 0) {
        double endTime = get_elapsed_time_from(INITIAL_SIMULATION_TIME);
        finish_process(currentUnit->processData, endTime);
        P(systemQueue.runningQueueMutex);
        cpu = currentUnit->cpuID;
        currentUnit->cpuID = -1;
        if (systemQueue.runningQueue[cpu] == currentUnit) systemQueue.runningQueue[cpu] = NULL;
        pthread_cond_signal(&systemQueue.wakeUpScheduler);   
        finishedProcesses++;
        V(systemQueue.runningQueueMutex);
    } else {
        currentUnit->processData->numPreemptions++;
        update_priority(currentUnit->processData, false);
        increment_context_switch_counter();
        reset_execution_time(currentUnit->processData);

        if (systemQueue.scheduler == PRIORITY_SCHEDULING) {
            P(systemQueue.runningQueueMutex);
            preempt_process(currentUnit);
            pthread_cond_signal(&systemQueue.wakeUpScheduler);
            V(systemQueue.runningQueueMutex);
        }
    }
}

/* >>>>>>>>>>>>>>>>>> UTILIDADES DE GERÊNCIA DE PROCESSOS <<<<<<<<<<<<<<<<<<<<<<< */

void wait_unpause(SimulatedProcessUnit *unit) {
    /* Aguarda que o escalonador sinalize que o processo foi despausado. */
    P(unit->pauseMutex);
    while (unit->paused) {
        pthread_cond_wait(&unit->pauseCond, &unit->pauseMutex);
    }
    V(unit->pauseMutex);
}

void pause_process(SimulatedProcessUnit *unit) {
    /* Sinaliza que o processo deve parar e retornar para o estado de pronto */
    P(unit->pauseMutex);
    unit->paused = true;
    unit->processData->status = READY;
    V(unit->pauseMutex);
}

void resume_process(SimulatedProcessUnit *unit) {
    /* Retorna o processo ao estado de rodando e libera ele da condição de pausa caso esteja lá. */
    P(unit->pauseMutex);
    unit->processData->status = RUNNING;
    unit->paused = false;
    pthread_cond_signal(&unit->pauseCond);
    V(unit->pauseMutex);
}

void dispatch_process(SimulatedProcessUnit *unit) {
    /* Aloca uma CPU e faz o processo ser executado lá. */
    long cpu;
    cpu = get_available_core();
    unit->cpuID = cpu;
    systemQueue.runningQueue[cpu] = unit;    
    allocate_cpu(unit->threadID, cpu);
    resume_process(unit);
}

void preempt_process(SimulatedProcessUnit *unit) {
    /* Interrompe o processo e desvincula o processo da CPU previamente destinada a ele. */
    long cpu = unit->cpuID;
    unit->cpuID = -1;
    pause_process(unit);
    enqueue(&systemQueue, unit);
    /* Teste de sanidade para garantirmos que nenhum outro processo irá tentar limpar outro que não seja si próprio da CPU. */
    /* Em geral não é usado, mas é só uma garantia. */
    if (systemQueue.runningQueue[cpu] == unit) systemQueue.runningQueue[cpu] = NULL;
}

void finish_process(SimulatedProcessData * process, double finishTime){
    /* Atualiza o status do processo e salva seus resultados no arquivo de output */
    
    char outputLine[MAX_LINE_SIZE];
    int roundedFinishTime = customRound(finishTime);
    
    bool deadlineMet = process->deadline >= roundedFinishTime;
    int clockTime = roundedFinishTime - process->arrival;
    snprintf(outputLine, MAX_LINE_SIZE, "%s %d %d %d", process->name, clockTime, roundedFinishTime, deadlineMet);
    
    /* Apenas para garantir que nenhum par de processos tente escrever ao mesmo tempo. */
    P(writeFileMutex);
    write_next_line(outputFile, outputLine);
    V(writeFileMutex);
    process->status = TERMINATED;
}

bool should_preempt_SRTN() {
    SimulatedProcessUnit *newProcess;
    SimulatedProcessUnit *longest;
    newProcess = peek_queue_front(&systemQueue); 
    if (newProcess == NULL) return false;
    
    longest = get_longest_running_process(&systemQueue);
    if (longest == NULL) return false;
    
    /* Se o processo no topo da fila for mais curto que o maior processo rodando na fila, então preemptamos. */
    return newProcess->processData->executionTime < longest->processData->executionTime;
}

SimulatedProcessUnit* get_longest_running_process(CoreQueueManager *this) {
    SimulatedProcessUnit *longest = NULL;
    double largestExecutionTime = -1;
    int i;
    
    for (i = 0; i < AVAILABLE_CORES; i++) {
        if (this->runningQueue[i] != NULL && this->runningQueue[i]->processData->executionTime > largestExecutionTime) {
            longest = this->runningQueue[i];
            largestExecutionTime = longest->processData->executionTime;
        }
    }
    return longest;
}

bool wait_for_finish_iminence() {
    int i;
    SimulatedProcessUnit *shortest = NULL;
    /* Nenhum processo vai ter tempo maior que isso por definição do EP. */
    double minRemaining = 120;

    for (i = 0; i < AVAILABLE_CORES; i++) {
        SimulatedProcessUnit *unit = systemQueue.runningQueue[i];
        if (unit != NULL) {
            double remaining = unit->processData->remainingTime;
            if (remaining < minRemaining && remaining <= 0.25) {
                shortest = unit;
                minRemaining = remaining;
            }
        }
    }

    /* Se nenhum processo estiver com tempo restante menor que 0.25, significa que nenhum processo atual está prestes a terminar. */
    /* Então podemos liberar o escalonador */
    if (shortest == NULL) return false;

    /* Caso contrário, aguardamos um curto período de tempo para que o processo informe que terminou e acorde o escalonador. */
    P(systemQueue.runningQueueMutex);
    while (shortest->processData->status == RUNNING) {
        pthread_cond_wait(&systemQueue.wakeUpScheduler, &systemQueue.runningQueueMutex);
    }
    V(systemQueue.runningQueueMutex);
    return true;
}

bool has_available_core()     { return get_available_core() != -1; }

bool all_processes_finished() { return numProcesses <= finishedProcesses; }

long get_available_core() {
    long i = 0;
    for (i = 0; i < AVAILABLE_CORES; i++) {
        if (systemQueue.runningQueue[i] == NULL) { return i; }
    }
    return -1;
}


void allocate_cpu(pthread_t targetProcess, long chosenCPU) {
    /* Praticamente copiado da Man Page, é a única parte que faz uso de sched.h */
    /* Entretanto, essa parte em tese foi liberada mas fica o aviso. */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(chosenCPU, &cpuset);    
    pthread_setaffinity_np(targetProcess, sizeof(cpu_set_t), &cpuset);
}

void increment_context_switch_counter(){
    P(preemptionCounterMutex);
    contextSwitches++;
    V(preemptionCounterMutex);
}

void join_simulation_threads(SimulatedProcessUnit **simulationUnits, int numThreads) {
    int t = 0;
    for (t = 0; t < numThreads; t++) { pthread_join(simulationUnits[t]->threadID, NULL); }
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DO CORE MANAGER <<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

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

int compareProcessUnits(const SimulatedProcessUnit *p1, const SimulatedProcessUnit *p2, SchedulerModel scheduler) {
    /* FIRST_COME_FIRST_SERVED praticamente não é usado, preferimos simplesmente usar a fifo_enqueue por ser mais rápido. */
    /* Também copiado do livro do SedgeWick e adaptado (até porque o original era em Java) */
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

/* Utilidade única para o insertionSort para adaptar o index dele para uma fila circular. */
int circular_index(CoreQueueManager *this, int index) { return (this->front + index) % this->maxSize; }

void swap(CoreQueueManager *this, int sourceIndex, int targetIndex) { this->readyQueue[targetIndex] = this->readyQueue[sourceIndex]; }

/* Usamos InsertionSort pra manter a estabilidade e também por motivos de ser mais rápido que MergeSort para arrays pequenos. */
void ordered_insert(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    int i;
    int insertPos = this->length;
    for(i = 0; i < this->length; i++) {
        int index = circular_index(this, i);
        if(compareProcessUnits(processUnit, this->readyQueue[index], this->scheduler) < 0) {
            insertPos = i;
            break;
        }
    }
    
    for(i = this->length; i > insertPos; i--) { swap(this, circular_index(this, i-1), circular_index(this, i)); }
    
    this->readyQueue[circular_index(this, insertPos)] = processUnit;
    this->length++;
    this->rear = circular_index(this, this->length);
}

/* Extra pra facilitar e "naturalizar", a inserção de processos pelo FCFS */
void fifo_enqueue(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    this->readyQueue[this->rear] = processUnit;
    this->rear = (this->rear + 1) % this->maxSize;
    this->length++;
}

void enqueue(CoreQueueManager *this, SimulatedProcessUnit *processUnit) {
    P(this->readyQueueMutex);
    if (this->scheduler == FIRST_COME_FIRST_SERVED) fifo_enqueue(this, processUnit);
    else ordered_insert(this, processUnit);
    pthread_cond_signal(&this->cond);
    V(this->readyQueueMutex);
}

SimulatedProcessUnit *dequeue(CoreQueueManager *this) {
    SimulatedProcessUnit *processUnit = NULL;
    P(this->readyQueueMutex);
    
    while (is_empty(this) && !this->allProcessesArrived) { pthread_cond_wait(&this->cond, &this->readyQueueMutex); }
    
    if (!is_empty(this)) {
        processUnit = this->readyQueue[this->front];
        this->readyQueue[this->front] = NULL;
        this->front = (this->front + 1) % this->maxSize;
        this->length--;
    }
    
    V(this->readyQueueMutex);
    return processUnit;
}

/* Retorna o elemento do topo da fila sem removê-lo (também inspirado no livro do SedgeWick) */
SimulatedProcessUnit* peek_queue_front(CoreQueueManager *this) {
    if (!is_empty(this) && this->readyQueue[this->front] != NULL) { return this->readyQueue[this->front]; }
    return NULL;
}

void destroy_manager(CoreQueueManager *this) {
    pthread_mutex_destroy(&this->readyQueueMutex);
    pthread_mutex_destroy(&this->runningQueueMutex);
    pthread_mutex_destroy(&this->schedulerMutex);
    pthread_cond_destroy(&this->cond);
    pthread_cond_destroy(&this->wakeUpScheduler);
    
    free(this->readyQueue);
    free(this->runningQueue);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO STRUCT <<<<<<<<<<<<<<<<<<<<<<<<< */


/* Talvez isso seja bem cursed em C, mas acho que deixa o código mais organizado */
/* Imagina fazer um EP em C++? */
void set_process_name(SimulatedProcessData *process, const char* name){ strcpy(process->name, name); }

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){ process->arrival = atoi(arrivalTime); }

void update_process_duration(SimulatedProcessData * process, double elapsedTime){ 
    process->executionTime    -= elapsedTime;
    process->remainingTime    -= elapsedTime;
    process->quantumTime      -= elapsedTime;
}

/*Caso ele seja preemptado, então o seu executionTime é resetado para poder ser comparado depois assim como visto em aula. */
void reset_execution_time(SimulatedProcessData * process){ process->executionTime = process->burstTime; }

void set_process_deadline(SimulatedProcessData * process, const char * deadline){ process->deadline = atoi(deadline); }

void set_process_burst_time(SimulatedProcessData * process, const char * burstTime){
    process->burstTime     = atof(burstTime);
    process->remainingTime = atof(burstTime);
    process->executionTime = atof(burstTime);
}

void set_quantum_time(SimulatedProcessData * process, double quantumTime){ process->quantumTime = quantumTime; }

void set_process_priority(SimulatedProcessData * process, int priority){ process->priority = priority; }

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
            if (process->numPreemptions >= MAX_PREEMPTIONS_UNTIL_DEMOTION){
                process->priority++;
                process->numPreemptions = 0;
                if (process->priority < MAX_PRIORITY) process->priority = MAX_PRIORITY;
            }
        }
    }
}

int get_process_quantum(int priority){
    const int PRIORITY_RANGE = get_priority_range();
    const int QUANTUM_RANGE  = get_quantum_range();
    double angularCoefficient = 1.0 - ((double)(priority - MAX_PRIORITY) / PRIORITY_RANGE);
    /* Poderiamos ter arredondado, mas o objetivo é dar 3 quantums APENAS para quem realmente precisa */
                    /* Isto é, ou roda o máximo o máximo sem parar, ou não roda */
            /* Por isso, na esmagadora maioria dos casos, os processos recebem 1 ou 2 quantums*/
    return MIN_QUANTUM + (int)(angularCoefficient * QUANTUM_RANGE+0.06);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<< */

bool open_file(FILE **fileDescriptor, char *filepath, char *fileMode) {
    *fileDescriptor = fopen(filepath, fileMode);
    /* Eu não ia colocar os avisos, mas como eu fiquei um bom tempo perdido por ficar digitando errado */
    /* Decidi colocar, vai que isso seja útil para o monitor ou para meu eu do futuro, que digite o arquivo errado. */
    if (*fileDescriptor == NULL) { printf("Erro ao abrir o arquivo %s\n", filepath); }
    return (*fileDescriptor == NULL);
}

bool read_next_line(FILE * fileDescriptor, char* lineBuffer){ return fgets(lineBuffer, MAX_LINE_SIZE, fileDescriptor) != NULL;  }

bool close_file(FILE *fileDescriptor) { return fclose(fileDescriptor); }

bool write_next_line(FILE *fileDescriptor, char *lineBuffer) { return fprintf(fileDescriptor, "%s\n", lineBuffer) >= 0;  }

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
        if (currentParsedData < 4) { set_process_values[currentParsedData](process, parsed_data); }
        
        /* Caso extremamente específico de erro quando o último valor de uma linha de trace é simplesmente " " */
        /* Isto é, colocamos os 3 valores e na hora do último, colocamos apenas um espaço e pulamos uma linha. */
        /* Provavelmente não será avaliado, mas me tomou uns bons 10 minutos achando que meu algoritmo de prioridade estava errado. */
        if (strcmp(parsed_data, "\r\n")==0) return true;
        parsed_data = strtok(NULL, FILE_SEPARATOR);
        currentParsedData++;
    }

    /* Se diferente de, quer dizer que alguma linha tinha ou argumentos de sobra ou em falta. */
    return currentParsedData != 4;
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

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

int customRound(double value) { return (int)(value + 0.5); }

int get_priority_range(){ return MIN_PRIORITY - MAX_PRIORITY; }

int get_quantum_range() { return MAX_QUANTUM - MIN_QUANTUM; }

void start_simulation_time(){ INITIAL_SIMULATION_TIME = get_current_time(); }

void write_context_switches_quantity(){
    char outputLine[MAX_CONTEXT_SWITCH_ORDER];
    snprintf(outputLine, MAX_CONTEXT_SWITCH_ORDER, "%d", contextSwitches);
    write_next_line(outputFile, outputLine);
}

double get_current_time() {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}

void destroy_process_unit(SimulatedProcessUnit *unit) {
    pthread_mutex_destroy(&unit->pauseMutex);
    pthread_cond_destroy(&unit->pauseCond); 
    free(unit->processData);
    free(unit);
}

double get_elapsed_time_from(double startTime) { return get_current_time() - startTime; }

void suspend_until(int nextArrival){
    struct timespec req;
    double targetTime = INITIAL_SIMULATION_TIME + nextArrival;
    req.tv_sec  = (time_t)targetTime;
    req.tv_nsec = (long)((targetTime - req.tv_sec) * 1e9);

    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &req, NULL);
}

/* Escolhemos o MergeSort para garantir que se vários processos chegarem ao mesmo tempo */
/* Poderemos ter alguma espécie de estabilidade e previsibilidade na execução dos processos */
void merge(SimulatedProcessData* arr[], SimulatedProcessData* temp[], int left, int mid, int right) {
    int i = left;
    int j = mid + 1;
    int k = left;
    int l;

    while (i <= mid && j <= right) {
        if (arr[i]->arrival <= arr[j]->arrival) temp[k++] = arr[i++];
        else                                    temp[k++] = arr[j++];
    }

    while (i <= mid) temp[k++] = arr[i++];
    while (j <= right) temp[k++] = arr[j++];

    for (l = left; l <= right; l++) arr[l] = temp[l]; 
}

void mergeSort(SimulatedProcessData* arr[], SimulatedProcessData* temp[], int left, int right) {
    if (left < right) {
        int mid = left + (right - left) / 2;
        mergeSort(arr, temp, left, mid);     
        mergeSort(arr, temp, mid + 1, right);
        merge(arr, temp, left, mid, right);
    }
}

void sort(SimulatedProcessData **arr, int left, int right){
    int size = right - left + 1;
    SimulatedProcessData** temp = malloc(size * sizeof(SimulatedProcessData*));
    mergeSort(arr, temp, left, right);
    free(temp);
}

/* >>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DE LIMPEZA <<<<<<<<<<<<<<<<<<<<<<<< */

void clean_simulation_memory(SimulatedProcessUnit **processUnities, int numProcesses, CoreQueueManager *systemQueue) {
    int i;
    for (i = 0; i < numProcesses; i++) { destroy_process_unit(processUnities[i]); }
    destroy_manager(systemQueue);
}

void finish_scheduler_simulation(pthread_t creatorThread) {
    /* Aguardamos todas as threads de simulação terminarem, depois limpamos as unidades e o gerenciador. */
    pthread_join(creatorThread, NULL);
    join_simulation_threads(processUnits, numProcesses);
    write_context_switches_quantity();
    clean_simulation_memory(processUnits, numProcesses, &systemQueue); 
}