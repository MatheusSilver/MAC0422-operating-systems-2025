#include "ep3.h"

const MemoryStatus FULL  = 000;
const MemoryStatus EMPTY = 255;

int  nextPos = 0; 
int  memoryPositions;

bool isLastCharEnter = true;

void check_startup(int args, char* argv[]){
    if (args != 5) {
        fprintf(stderr, "Uso: %s <algoritmo> <entrada.pgm> <trace.txt> <saida.pgm>\n", argv[0]);
        exit(-1);
    }
    
    int simulatorID = atoi(argv[1]);
    if (simulatorID < 1 || simulatorID > 4) {
        fprintf(stderr, "Escolha um escalonador válido:\n");
        fprintf(stderr, "\t1 = FIRST FIT\n");
        fprintf(stderr, "\t2 = NEXT FIT \n");
        fprintf(stderr, "\t3 = BEST FIT \n");
        fprintf(stderr, "\t4 = WORST FIT\n");
        exit(-1);
    }
}

void init(char * argv[]){
    AllocatorModel allocatorID = atoi(argv[1]);
    FILE * initStateFile;
    char * initStateFilename = argv[2];
    if (open_file(&initStateFile,  initStateFilename, "r"))  exit(-1); 

    FILE * simulatedResultsFile;
    char * simulatedResultsFilename = argv[4];
    if (open_file(&simulatedResultsFile, simulatedResultsFilename, "w+")) exit(-1);

    copy_content(initStateFile, simulatedResultsFile);
    
    close_file(initStateFile);
    isLastCharEnter = verify_last_char_on_file(simulatedResultsFile, '\n');
    memoryPositions = get_memory_slots_quantity(simulatedResultsFile);

    FILE * traceFile;
    char * traceFilename = argv[3];
    if (open_file(&traceFile,  traceFilename, "r"))  exit(-1); 

    simulate(allocatorID, simulatedResultsFile, traceFile);

    close_file(traceFile);
    close_file(simulatedResultsFile);
}

void get_data_from_line(char *readLine, int *lineNumber, char *command){ sscanf(readLine, "%d %s", lineNumber, command); }

bool is_compression_command(char *command){ return strcmp(command, "COMPACTAR") == 0; }

int  get_requested_allocation_units(char *command){ return atoi(command); }

bool allocate_memory(AllocatorModel allocatorID, FILE * memoryFile, int requestedSpace){
    bool allocationSuccess = false; 
    switch(allocatorID){
        case FIRST_FIT: allocationSuccess = first_fit(memoryFile, requestedSpace); break;
        case NEXT_FIT:  allocationSuccess = next_fit (memoryFile, requestedSpace); break;
        case BEST_FIT:  allocationSuccess = best_fit (memoryFile, requestedSpace); break;
        case WORST_FIT: allocationSuccess = worst_fit(memoryFile, requestedSpace); break;
    }
    return allocationSuccess;
}

void register_failure(int lineNumber, int requestedSpace){ printf("%d %d\n", lineNumber, requestedSpace); }

bool get_next_trace_line(FILE * traceFile, char * buffer){ return fgets(buffer, MAX_TRACE_LINE_SIZE, traceFile) != NULL; }

/* Joga fora o começo do PGM que apenas contem dados para a geração das imagens. */
void skip_pgm_header(FILE *memoryFile) {
    char dump[MAX_PGM_LINE_SIZE];
    for (int i = 0; i < PGM_HEADER_SIZE; i++) fgets(dump, MAX_PGM_LINE_SIZE, memoryFile);
}

bool get_next_pgm_status(FILE * memoryFile, MemoryStatus *buffer) { 
    char c;

    do {
        c = fgetc(memoryFile);
        if (c == EOF) return false;
    } while (isspace(c));
    
    int value = 0;
    while (isdigit(c)) {
        value = value * 10 + (c - '0');
        c = fgetc(memoryFile);
    }
    
    if (value != 0 && value != 255) {
        fprintf(stderr, "era esperado 0 ou 255, mas o valor encontrado foi '%d'\n", value);
        exit(-1);
    }

    *buffer = (value == 0 ? FULL : EMPTY);
    return true;
}

void init_next_fit_pointer(FILE *memoryFile) {
    int firstEmptyPos = -1;
    int lastEmptyStart = -1;
    int curPos = 0;
    bool inEmptySequence = false;
    bool readSuccess = false;
    MemoryStatus curStatus;

    reset_pgm_reader(memoryFile);

    while (get_next_pgm_status(memoryFile, &curStatus)) {
        readSuccess = true;

        if (curStatus == EMPTY) {
            if (firstEmptyPos == -1) firstEmptyPos = curPos;

            if (!inEmptySequence) {
                inEmptySequence = true;
                lastEmptyStart = curPos;  // marca o início de uma nova sequência
            }
        } else {
            inEmptySequence = false;
        }

        curPos++;
    }

    if (!readSuccess || firstEmptyPos == -1) {
        nextPos = -1;
        return;
    }

    // Decide se a última posição da memória estava cheia ou fazia parte de uma sequência vazia
    // Se a última posição lida for parte de uma sequência vazia, então `lastEmptyStart` aponta corretamente
    // Caso contrário, voltamos para o primeiro EMPTY encontrado

    nextPos = inEmptySequence ? lastEmptyStart : firstEmptyPos;
    reset_pgm_reader(memoryFile);
    for (int i = 0; i < nextPos; i++) {
        get_next_pgm_status(memoryFile, &curStatus);
    }
}

