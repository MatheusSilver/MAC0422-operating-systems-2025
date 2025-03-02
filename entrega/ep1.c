#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#define MAX_PROCESS_NAME 33
#define MAX_LINE_SIZE 50
//Nome = 32 + 3*9 (número de 3 digitos) + 3 espaços + \0 = 45
//Só pra garantir, arrendondamos para 50

#define MAX_SIMULATED_PROCESSES 50

#define FILE_SEPARATOR " "

typedef struct {
    char name[MAX_PROCESS_NAME];
    int arrival;
    int executionTime;
    int deadline;
} SimulatedProcessData;

//Lembrar de organizar isso depois.
int open_file(FILE **, char*, char *);
int read_next_line(FILE *, char*);
int close_file(FILE *);
int parse_line_data(char *, SimulatedProcessData *);
void set_process_name(SimulatedProcessData *, const char *);
void set_process_arrival_time(SimulatedProcessData *, const char *);
void set_process_execution_time(SimulatedProcessData *, const char *);
void set_process_deadline(SimulatedProcessData *, const char *);
int get_array_of_processes(FILE *, SimulatedProcessData*[]);
void mergeSort(SimulatedProcessData *[], int, int);


int main(int args, char* argv[]){
    if (args != 2){
        printf("Execute este programa usando %s <escalonador a simular> <nome do arquivo de trace> <nome do arquivo de saida>\n", argv[0]);
        exit(-1);
    }

    FILE *fileDescriptor;
    SimulatedProcessData *processList[MAX_SIMULATED_PROCESSES];

    if (open_file(&fileDescriptor, argv[1], "r")){exit(-1);}

    int numProcesses = get_array_of_processes(fileDescriptor, processList);
    mergeSort(processList, 0, numProcesses-1); //-1 AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
    
    //Testando, lembrar de tirar depois.
    for (int i = 0; i < numProcesses; i++) {
        printf("%s\n", processList[i]->name);
    }
    
    close_file(fileDescriptor);

    exit(0);
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
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
*/

int parse_line_data(char * line, SimulatedProcessData * process){
    int currentParsedData = 0;

    void (*set_process_values[])(SimulatedProcessData *, const char *) = {
        set_process_name,
        set_process_arrival_time,
        set_process_execution_time,
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


/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO STRUCT <<<<<<<<<<<<<<<<<<<<<<<<<

Vulgo tentativa de lembrar o POO...
*/

void set_process_name(SimulatedProcessData *process, const char* name){
    strcpy(process->name, name);
}

void set_process_arrival_time(SimulatedProcessData * process, const char * arrivalTime){
    process->arrival = atoi(arrivalTime);
}

void set_process_execution_time(SimulatedProcessData * process, const char * executionTime){
    process->executionTime = atoi(executionTime);
}

void set_process_deadline(SimulatedProcessData * process, const char * deadline){
    process->deadline = atoi(deadline);
}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<<
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