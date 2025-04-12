#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#define true  1
#define false 0

#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 50
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50

#define FILE_SEPARATOR " "

typedef struct {
    char name[MAX_PROCESS_NAME];
    double arrival;
    double burstTime;
    double remainingTime;
    double deadline;
} SimulatedProcessData;

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

//Essas variáveis são definidas uma vez e usadas apenas para leitura no decorrer do código
int AVAILABLE_CORES;
double INITIAL_SIMULATION_TIME; 

int main(int args, char* argv[]){
    if (args != 2){
        printf("Execute este programa usando %s <escalonador a simular> <nome do arquivo de trace> <nome do arquivo de saida>\n", argv[0]);
        exit(-1);
    }

    AVAILABLE_CORES = get_nprocs();
    FILE *traceFile;
    FILE *outputFile;
    SimulatedProcessData *processList[MAX_SIMULATED_PROCESSES];

    if (open_file(&traceFile, argv[1], "r")){exit(-1);}
    if (open_file(&outputFile, argv[2], "w")){exit(-1);}

    int numProcesses = get_array_of_processes(traceFile, processList);
    
    //mergeSort por tempos de chegada
    mergeSort(processList, 0, numProcesses-1);

    FCFS(processList, numProcesses);
    
    close_file(traceFile);

    exit(0);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> ALGORITMOS DE ESCALONAMENTO <<<<<<<<<<<<<<<<<<<<<<<<<
*/

/*
>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> FCFS <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
*/

void FCFS(SimulatedProcessData * processList, int numProcesses){
    pthread_t * workThreads = malloc(AVAILABLE_CORES * sizeof(pthread_t));
    int *occupiedThreads   = calloc(AVAILABLE_CORES, sizeof(int));
    int occupiedThreadsQtd = 0;

    //Esse é o tempo 0 da Simulação a ser adotado como referência.
    double INITIAL_SIMULATION_TIME = get_current_time();
    //Vai falar qual processo é o próximo a rodar na lista de processos.
    int nextProcessTracker = 0;
    
    while (nextProcessTracker < numProcesses){
        join_threads(workThreads, occupiedThreads, AVAILABLE_CORES, false);
        SimulatedProcessData *nextProcess = processList[nextProcessTracker];
        //Até dá pra travar ele aqui já que em tese, não tem nenhum outro processo pra rodar no momento
        //Se o próximo da fila não estiver pronto, certamente não vai ser o cara depois dele que vai estar.
        while (nextProcess->arrival > get_elapsed_time(initialSimulationTime));
        
        //Espera que haja pelo menos uma thread livre pra poder executar
        while (occupiedThreadsQtd >= AVAILABLE_CORES){ continue; } 

        int threadSlot = find_available_thread(occupiedThreads, AVAILABLE_CORES);
        if (threadSlot != -1){
            pthread_create(&workThreads[threadSlot], NULL, simula_processo_FCFS, nextProcess);
            occupiedThreads[threadSlot] = 1;
            nextProcessTracker++;
        }

    }

    join_threads(workThreads, occupiedThreads, AVAILABLE_CORES, true);
}

void* simula_processo_FCFS(void *arg) {
    SimulatedProcessData *p = (SimulatedProcessData*) arg;
    double endTime = get_current_time() + p->burstTime;
    busy_wait_until(endTime);
    return NULL;
}

int find_available_thread(int *occupiedThreads, int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        if (occupiedThreads[i] == 0) {
            return i;
        }
    }
    return -1; // Nenhuma thread disponível
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


int open_file(FILE **fileDescriptor, char *filepath, char *fileMode) {
    *fileDescriptor = fopen(filepath, fileMode);
    if (*fileDescriptor == NULL) {
        printf("O arquivo %s não pode ser aberto\n", filepath);
        return -1;
    }

    return 0;
}

int read_next_line(FILE * fileDescriptor, char* lineBuffer){
    if (fgets(lineBuffer, MAX_LINE_SIZE, fileDescriptor) == NULL) {
        return 0; // Acabou o arquivo.
    }

    return 1; // Algo foi lido
}

int close_file(FILE * fileDescriptor){
    if (fclose(fileDescriptor)) {
        printf("Não foi possível fechar o arquivo\n");
        return 0;
    }

    return -1;
}

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
    process->remainingTime = atof(burstTime)
}

void set_process_deadline(SimulatedProcessData * process, const char * deadline){
    process->deadline = atof(deadline);
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


double get_elapsed_time(double startTime) { return get_current_time() - startTime; }

int is_running(double startTime, double endTime)  { return get_elapsed_time(startTime) <= endTime; } 




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

    