void reset_pgm_reader(FILE * memoryFile){
    rewind(memoryFile);
    skip_pgm_header(memoryFile);
    fseek(memoryFile, -SPACE_OFFSET, SEEK_CUR);
}

void simulate(AllocatorModel allocatorID, FILE * memoryFile, FILE * traceFile) {
    char traceFileLine[MAX_TRACE_LINE_SIZE];
    char command[MAX_COMMAND_SIZE];
    bool allocationSuccess; 
    int  commandLine;
    int  requestedAllocationUnits;
    int  failedCommands = 0;
    
    rewind(traceFile);
    reset_pgm_reader(memoryFile);
    
    if (allocatorID == NEXT_FIT) init_next_fit_pointer(memoryFile);

    while (get_next_trace_line(traceFile, traceFileLine)) {
        get_data_from_line(traceFileLine, &commandLine, command);
        if (is_compression_command(command)) {
            compress_memory(memoryFile);
            if (allocatorID == NEXT_FIT) init_next_fit_pointer(memoryFile);
        }
        else{
            requestedAllocationUnits = get_requested_allocation_units(command);
            allocationSuccess = allocate_memory(allocatorID, memoryFile, requestedAllocationUnits);
            if (!allocationSuccess) {
                register_failure(commandLine, requestedAllocationUnits);
                failedCommands++;
            }
        }
    } 
    printf("%d\n", failedCommands);
}

int main(int args, char *argv[]) {
    check_startup(args, argv);
    init(argv);
    exit(0);
}

void write_memory_block(FILE* memoryFile, int *initialPos, int allocatedSpace, MemoryStatus value){
    int curSpace = *initialPos;
    for (int i = 0; i < allocatedSpace; i++){
        curSpace % 16 == 0 ? fprintf(memoryFile, "\n%3d", value) : fprintf(memoryFile, " %3d", value);
        curSpace++;
    }
    *initialPos = curSpace;
}

void compress_memory(FILE *memoryFile) {
    int fullCount = 0, total = 0;
    MemoryStatus status;

    reset_pgm_reader(memoryFile);
    while (get_next_pgm_status(memoryFile, &status)) {
        if (status == FULL) fullCount++;
        total++;
    }
    reset_pgm_reader(memoryFile);
    
    int initialPos = 0;
    write_memory_block(memoryFile, &initialPos, fullCount, FULL);
    write_memory_block(memoryFile, &initialPos, total - fullCount, EMPTY);
}


bool isStatusEmpty(MemoryStatus status) { return status == EMPTY; }

bool get_next_empty_block(FILE * memoryFile, int* startPos, int* endPos, int requiredSpace, bool getFullSpace){
    int initPos = * startPos;
    int lastTrackPos = initPos;
    int freeCount = 0;
    MemoryStatus curStatus;

    int pos = initPos;
    while (get_next_pgm_status(memoryFile, &curStatus)) {
        if (isStatusEmpty(curStatus)) {
            if (freeCount == 0) initPos = pos;
            freeCount++;
            if (freeCount == requiredSpace) {
                lastTrackPos = pos + 1;
                break;
            }
        } else {
            freeCount = 0;
        }
        pos++;
    }

    if (freeCount < requiredSpace) return false;

    if (getFullSpace){
        lastTrackPos++;
        while (get_next_pgm_status(memoryFile, &curStatus)){
            if (isStatusEmpty(curStatus)) lastTrackPos++;
            else break;
        }
    }

    *startPos = initPos;
    *endPos = lastTrackPos;
    return true;
}

void move_back_file_descriptor(FILE* memoryFile, int targetPos, int currentPos){
    int backUnits = (currentPos - targetPos)*(MAX_PIXEL_DIGITS + SPACE_OFFSET) + 1;
    if (!isLastCharEnter && currentPos == memoryPositions) backUnits--; 
    //Se o último char não for enter, então o último elemento tem apenas 3 digitos.
    fseek(memoryFile, -backUnits, SEEK_CUR);
}

