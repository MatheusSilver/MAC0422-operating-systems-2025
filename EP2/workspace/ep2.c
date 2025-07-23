#include "ep2.h"

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

Track globalTrack;
Judge judge;
pthread_t *cyclistThreads;
int totalCyclists;
int activeCyclists;
unsigned long long instant;
Cyclist **cyclistArray;
bool isEfficient;
bool isDebug = false;
const int RAND_SEED = 0;
bool finishedSimulation = false;

pthread_barrier_t barrier_depart[MAX_BARRIER_GROUPS];
pthread_barrier_t barrier_arrive[MAX_BARRIER_GROUPS];
pthread_barrier_t barrier_moved[MAX_BARRIER_GROUPS];
pthread_mutex_t *mutex_pos;
pthread_mutex_t mutex_global;


/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DO SIMULADOR <<<<<<<<<<<<<<<<<<<<<<<<< */

int main(int argc, char* argv[]) {
    parse_args(argc, argv);
    simulate();
    return 0;
}

void parse_args(int argc, char *argv[]) {
    if (argc < 4 || argc > 5) {
        fprintf(stderr, "Uso: %s <distancia> <num_ciclistas> <i|e> [-debug]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    globalTrack.length = atoi(argv[1]);
    totalCyclists      = atoi(argv[2]);

    if (strcmp(argv[3], "e") && strcmp(argv[3], "i")){
        fprintf(stderr, "Escolha apenas entre <i> para abordagem ingênua ou <e> para a abordagem eficiente.\n");
        exit(EXIT_FAILURE);
    }
    isEfficient = (argv[3][0] == 'e');

    if (argc == 5 && strcmp(argv[4], "-debug") == 0) isDebug = true;
}

void init(){
    /* Caso tenham algum caso de teste pré definido, é só mudar a RAND_SEED lá em cima, se não deixa rodar no aleatório mesmo. */
    if (RAND_SEED == 0) srand(time(NULL));
    else                srand(RAND_SEED);

    init_structures();
    init_cyclist_threads();

    activeCyclists = totalCyclists;
    instant = 0;
}

void init_structures() {
    globalTrack.lanes = MAX_LANES;
    globalTrack.slots = malloc(globalTrack.length * sizeof(int*));
    for (int i = 0; i < globalTrack.length; i++) {
        globalTrack.slots[i] = malloc(globalTrack.lanes * sizeof(int));
        for (int j = 0; j < globalTrack.lanes; j++) {
            globalTrack.slots[i][j] = EMPTY;
        }
    }

    /* É praticamente IMPOSSÍVEL que a corrida dure tantas voltas assim. */
    /* Mas depois de ter calculado errado o número máximo de processos no EP1, prefiro previnir. */
    judge.lapsQtd = totalCyclists*2;
    judge.currentLastLap = 0;
    judge.laps = malloc(judge.lapsQtd * sizeof(lapController*));

    for (int i = 0; i < judge.lapsQtd; i++) {
        judge.laps[i] = malloc(sizeof(lapController));
        judge.laps[i]->lapCompleteOrder = malloc((totalCyclists+1) * sizeof(Cyclist *));
        judge.laps[i]->cyclistsOnLap = totalCyclists;
        judge.laps[i]->defined = false;
        judge.laps[i]->eliminatedCyclist = NULL;
        judge.laps[i]->nextAvailablePositon = 0;
    }

    for (int i = 0; i < MAX_BARRIER_GROUPS; i++){
        pthread_barrier_init(&barrier_depart[i], NULL, totalCyclists + 1);
        pthread_barrier_init(&barrier_arrive[i], NULL, totalCyclists + 1);
        pthread_barrier_init(&barrier_moved[i], NULL, totalCyclists);
    }

    if (isEfficient) {
        mutex_pos = malloc(globalTrack.length * sizeof(pthread_mutex_t));
        for (int i = 0; i < globalTrack.length; i++) pthread_mutex_init(&mutex_pos[i], NULL);
    } else {
        pthread_mutex_init(&mutex_global, NULL);
    }

    cyclistThreads = malloc((totalCyclists + 1) * sizeof(pthread_t));
}

void init_cyclist_threads(){
    cyclistArray = malloc((totalCyclists+1) * sizeof(Cyclist *));
    int *idArray = malloc(totalCyclists * sizeof(int));
    /* A ideia é similar ao que o Oshiro (Eu acho...) passou em TecProg. */
                    /* Crie uma array de elementos unicos */
            /* Dê shuffle, e você terá uma array aleatória única.*/
               /* Não ironicamente eu copiei esse código de lá*/

    get_random_array(idArray, totalCyclists);
    for (int i = 0; i < totalCyclists; i++) {
        int x = i / 5;
        int l = i % 5;
        
        globalTrack.slots[x][l] = idArray[i];
        Cyclist *cyclist = malloc(sizeof(Cyclist));
        init_cyclist(cyclist, idArray[i], x, l);
        /* Pra manter o acesso ordenado, salvamos os ciclistas em ordem de ID, mas colocamos sua posição aleatóriamente na pista. */
        cyclistArray[idArray[i]] = cyclist;
        pthread_create(&cyclistThreads[idArray[i]], NULL, cyclist_simulation, cyclist);
    }
    free(idArray);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> ENTIDADE CENTRAL <<<<<<<<<<<<<<<<<<<<<<<<< */

void simulate(){
    init();

    bool shouldUpdateSync = false;
    int update_barriers = 0;
    int prevActive = activeCyclists;

    if (isDebug) print_full_track();

    while (activeCyclists >= 2) {
        /* Faça todos os ciclistas andarem um passo de forma concorrente. */
        sinc_in_barrier(barrier_depart, (instant % 2));

        if (shouldUpdateSync){
            rebuild_barrier(barrier_arrive, activeCyclists+1);
            shouldUpdateSync = false;
        }

        sinc_in_barrier(barrier_arrive, (instant % 2));
        /* Destrua todos os ciclistas que precisarem ser destruídos. */
        destroy_cyclists();
        
        /* Avance o relógio em 60ms */
        advance_time();

        /* Verifica se houve uma mudança no número de ciclistas para reconstruir as barreiras */
        /* Perceba que os ciclistas finalizados nesta rodada ainda rodam em mais uma barreira antes de sairem */
        if (activeCyclists != prevActive) {
            update_barriers = 2;
            prevActive = activeCyclists;
        }
        if (update_barriers > 0){
            rebuild_barrier(barrier_moved, activeCyclists);
            rebuild_barrier(barrier_depart, activeCyclists+1);
            shouldUpdateSync = true;
            update_barriers--;
        }

                    /* Determina o vencedor quando houver */
        /* Única diferença do algoritmo genérico passado no enunciado. */
        if (activeCyclists == 1) {
            for (int i = 1; i <= totalCyclists; i++){
                if (!cyclistArray[i]->eliminationVerified){
                    judge.winnerID = cyclistArray[i]->id;
                    break;
                }
            }
            finishedSimulation = true;
            sinc_in_barrier(barrier_depart, (instant % 2));
        }

        
        /* Imprima as informações na tela. */
        if (isDebug && activeCyclists >= 2) print_full_track();
        
        if (judge.laps[judge.currentLastLap]->cyclistsOnLap <= 0 && !isDebug){
            print_finishing_list();
            judge.currentLastLap++;
        } 
    }

    finish_simulation();
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILITÁRIOS DA ENTIDADE CENTRAL <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Honestamente, essa foi a parte mais chata do EP... */
/* E no fim a chatice foi toda pra arrumar o problema da eliminação tardia que ocorre só MUITO raramente. */
void destroy_cyclists(){
    /* Passo 1: atualizar os dados em cada uma das pistas finalizadas pelos ciclistas. */
    update_finished_laps();

    /* Passo 2: utilizar os dados atualizados para definir os eliminados da rodada. */
    int eliminatedCount = 0;
    Cyclist ** eliminationCandidates = get_elimination_candidates(&eliminatedCount);

    /* Passo 3.1: Verificar se na volta crítica atual, houve algum quebrado, se houver, então o eliminado da sprint é o quebrado */
    /* Só podemos dizer que o último quebrou. Se o último tiver quebrado numa volta de eliminação, então o eliminado é o que quebrou */
    /* Não faria sentido eliminar um ciclista correndo quando um "quebrado" já saiu por ele. */
    check_broke_candidate(eliminationCandidates, eliminatedCount);
    
    /* Passo 3.2: Se a volta crítica ainda não foi definida, é porque não houveram quebrados nesta volta, então devemos */
    /* Eliminar algum ciclista sorteando aleatoriamente dentro do vetor eliminationCandidates */
    if (!judge.laps[judge.currentLastLap]->defined && eliminatedCount > 0) {
        assign_random_elimination(eliminationCandidates, eliminatedCount);
    }
    free(eliminationCandidates);

    /* Passo 4: Propriamente eliminar quem precisa ser eliminado. */
    perform_eliminations();
}

/* Praticamente toda a mecânica de eliminação. */
/* Primeiramente escrevi o destroy como uma função gigante depois sai quebrando ela em pequenos módulos. */
void update_finished_laps() {
    for (int lane = 0; lane < globalTrack.lanes; lane++) {
        int id = get_track_content(0, lane);
        if (id == EMPTY) continue;
        Cyclist *avaliatedCyclist = get_cyclist_by_id(id);
        if (!avaliatedCyclist || !avaliatedCyclist->finishedLap) continue;

        int completedLap = avaliatedCyclist->currentLap - 1;
        if (avaliatedCyclist->status == BROKEN) {
            for (int j = completedLap; j < judge.lapsQtd; j++) judge.laps[j]->cyclistsOnLap--;
        } else if (avaliatedCyclist->status == RUNNING) {
            judge.laps[completedLap]->cyclistsOnLap--;
        }
        append_cyclist_on_order_list(avaliatedCyclist, completedLap);
    }
}

Cyclist** get_elimination_candidates(int *eliminatedCount) {
    Cyclist **candidates = malloc(sizeof(Cyclist*) * globalTrack.lanes);
    int count = 0;
    for (int lane = 0; lane < globalTrack.lanes; lane++) {
        int id = get_track_content(0, lane);
        if (id == EMPTY) continue;
        Cyclist *avaliatedCyclist = get_cyclist_by_id(id);
        if (!avaliatedCyclist || !avaliatedCyclist->finishedLap) continue;

        int completedLap = avaliatedCyclist->currentLap - 1;
        if ((completedLap % 2 == 1) && judge.laps[completedLap]->cyclistsOnLap == 0) {
            candidates[count++] = avaliatedCyclist;
        }
    }
    *eliminatedCount = count;
    return candidates;
}

void check_broke_candidate(Cyclist **candidates, int count) {
    for (int i = 0; i < count; i++) {
        Cyclist *avaliatedCyclist = candidates[i];
        if (!avaliatedCyclist->finishedLap) continue;
        int completedLap = avaliatedCyclist->currentLap - 1;
        if (avaliatedCyclist->status == BROKEN && (completedLap % 2 == 1) && !judge.laps[completedLap]->defined) {
            judge.laps[completedLap]->eliminatedCyclist = avaliatedCyclist;
            judge.laps[completedLap]->defined = true;
            return;
        }
    }
}

void assign_random_elimination(Cyclist **candidates, int count) {
    int eliminationIndex = ((int)get_normalized_probability()) % count;
    Cyclist *eliminatedCyclist = candidates[eliminationIndex];
    eliminatedCyclist->status = ELIMINATED;
    int lap = eliminatedCyclist->currentLap - 1;
    judge.laps[lap]->eliminatedCyclist = eliminatedCyclist;
    judge.laps[lap]->defined = true;

    for (int j = lap; j < judge.lapsQtd; j++) judge.laps[j]->cyclistsOnLap--;
}

void perform_eliminations() {
    for (int lane = 0; lane < globalTrack.lanes; lane++) {
        int id = get_track_content(0, lane);
        if (id == EMPTY) continue;
        Cyclist *avaliatedCyclist = cyclistArray[id];
        if (avaliatedCyclist->status == RUNNING || avaliatedCyclist->eliminationVerified) continue;

        P(avaliatedCyclist->accessMutex);
        while (!avaliatedCyclist->ready) {
            pthread_cond_wait(&avaliatedCyclist->updatedCond, &avaliatedCyclist->accessMutex);
        }
        avaliatedCyclist->eliminationVerified = true;
        V(avaliatedCyclist->accessMutex);
        activeCyclists--;
    }
}

void append_cyclist_on_order_list(Cyclist * cyclist, int lapIndex){
    int nextPos = judge.laps[lapIndex]->nextAvailablePositon;
    judge.laps[lapIndex]->lapCompleteOrder[nextPos] = cyclist;
    judge.laps[lapIndex]->nextAvailablePositon++;
}

void rebuild_barrier(pthread_barrier_t * barrier, int size){
    int barrierID = (instant + 1) % 2;
    pthread_barrier_destroy(&barrier[barrierID]);
    pthread_barrier_init(&barrier[barrierID], NULL, size);
}

void advance_time(){ instant++; }

void finish_simulation(){
    join_cyclist_threads();
    print_full_report(cyclistArray, totalCyclists);
    clean_simulation_memory();
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> IMPRESSÃO DOS RESULTADOS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* O objetivo é imprimir primeiro os ciclistas eliminados (ou que encerraram a prova) em ordem do pódio. */
/* E em seguida, imprimimos todos os ciclistas que quebraram no meio do caminho do último a quebrar ao primeiro. */
void print_full_report(Cyclist **cyclistArray, int totalCyclists) {
    Cyclist **brokenCyclists      = malloc(totalCyclists * sizeof *brokenCyclists);
    Cyclist **finishedCyclists    = malloc(totalCyclists * sizeof *finishedCyclists);
    int brokenCount   = 0;
    int finisherCount = 0;

    for (int i = 1; i <= totalCyclists; i++) {
        Cyclist *avaliatedCyclist = cyclistArray[i];
        if (avaliatedCyclist->status == BROKEN) brokenCyclists[brokenCount++] = avaliatedCyclist;
        else                                    finishedCyclists[finisherCount++] = avaliatedCyclist;
    }

    /* Sugestão recebida do Lucas, quando perguntei se precisava implementar meu próprio sort */
        /* No EP1 tinha motivo pra eu querer o Merge, já aqui... Qualquer um resolve. */
    qsort(finishedCyclists, finisherCount, sizeof *finishedCyclists, decreasing_order_if_finish_time);
    qsort(brokenCyclists, brokenCount,     sizeof *brokenCyclists,   decreasing_order_of_broken_lap);

    printf("Ranqueamento Final (Tempo em ms):\n\n");
    for (int i = 0; i < finisherCount; i++) {
        Cyclist *finishedCyclist = finishedCyclists[i];
        printf("%3dº lugar: Ciclista %3d: %llums\n", i+1, finishedCyclist->id, finishedCyclist->finishTime);
    }
    for (int i = 0; i < brokenCount; i++) {
        Cyclist *brokenCyclist = brokenCyclists[i];
        printf("Ciclista %3d: QUEBROU na volta %3d\n", brokenCyclist->id, brokenCyclist->currentLap);
    }

    free(brokenCyclists);
    free(finishedCyclists);
}

void print_full_track(){
    fprintf(stderr, "Estado da pista (tempo %llu ms):\n", instant*60);
    print_lane_border();
    for (int i = globalTrack.lanes-1; i >= 0; i--) {
        for (int j = globalTrack.length-1; j >= 0; j--) {
            int positionID = globalTrack.slots[j][i];
            if (positionID == EMPTY) fprintf(stderr, "%3c ", '.');
            else                     fprintf(stderr, "%3d ", positionID);
        }
        fprintf(stderr, "\n");
    }
    print_lane_border();
}

void print_finishing_list(){
    int lapIndex = judge.currentLastLap;
    /* No enunciado não é feita distinção entre quebrados ou não, então imprimimos todos que completaram a volta. */
    printf("\n-------------------------------------------------\n");
    printf("ORDEM DE FINALIZAÇÃO DOS CICLISTAS NA VOLTA: %3d", lapIndex+1);
    printf("\n-------------------------------------------------\n");
    for (int i = 0; i < judge.laps[lapIndex]->nextAvailablePositon; i++){
        if (judge.laps[lapIndex]->lapCompleteOrder[i] == NULL) break;
        Cyclist * cyclistOnPos = judge.laps[lapIndex]->lapCompleteOrder[i];
        printf("%3dº lugar: Ciclista %3d\n", i + 1, cyclistOnPos->id);
    }
    printf("-------------------------------------------------\n");
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> CICLISTAS <<<<<<<<<<<<<<<<<<<<<<<<< */

void *cyclist_simulation(void *arg) {
    Cyclist *self = (Cyclist*)arg;
    
    while (!self->eliminationVerified) {
        /* Antes de caminhar, marcamos que ele não caminhou ainda. */
        reset_cyclist_atributtes(self);
        
        P(self->accessMutex);
        self->ready = true;
        pthread_cond_signal(&self->updatedCond);
        V(self->accessMutex);

        sinc_in_barrier(barrier_depart, self->round);
        /* Variável readOnly pra evitar deadlock ao final, ou então muitas passagens em uma lista. */
        /* Não é bem readonly, na prática, ela é alterada uma vez bem depois quando já decidimos o vencedor. */
        if (finishedSimulation) break;
        self->finishedLap = false;
        if (self->status == RUNNING) {
            move(self);

            /* Espera todos terem terminado de andar, pra frente pra depois alocar eles pra baixo. */
            sinc_in_barrier(barrier_moved, self->round);
            if (self->failedToMove) failed_to_move_ahead(self);

            /* Perceba que só os ciclistas esperam aqui. */
             /* Depois tenta ir pra faixa mais interna. */
            recalculate_lane(self);

            if (self->finishedLap) {
                self->currentLap++;
                decide_next_speed(self, self->speed);
                decide_continuity(self);
            }
        }else{
            remove_position_on_track(self, self->xPos, self->lane);
            signal_move_complete(self);
            sinc_in_barrier(barrier_moved, self->round);
            signal_relocation_complete(self);
        }
        self->ready = false;
        sinc_in_barrier(barrier_arrive, self->round);
        self->round = !self->round;
    }
    /* Em teoria os dois finalistas são encerrados ao mesmo tempo, então em tese ambos teriam o mesmo tempo na pista. */
    /* Essa condição é só pra deixar ele com algum tempo a mais. para diferenciar o primeiro do segundo. */
    if (self->id == judge.winnerID) self->finishTime = instant*60;
    else                            self->finishTime = (instant-1)*60;
    /* -1 porque ele só sai do loop uma sincronização após ser eliminado. */
    
    pthread_exit(NULL);
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO CICLISTA <<<<<<<<<<<<<<<<<<<<<<<<< */

void init_cyclist(Cyclist *self, int id, int xPos, int lane){
    self->id    = id;
    self->finishedLap = false;
    self->xPos  = xPos;
    self->lane  = lane;
    self->speed = 30;
    self->currentLap = 0;
    self->round = 0;
    self->eliminationVerified = false;
    self->status = RUNNING;
    self->speedMoment = false;
    self->moved = false;
    self->relocated = false;
    self->failedToMove = false;
    self->ready = false;
    self->isSomeoneWaiting = false;

    pthread_cond_init(&self->updatedCond, NULL);
    pthread_cond_init(&self->globalRelocationCond, NULL);
    pthread_mutex_init(&self->accessMutex, NULL);
}

void add_cyclist_on_track(Cyclist * cyclist){ globalTrack.slots[cyclist->xPos][cyclist->lane] = cyclist->id; }

void remove_position_on_track(Cyclist * self, int xPos, int lane){  if (globalTrack.slots[xPos][lane] == self->id) globalTrack.slots[xPos][lane] = EMPTY; }

void update_position_on_track(Cyclist *cyclist, int prevPos, int prevLane){ 
    remove_position_on_track(cyclist, prevPos, prevLane);
    add_cyclist_on_track(cyclist);
}

void update_position_on_lane(Cyclist *cyclist, int prevLane){ 
    remove_position_on_track(cyclist, cyclist->xPos, prevLane);
    add_cyclist_on_track(cyclist);
}


void decide_next_speed(Cyclist * self, int currentSpeed){
    if      (currentSpeed == 30) self->speed = (verify_event_probability(75.0)) ? 60 : 30;  
    else if (currentSpeed == 60) self->speed = (verify_event_probability(45.0)) ? 60 : 30;
}

void report_broke(Cyclist * self){ if (!isDebug) printf("Ciclista %3d: QUEBROU na volta %3d\n", self->id, self->currentLap); }

void decide_continuity(Cyclist * self){
    if (self->currentLap % 5 == 0 && verify_event_probability(10.0)) {
        self->status = BROKEN;
        report_broke(self);
    }
}

void recalculate_position(Cyclist * self){
    int newPos = (self->xPos + 1) % globalTrack.length;
    if (self->speed == 60 || (self->speed == 30 && self->speedMoment)) {
        self->finishedLap = (newPos < self->xPos);
        self->xPos = newPos;
        self->speedMoment = false;
    } else {
        self->speedMoment = true;
    }
}

void lock_track_mutex(int prevPos, int nextPos){
    if (isEfficient) {
        int primaryIndex   = (prevPos != 0) ? nextPos : prevPos;
        int secondaryIndex = (prevPos == 0) ? nextPos : prevPos;
        P(mutex_pos[primaryIndex]);
        P(mutex_pos[secondaryIndex]);
    } else {
        P(mutex_global);
    }
}

void unlock_track_mutex(int prevPos, int nextPos){
    if (isEfficient) {
        int primaryIndex   = (prevPos != 0) ? nextPos : prevPos;
        int secondaryIndex = (prevPos == 0) ? nextPos : prevPos;
        V(mutex_pos[primaryIndex]);
        V(mutex_pos[secondaryIndex]);
    } else {
        V(mutex_global);
    }
}

Cyclist * get_cyclist_by_id(int id){ return cyclistArray[id]; }

void lock_lane(int curLane){
    if (isEfficient) P(mutex_pos[curLane]);
    else             P(mutex_global);
}

void unlock_lane(int curLane){
    if (isEfficient) V(mutex_pos[curLane]);
    else             V(mutex_global);
}

int getPrevPosition(int xPos){
    if (xPos - 1 < 0) return xPos = globalTrack.length - 1;
    else              return xPos - 1;
}

void wait_for_cyclist_on_top(int nextPos){
    for (int i = globalTrack.lanes-1; i >= 0; i--){
        int blocker = get_track_content(nextPos, i);
        if (blocker != EMPTY){
            Cyclist * topCyclist = get_cyclist_by_id(blocker);
            if (topCyclist->relocated) break;
            P(topCyclist->accessMutex);
            if (!topCyclist->relocated){
                while (!topCyclist->relocated){
                    pthread_cond_wait(&topCyclist->globalRelocationCond, &topCyclist->accessMutex);
                }
            }
            V(topCyclist->accessMutex);
            i=globalTrack.lanes-1; /* Verifica de novo para o caso de algum dos ciclistas que vão receber a segunda chance, terem ido para a próxima faixa. */
        }
    }
}

void failed_to_move_ahead(Cyclist * self){
    int prevPos = getPrevPosition(self->xPos);
    int nextPos = self->xPos;
    int tempLane = self->lane;
    self->lane--;
    lock_track_mutex(prevPos, nextPos);
    /* Esperamos que a relocação na coluna seguinte esteja completa antes de tentar movimentar. */
    if (!nextLaneReady(nextPos)){
        unlock_track_mutex(prevPos, nextPos);
        wait_for_cyclist_on_top(nextPos);
        lock_track_mutex(prevPos, nextPos);
    }
    if (can_overtake(self, prevPos, nextPos, true)){
        update_position_on_track(self, prevPos, tempLane);
    }else{
        self->lane = tempLane;
        int tempSpeed = self->speed;
        self->xPos = prevPos;
        self->speed = 30;
        /* Só pra garantir que ele não vai tentar andar uma terceira vez. */
        if (self->speedMoment) self->speedMoment = false;
        recalculate_position(self);
        self->speed = tempSpeed;
    }
    unlock_track_mutex(prevPos, nextPos);
}

int get_track_content(int xPos, int lane){ return globalTrack.slots[xPos][lane]; }

void wait_partner_to_go_down(int id, int prevPos, int nextPos){
    Cyclist * analisedPartner = get_cyclist_by_id(id);
    P(analisedPartner->accessMutex);
    if (!analisedPartner->relocated && can_wait_for(analisedPartner)){
        analisedPartner->isSomeoneWaiting = true;
        /* A ideia aqui é saber se estamos avaliando uma relocação simples ou uma segunda chance */
        /* Caso prevPos != nextPos então é uma segunda chance, e por isso, liberamos o mutex de movimento */
        if (prevPos != nextPos) unlock_track_mutex(prevPos, nextPos);
        else                    unlock_lane(prevPos);

        while (!analisedPartner->relocated){
            pthread_cond_wait(&analisedPartner->updatedCond, &analisedPartner->accessMutex);
        }
        if (prevPos != nextPos) lock_track_mutex(prevPos, nextPos);
        else                    lock_lane(prevPos);
        analisedPartner->isSomeoneWaiting = false;
    }
    V(analisedPartner->accessMutex);
}

void wait_partner_to_move(int id, int prevPos, int nextPos){
    Cyclist * analisedPartner = get_cyclist_by_id(id);
    P(analisedPartner->accessMutex);
    if (!alreadyMoved(analisedPartner) && can_wait_for(analisedPartner)){
        analisedPartner->isSomeoneWaiting = true;
        unlock_track_mutex(prevPos, nextPos);
        while (!alreadyMoved(analisedPartner)){
            pthread_cond_wait(&analisedPartner->updatedCond, &analisedPartner->accessMutex);
        }
        lock_track_mutex(prevPos, nextPos);
        analisedPartner->isSomeoneWaiting = false;
    }
    V(analisedPartner->accessMutex);
}

bool alreadyRelocated(Cyclist * cyclist){ return cyclist->relocated; }

bool alreadyMoved(Cyclist * cyclist){ return cyclist->moved; }

bool can_wait_for(Cyclist * cyclist){ return !cyclist->isSomeoneWaiting; }

bool nextLaneReady(int nextPos){
    for (int i = 0; i < globalTrack.lanes; i++) {
        int blocker = get_track_content(nextPos, i);
        if (blocker == EMPTY) continue;
        else{
            Cyclist * cyclist = get_cyclist_by_id(blocker);
            if (!alreadyRelocated(cyclist)) return false;
        }
    }
    return true;
}

bool can_overtake(Cyclist *self, int prevPos, int nextPos, bool waitForRelocation) {
    int curLane = self->lane;
    int newLane = -1;

    while (newLane == -1){
        int targetLane = curLane + 1;
        int blocker = get_track_content(nextPos,targetLane);
        if (targetLane >= globalTrack.lanes) break;
        
        if (targetLane < globalTrack.lanes){
            if (blocker != EMPTY){
                Cyclist * blockerCyclist = get_cyclist_by_id(blocker);
                bool alreadyUpdated = waitForRelocation ? alreadyRelocated(blockerCyclist) : alreadyMoved(blockerCyclist);
                bool canWait = can_wait_for(blockerCyclist);
                
                if (!alreadyUpdated && canWait){
                    waitForRelocation ? wait_partner_to_go_down(blocker, prevPos, nextPos) : wait_partner_to_move(blocker, prevPos, nextPos);
                    continue;
                    /* Perdi 2 dias só pra descobrir que se um cara está esperando uma posição ou já se moveu, os outros deveriam simplesmente tentar outras posições...*/
                }else{ /* Na prática só entramos aqui se ele já atualizou ou se não pode esperar. */
                    curLane +=1;
                    continue;
                }
            }else{
                newLane = targetLane;
                break;
            }
        }else{
            newLane = -1;
            break;
        }
    }
    
    if (newLane != -1) {
        self->lane = newLane;
        return true;
    }
    return false;
}

void recalculate_lane(Cyclist * self){
    /* Não tem motivo pra descer se o cara já estiver lá na faixa mais interna. */
    if (self->lane == 0){
        signal_relocation_complete(self);
        return;
    }
    int curPos = self->xPos;
    lock_lane(curPos);
    while (self->lane > 0){
        int targetLane = self->lane-1;
        int blocker = get_track_content(curPos,targetLane);
        if (blocker != EMPTY){
            Cyclist * blockerCyclist = get_cyclist_by_id(blocker);
            if (!alreadyRelocated(blockerCyclist)){
                wait_partner_to_go_down(blocker, curPos, curPos);
                continue;
            }else{
                break;
            }
        }
        remove_position_on_track(self, curPos, self->lane);
        self->lane = targetLane;
        add_cyclist_on_track(self);
    }
    unlock_lane(curPos);
    signal_relocation_complete(self);
}

/* Vamos separar por casos, primeiro, se o ciclista só for atualizar o momentum, então ele faz isso e está pronto. */
/* Caso o ciclista precise ultrapassar, então ele irá segurar uma faixa e na próxima, irá tentar procurar uma faixa livre. */
void move(Cyclist * self){
    int prevPos  = self->xPos;
    int prevLane = self->lane;
    recalculate_position(self);
    int nextPos = self->xPos;
    /* O ciclista quer acessar uma nova posição, precisamos verificar se ele pode fazer isso de fato. */
    if (prevPos != nextPos){
        lock_track_mutex(prevPos, nextPos);
        if (can_overtake(self, prevPos, nextPos, false)){
            update_position_on_track(self, prevPos, prevLane);
        }else{
            /* Marcando a posição da segunda tentativa. */
            self->xPos = nextPos;
            self->failedToMove = true;
        }
        unlock_track_mutex(prevPos, nextPos);
    }
    signal_move_complete(self);
}

void signal_move_complete(Cyclist * self){
    P(self->accessMutex);
    self->moved = true;
    pthread_cond_signal(&self->updatedCond);
    V(self->accessMutex);
}

void signal_relocation_complete(Cyclist * self){
    P(self->accessMutex);
    self->relocated = true;
    pthread_cond_signal(&self->updatedCond);
    pthread_cond_broadcast(&self->globalRelocationCond);
    V(self->accessMutex);
}

void reset_cyclist_atributtes(Cyclist * self){
    self->moved = false;
    self->relocated = false;
    self->failedToMove = false;
}

/* >>>>>>>>>>>>>>>>>>>>>>>>> SINCRONIZAÇÃO EM BARREIRAS <<<<<<<<<<<<<<<<<<<<<<<<< */

void sinc_in_barrier(pthread_barrier_t * barrier, bool round){ pthread_barrier_wait(&barrier[round]); }

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

double get_normalized_probability(){ return ((double)rand() / (double)RAND_MAX) * 100.0; }

bool verify_event_probability(double targetProbability){ return get_normalized_probability() <= targetProbability; }

void swap(int *arr, int source, int target) { 
    int tmp = arr[source];
    arr[source] = arr[target];
    arr[target] = tmp;
}

void get_random_array(int *arr, int n) {
    for (int i = 0; i < n; i++) arr[i] = i+1;
    for (int i = n - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        swap(arr, i, j);
    }
}

void print_lane_border(){ 
    for (int i = 0; i < globalTrack.length; i++) fprintf(stderr, "____"); 
    fprintf(stderr, "\n");
}

                                /* Nunca usei qsort antes, então apenas copiei e adaptei esta parte de: */
/* https://www-tutorialspoint-com.translate.goog/c_standard_library/c_function_qsort.htm?_x_tr_sl=en&_x_tr_tl=pt&_x_tr_hl=pt&_x_tr_pto=tc */
int decreasing_order_if_finish_time(const void *a, const void *b) {
    const Cyclist *c1 = *(const Cyclist **)a;
    const Cyclist *c2 = *(const Cyclist **)b;
    if (c1->finishTime < c2->finishTime) return  1;
    if (c1->finishTime > c2->finishTime) return -1;
    return 0;
}

int decreasing_order_of_broken_lap(const void *a, const void *b) {
    const Cyclist *c1 = *(const Cyclist **)a;
    const Cyclist *c2 = *(const Cyclist **)b;
    return (c2->currentLap - c1->currentLap);
}

/* >>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DE LIMPEZA <<<<<<<<<<<<<<<<<<<<<<<< */

void join_cyclist_threads(){ for (int id = 1; id <= totalCyclists; id++) pthread_join(cyclistThreads[id], NULL); }

void clean_simulation_memory() {
    for (int i = 0; i < MAX_BARRIER_GROUPS; i++){
        pthread_barrier_destroy(&barrier_depart[i]);
        pthread_barrier_destroy(&barrier_arrive[i]);
        pthread_barrier_destroy(&barrier_moved[i]);
    }

    if (isEfficient) {
        for (int i = 0; i < globalTrack.length; i++) pthread_mutex_destroy(&mutex_pos[i]);
        free(mutex_pos);
    } else {
        pthread_mutex_destroy(&mutex_global);
    }

    for (int i = 0; i < globalTrack.length; i++) free(globalTrack.slots[i]);
    free(globalTrack.slots);
    
    for (int i = 0; i < judge.lapsQtd; i++) {
        free(judge.laps[i]->lapCompleteOrder);
        free(judge.laps[i]);
    }
    free(judge.laps);

    for (int i = 1; i <= totalCyclists; i++) {
        Cyclist *cyclistCleanTarget = cyclistArray[i];
        pthread_cond_destroy(&cyclistCleanTarget->updatedCond);
        pthread_cond_destroy(&cyclistCleanTarget->globalRelocationCond);
        pthread_mutex_destroy(&cyclistCleanTarget->accessMutex);
        free(cyclistCleanTarget);
    }
    
    free(cyclistArray);
    
    free(cyclistThreads);
}