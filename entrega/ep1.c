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

int main(int args, char* argv[]){
    if (args != 2){
        printf("Execute este programa usando %s <nome_do_arquivo_de_trace>\n", argv[0]);
        exit(-1);
    }

    FILE *fileDescriptor;
    char * lineBuffer[MAX_LINE_SIZE];
    SimulatedProcessData * processList[MAX_SIMULATED_PROCESSES];
    int appendedProcesses = 0;

    if (open_file(fileDescriptor, argv[1])){exit(-1);}
    
    while (read_next_line(fileDescriptor, lineBuffer))
    {
        SimulatedProcessData *process;
        if (parse_line_data(lineBuffer, process)){
            printf("Há um erro na linha %d do arquivo de trace", (appendedProcesses+1));
            exit(-1);
        }
        processList[appendedProcesses++] = process;
    }
    


    


}

/*
>>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<<
*/


int open_file(FILE * fileDescriptor, char* filepath){
    fileDescriptor = fopen(filepath, "r");

    if (fileDescriptor == NULL) {
        printf("O arquivo %s não pode ser aberto\n", filepath);
        return -1;
    }

    return 0;
}

int read_next_line(FILE * fileDescriptor, char* lineBuffer){
    if (fgets(lineBuffer, sizeof(lineBuffer), fileDescriptor) == NULL) {
        return -1; // Acabou o arquivo ou algo deu errado.
    }
    
    return 0; // Algo foi lido
}

int close_file(FILE * fileDescriptor){
    if (fclose(fileDescriptor)) {
        printf("Não foi possível fechar o arquivo\n");
        return -1;
    }

    return 0;
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
        set_process_values[currentParsedData++](process, parsed_data);
        parsed_data = strtok(NULL, FILE_SEPARATOR);
    }
    
    return currentParsedData == 4; //Se diferente de 4, então alguma coisa deu bem errado.
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