bool first_fit(FILE *memoryFile, int requestedSpace) {
    int initPos = 0;
    int finalPos = 1;

    reset_pgm_reader(memoryFile);
    bool hasAvailableSpace = get_next_empty_block(memoryFile, &initPos, &finalPos, requestedSpace, false);
    if (hasAvailableSpace){
        move_back_file_descriptor(memoryFile, initPos, finalPos);
        write_memory_block(memoryFile, &initPos, requestedSpace, FULL);
        return true;
    }
    return false;
}

bool next_fit(FILE *memoryFile, int requestedSpace) {
    int initPos = nextPos;
    int finalPos = initPos + 1;

    bool hasAvailableSpace = get_next_empty_block(memoryFile, &initPos, &finalPos, requestedSpace, false);
    if (hasAvailableSpace) {
        move_back_file_descriptor(memoryFile, initPos, finalPos);
        write_memory_block(memoryFile, &initPos, requestedSpace, FULL);
        nextPos = initPos;
        return true;
    }

    // Se não encontrar, tenta do início
    // Neste ponto, praticamente realizamos um first_fit mas recalculando o ponteiro do next fit.
    
    reset_pgm_reader(memoryFile);
    initPos = 0;
    finalPos = 1;

    hasAvailableSpace = get_next_empty_block(memoryFile, &initPos, &finalPos, requestedSpace, false);
    if (hasAvailableSpace) {
        move_back_file_descriptor(memoryFile, initPos, finalPos);
        write_memory_block(memoryFile, &initPos, requestedSpace, FULL);
        nextPos = initPos;
        return true;
    }

    return false;
}

/* Os dois são literalmente a mesma coisa, a única diferença é a ordem que registramos o tamanho. */
/* É tipo aqueles algoritmos de ordenação com um booleano falando se é em ordem crescente ou decrescente. */

bool best_fit(FILE *memoryFile, int requestedSpace) {
    int bestInit = -1;
    /* Inicializamos com o máximo de memória possível */
    /* O objetivo é que ao final, este seja o menor tamanho de menória contiguo. */
    int minFitSize = memoryPositions;

    int startPos = 0;
    int endPos = 1;

    reset_pgm_reader(memoryFile);

    // Tenta encontrar todos os blocos vazios que comportem o requestedSpace
    while (get_next_empty_block(memoryFile, &startPos, &endPos, requestedSpace, true)) {
        int blockSize = endPos - startPos;

        if (blockSize >= requestedSpace && blockSize < minFitSize) {
            minFitSize = blockSize;
            bestInit = startPos;
        }

        startPos = endPos;
    }

    if (bestInit != -1) {
        move_back_file_descriptor(memoryFile, bestInit, memoryPositions);
        write_memory_block(memoryFile, &bestInit, requestedSpace, FULL);
        return true;
    }

    return false;
}

bool worst_fit(FILE *memoryFile, int requestedSpace) {
    int worstInit = -1;
    int maxFitSize = 0;

    int startPos = 0;
    int endPos = 1;

    reset_pgm_reader(memoryFile);

    // Tenta encontrar todos os blocos vazios que comportem o requestedSpace
    while (get_next_empty_block(memoryFile, &startPos, &endPos, requestedSpace, true)) {
        int blockSize = endPos - startPos;

        if (blockSize >= requestedSpace && blockSize > maxFitSize) {
            maxFitSize = blockSize;
            worstInit = startPos;
        }

        startPos = endPos;
    }

    if (worstInit != -1) {
        move_back_file_descriptor(memoryFile, worstInit, memoryPositions);
        write_memory_block(memoryFile, &worstInit, requestedSpace, FULL);
        return true;
    }

    return false;
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DE ARQUIVO <<<<<<<<<<<<<<<<<<<<<<<<< */

bool open_file(FILE **fileDescriptor, char *filepath, char *fileMode) {
    *fileDescriptor = fopen(filepath, fileMode);
    /* Eu não ia colocar os avisos, mas como eu fiquei um bom tempo perdido por ficar digitando errado */
    /* Decidi colocar, vai que isso seja útil para o monitor ou para meu eu do futuro, que digite o arquivo errado. */
    if (*fileDescriptor == NULL) { fprintf(stderr, "Erro ao abrir arquivo '%s' com modo '%s'\n", filepath, fileMode); }
    return (*fileDescriptor == NULL);
}

bool close_file(FILE *fileDescriptor) { return fclose(fileDescriptor); }

void copy_content(FILE *src, FILE * dest){
    int c;
    rewind(src);
    while ((c = fgetc(src)) != EOF) fputc(c, dest); 
}

bool verify_last_char_on_file(FILE* fileDescriptor, char charToVerify){
    fseek(fileDescriptor, -1, SEEK_END);
    return fgetc(fileDescriptor) == charToVerify;
}

int get_memory_slots_quantity(FILE * memoryFile){
    MemoryStatus dump;
    int slotsNumber = 0;
    reset_pgm_reader(memoryFile);
    while (get_next_pgm_status(memoryFile, &dump)) slotsNumber++;
    return slotsNumber;
}