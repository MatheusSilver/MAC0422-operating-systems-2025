#ifndef EP2_H
#define EP2_H

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEPENDÊNCIAS EXTERNAS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Bibliotecas padrão */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Esse não era necessário, mas ajudou a organizar o código */
#include <stdbool.h>

/* Usado para a manipulação das threads */
#include <pthread.h>

/* Usada para definir uma seed aleatória para cada execução */
/* Se quiser, pode definir uma seed específica != 0 na variável RAND_SEED */
#include <time.h>

/* >>>>>>>>>>>>>>>>>>>>>>>>> DEFINIÇÕES DE MACROS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* As threads são sincronizadas com barreiras alternadas */
/* Portanto, MAX_BARRIER_GROUPS define quantas dessas barreiras */
/* Serão criadas e posteriormente destruídas. */
#define MAX_BARRIER_GROUPS 2

/* Número máximo de faixas por posição, alterável se necessário */
/* Mas note que nunca deve ser menor que 6, caso contrário, nenhum ciclista irá andar. */
#define MAX_LANES 10

/* Definição de uma ID para posição vazia na pista. */
#define EMPTY     -1

/* Macros usadas para padronizar o uso dos mutexes como visto em aula */
#define P(mutex)                        pthread_mutex_lock(&(mutex))
#define V(mutex)                        pthread_mutex_unlock(&(mutex))

/* Status do ciclista */
typedef enum {
    BROKEN,
    RUNNING,
    ELIMINATED
} CyclistStatus;
/* Todo ciclista começa como RUNNING, tal condição também é usada */
/* para sinalizar para thread que esta deve executar sua execução */
/* Dizemos que se um ciclista não está correndo, então ele deve sair da pista. */

/* >>>>>>>>>>>>>>>>>>>>>>>>> ESTRUTURAS AUXILIARES <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Estrutura geral da pista simulada. */
typedef struct {
    int **slots;
    int length;
    int lanes;
} Track;

/* Basicamente uma tentativa de tratar o ciclista e sua respectiva thread como um objeto */
typedef struct {
    int id;                              /* Identificador único usado para se referir ao ciclista.                          */
    int xPos;                            /* Posição horizontal do ciclista na pista                                         */
    int lane;                            /* Faixa vertical atual do ciclista                                                */
    int speed;                           /* Velocidade atual do ciclista 30km/h ou 60km/h                                   */
    int currentLap;                      /* Volta atual (usado para demarcar cruzamentos e decidir quebras)                 */
    unsigned long long finishTime;       /* Último instante de tempo que um ciclista estava correndo                        */
    bool speedMoment;                    /* Marca se um ciclista a 30km/h está na iminência de se mover                     */
    bool round;                          /* Controla em qual barreira o ciclista deve entrar em cada turno                  */
    bool eliminationVerified;            /* Liberação dada pelo coordenador de que o ciclista deve sair                     */
    bool finishedLap;                    /* Sinaliza que finalizou uma volta                                                */
    bool moved;                          /* Indica que o ciclista já tentou se mover no seu turno                           */
    bool relocated;                      /* Indica que o ciclista já está na menor faixa possível                           */
    bool ready;                          /* Indica para o coordenador que o ciclista está pronto para partir                */
    bool failedToMove;                   /* Marca para o próprio ciclista tentar de novo, no turno da relocação             */
    pthread_mutex_t accessMutex;         /* Mutex para acesso ao ciclista, usado para evitar condição de corrida            */
                                         /* Quando um ciclista próximo vai verificar se este já andou.                      */
    pthread_cond_t updatedCond;          /* Variável de condição usada para indicar que uma atualização foi feita           */
    pthread_cond_t globalRelocationCond; /* Variável de condição final que indica que o ciclista terminou tudo em seu turno */

    bool isSomeoneWaiting;               /* Variável que indica se já há outro ciclista esperando uma atualização deste     */

    CyclistStatus status;                /* Indica se o ciclista está correndo, ou se já foi eliminado ou quebrado          */
} Cyclist;

/* Estrutura de controle de cada volta */
/* Em resumo, registra se todos os ciclistas já cruzaram */
/* E em especial, atua registrando sempre o último ciclista a cruzar */
/* Caso seja preciso fazer uma eliminação tardia.                    */

/* Adiciona um grande custo em memória para manter objetivando resolver a eliminação tardia */
/* Na prática, este fenômeno foi raramente observado.                                       */

typedef struct {
    Cyclist * eliminatedCyclist;        /* Provavel ciclista eliminado (último que cruzou)                         */
    int cyclistsOnLap;                  /* Número de ciclistas que ainda poderão cruzar esta volta.                */
    bool defined;                       /* Indica que a volta já foi concluída e não precisa atualizar             */
    Cyclist ** lapCompleteOrder;        /* Ordem de chegada dos ciclistas (usado para imprimir o resumo por volta) */
    int nextAvailablePositon;           /* Contador que indica qual a próxima colocação do ciclista que cruzar     */
} lapController;

typedef struct {
    lapController ** laps;              /* Apontador para o registro de todas as voltas possíveis na corrida                       */
    int lapsQtd;                        /* Quantidade total de voltas possíveis                                                    */
    int currentLastLap;                 /* Última volta ainda não completada por algum ciclista vivo                               */
    int winnerID;                       /* Indice do vencedor da corrida (usado em especial para diferenciar o primeiro do último) */
} Judge;

/* >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/* Acesso a pista compartilhada */
extern Track globalTrack;

/* Acesso ao juiz (READ-ONLY para os ciclistas) */
extern Judge judge;

/* Array de threads criadas para cada ciclista */
/* Usado para garantir o join correto no final */
extern pthread_t *cyclistThreads;

/* Variável READ-ONLY que marca o número inicial total de ciclistas na corrida */
extern int totalCyclists; 

/* Contador de ciclistas ainda correndo (usado em especial para sincronizar a barreira e determinar o fim da corrida) */
extern int activeCyclists;

/* Contador de turnos transcorridos na simulação */
/* Cada turno tem duração de 60ms */
extern unsigned long long instant; 

/* Array READ-ONLY com todos os ciclistas participantes da corrida */
extern Cyclist **cyclistArray;

/* Variável READ-ONLY que verifica se estamos operando na abordagem "Eficiente" ou ingênua. */
extern bool isEfficient;

/* Variável READ-ONLY que verifica se estamos em modo Debug (imprime a pista a cada instante) */
extern bool isDebug;

/* Variável READ-ONLY que apenas inicia o rand com alguma semente específica */
extern const int RAND_SEED;

/* Verifica e sinaliza para os corredores restantes para encerrarem pois a simulação acabou. */
extern bool finishedSimulation;

/* Barreiras alternadas usadas para sincronizar o ciclista e a entidade central */
extern pthread_barrier_t barrier_depart[MAX_BARRIER_GROUPS];    /* Barreira de sincronização de largada                     */
extern pthread_barrier_t barrier_arrive[MAX_BARRIER_GROUPS];    /* Barreira de sincronização de chegada                     */
extern pthread_barrier_t barrier_moved[MAX_BARRIER_GROUPS];     /* Barreira de sincronização de fase de movimento e descida */

/* Mutexes de acesso a pista */
extern pthread_mutex_t *mutex_pos;      /* Mutex por posição usado na abordagem eficiente */
extern pthread_mutex_t mutex_global;    /* Mutex global usado na abordagem ingênua        */

/* >>>>>>>>>>>>>>>>>>>>>>>>> INICIALIZAÇÃO DO SIMULADOR <<<<<<<<<<<<<<<<<<<<<<<<< */

/*!
 * @brief Faz a validação dos argumentos de entrada e inicializa as variáveis que os usam.
 * @note Se os parâmetros não condizerem com o esperado, encerrada o programa informando ao usuário o que estava errado.
 * @param args Número de argumentos passados.
 * @param argv Array de argumentos passados.
 */
void parse_args(int argc, char *argv[]);

/**
 * @brief Inicializa componentes gerais da simulação.
 */
void init();

/**
 * @brief Aloca e inicializa estruturas de dados usadas na simulação como os ciclistas, suas threads, a pista, etc...
 */
void init_structures();

/**
 * @brief Cria e inicializa as threads de ciclistas.
 */
void init_cyclist_threads();

/* >>>>>>>>>>>>>>>>>>>>>>>>> ENTIDADE CENTRAL <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Executa o loop principal da simulação.
 */
void simulate();

/**
 * @brief Verifica a cada volta concluída quais ciclistas devem ser eliminados ou removidos da pista e os sinaliza para interromperem sua execução
 * @note É também o principal responsável por manejar o @see Judge.
 */
void destroy_cyclists();

/**
 * @brief Atualiza as informações das voltas concluídas.
 */
void update_finished_laps();

/**
 * @brief Identifica ciclistas a serem eliminados após completarem uma volta.
 * @param eliminatedCount Ponteiro para armazenar o número de candidatos.
 * @return Vetor de ponteiros de ciclistas que poderão ser eliminados.
 */
Cyclist** get_elimination_candidates(int *eliminatedCount);

/**
 * @brief Verifica se entre os ciclistas a serem eliminados em uma dada, existem ciclistas quebrados
 * @note  A escolha pela verificação se deu pois, se em uma volta de eliminação, um ciclista quebrar, então o eliminado da rodada é o quebrado.
 * @param candidates Vetor de ciclistas a serem eliminados
 * @param count Número de candidatos que podem ser eliminados.
 */
void check_broke_candidate(Cyclist **candidates, int count);

/**
 * @brief Elimina ciclistas que cruzaram juntos aleatoriamente mudando seus status para ELIMINATED @see CyclistStatus.
 * @note  Se só houver um, então este é eliminado simplesmente.
 * @note  Só é executado se não houverem ciclistas quebrados entre os candidatos a eliminação.
 * @param candidates Vetor de candidatos a eliminação (neste momento, sem ciclistas quebrados.).
 * @param count Número de candidatos a eliminação.
 */
void assign_random_elimination(Cyclist **candidates, int count);

/**
 * @brief Sinaliza as threads cujo o status já não é mais de RUNNING, para que deixem a pista.
 */
void perform_eliminations();

/**
 * @brief Adiciona ciclista na lista de ordem de chegada de uma volta.
 * @note Utiliza a variável nextAvailablePositon para saber qual a ordem de chegada do ciclista @see lapController.
 * @param cyclist Ponteiro para o ciclista que completou uma volta.
 * @param lapIndex Índice da volta na lista de voltas.
 */
void append_cyclist_on_order_list(Cyclist *cyclist, int lapIndex);

/**
 * @brief Reconstrói uma barreira para novo tamanho.
 * @note  Internamente, utiliza a variável global @see instant para saber qual barreira deve ser reconstruída.
 * @param barrier Ponteiro para o vetor de barreiras.
 * @param size Novo tamanho da barreira
 */
void rebuild_barrier(pthread_barrier_t *barrier, int size);

/**
 * @brief Avança o tempo simulado de forma discreta.
 * @note  Na pratica, incrementa instant em uma uma unidade, o que significa avançar o relógio em 60ms.
 */
void advance_time();

/**
 * @brief Executa os procedimentos de encerramento da simulação como aguardar o fim das threads, limpar memória e gerar o relatório final da corrida.
 */
void finish_simulation();

/* >>>>>>>>>>>>>>>>>>>>>>>>> IMPRESSÃO DOS RESULTADOS <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Imprime relatório completo da conclusão da corrida informando os ciclistas que quebraram e a ordem de finalização da prova.
 * @param cyclistArray Vetor com todos as estruturas de ciclistas.
 * @param totalCyclists Total de ciclistas que começaram a prova.
 */
void print_full_report(Cyclist **cyclistArray, int totalCyclists);

/**
 * @brief Desenha o estado atual da pista com as posições de cada ciclista numa visão da direita pra esquerda.
 */
void print_full_track();

/**
 * @brief Imprime a posição de cada ciclista em uma volta concluída.
 * @note  Dizemos que uma volta foi concluída quando todos os ciclistas ativos em dada volta cruzaram a linha de chegada.
 */
void print_finishing_list();

/* >>>>>>>>>>>>>>>>>>>>>>>>> CICLISTA <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Função executada por cada thread de ciclista até que este não esteja mais na corrida.
 * @param arg Ponteiro para a estrutura do ciclista simulado.
 */
void *cyclist_simulation(void *arg);

/**
 * @brief Inicializa atributos de um ciclista.
 * @param self Ponteiro para o ciclista.
 * @param id Identificador único do ciclista.
 * @param xPos Posição inicial na pista.
 * @param lane Faixa inicial.
 */
void init_cyclist(Cyclist *self, int id, int xPos, int lane);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES DO CICLISTA <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Adiciona um ciclista em uma dada posição da pista baseada na sua informação interna de xPos e lane
 * @param cyclist ponteiro para o ciclista que será adicionado na pista.
 * @see Cyclist
 */
void add_cyclist_on_track(Cyclist *cyclist);

/** @brief Remove ciclista de uma dada posição na pista.
 *  @param self Ponteiro para ciclista.
 *  @param xPos Posição anterior.
 *  @param lane Faixa anterior.
 *  @note  Pedimos também o ponteiro do ciclista para verificar se a posição ainda é a dele, se for então limpamos a posição
 *         Na prática não é usado, mas é mais como uma medida de segurança.
 */
void remove_position_on_track(Cyclist *self, int xPos, int lane);

/** @brief Atualiza posição de ciclista na pista, removendo ele de sua posição e faixa antigas e o colocando na sua posição e faixa especificas em sua estrutura.
 *  @param cyclist Ponteiro para ciclista movimentado.
 *  @param prevPos Posição anterior do ciclista.
 *  @param prevLane Faixa anterior do ciclista.
 */
void update_position_on_track(Cyclist *cyclist, int prevPos, int prevLane);

/** @brief Atualiza posição apenas na faixa.
 *  @note  Usado em geral para realocar um dado ciclista para uma faixa mais interna.
 *  @param cyclist Ponteiro para ciclista.
 */
void update_position_on_lane(Cyclist *cyclist, int prevLane);

/** @brief Decide próxima velocidade do ciclista baseado na lógica de, se este estiver a 60km/h então ele tem 55% de chance de reduzir.
 *          Caso esteja a 30 km/h, então ele tem 75% de chance de tentar aumentar a velocidade.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 *  @param currentSpeed Velocidade atual.
 */
void decide_next_speed(Cyclist *self, int currentSpeed);

/** @brief Decide se irá continuar ou não na competição "quebrar"
 *  @note  A cada vez que o ciclista completar um múltiplo de 5 voltas, então ele terá 10% de chance de decidir "quebrar".
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void decide_continuity(Cyclist *self);

/** @brief Caso um ciclista tenha desistido, reporta na saida padrão no exato momento que ele tomou a decisão.
 *  @note  Só é usado caso o programa não esteja em modo debug.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void report_broke(Cyclist *self);

/** @brief Recalcula próxima posição objetivo do ciclista baseada na sua velocidade atual, e em seu speedMomentum.
 *  @see Cyclist
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void recalculate_position(Cyclist *self);

/** @brief Trava mutex para movimentação.
 *  @note  Caso seja a abordagem ingênua, então trava o Mutex Global
 *         Caso seja a abordagem eficiente, trava a coluna atual ou a próxima de forma alternada a depender da posição atual do ciclista.
 *  @param prevPos Posição anterior (Atual do ciclista).
 *  @param nextPos Próxima posição. (Objetivo do ciclista)
 */
void lock_track_mutex(int prevPos, int nextPos);

/** @brief Destrava mutex após movimentação (ou caso um dado ciclista deva esperar por outro).
 *  @note  Caso seja a abordagem ingênua, então destrava o Mutex Global
 *         Caso seja a abordagem eficiente, destrava a coluna atual ou a próxima de forma alternada a depender da posição atual do ciclista.
 *  @param prevPos Posição anterior (Atual do ciclista).
 *  @param nextPos Próxima posição. (Objetivo do ciclista)
 */
void unlock_track_mutex(int prevPos, int nextPos);

/** @brief Retorna ponteiro para um dado ciclista por ID.
 *  @param id Identificador do ciclista.
 *  @return Ponteiro para ciclista.
 */
Cyclist *get_cyclist_by_id(int id);

/** @brief Trava mutex de faixa.
 *  @note  Caso seja a abordagem ingênua, trava o Mutex Global
 *  @param curLane Faixa atual.
 */
void lock_lane(int curLane);

/** @brief Destrava mutex de faixa.
 *  @note  Caso seja a abordagem ingênua, destrava o Mutex Global
 *  @param curLane Faixa atual.
 */
void unlock_lane(int curLane);

/** @brief Obtém a posição horizontal anterior dada uma posição atual.
 *  @param xPos Posição atual.
 *  @return Posição anterior.
 */
int getPrevPosition(int xPos);

/** @brief Aguarda movimentação/realocação do ciclista na faixa mais externa de uma dada posição da pista.
 *  @param nextPos Próxima posição.
 */
void wait_for_cyclist_on_top(int nextPos);

/** @brief Fallback para o caso de um ciclista não ter que conseguido se movimentar no turno normal
 *          Nesta ocasião, ele aguarda todos terem andado e realocado para então tentar andar pra frente de novo.
 *  @param self ponteiro para a própria struct do ciclista que o chamou e falhou em avançar.
 */
void failed_to_move_ahead(Cyclist *self);

/** @brief Obtém um elemento em uma dada posição da pista. 
 *  @param xPos Posição horizontal da pista.
 *  @param lane Faixa vertical da pista.
 *  @return Conteúdo da célula (Retorna EMPTY se não tem ninguem), retorna o ID do ciclista se houver um ciclista lá.
 */
int get_track_content(int xPos, int lane);

/** @brief Aguarda que um dado ciclista atinja a faixa mais interna possível em uma dada coluna.
 *  @param id ID do ciclista que iremos aguardar.
 *  @param prevPos Posição anterior de quem está aguardando.
 *  @param nextPos Próxima posição de quem está aguardando.
 *  @note As posições atuais e próximas são usadas para controlar o uso dos mutexes tanto para a segunda chance quando para relocação simples.
 */
void wait_partner_to_go_down(int id, int prevPos, int nextPos);

/** @brief Aguarda que um certo ciclista complete sua movimentação, seja pela atualização do seu momentum, seja por conseguir se movimentar propriamente na pista.
 *  @param id ID do ciclista que iremos aguardar.
 *  @param prevPos Posição anterior de quem está aguardando.
 *  @param nextPos Próxima posição de quem está aguardando.
 */
void wait_partner_to_move(int id, int prevPos, int nextPos);

/** @brief Verifica se um dado ciclista já se digiriu a faixa mais interna possivel.
 *  @param cyclist Ponteiro para ciclista que estamos verificando.
 *  @return verdadeiro caso já tenha relocado, falso caso contrário
 *  @note na prática retorna cyclist->relocated
 *  @see Cyclist
 */
bool alreadyRelocated(Cyclist *cyclist);

/** @brief Verifica se um dado ciclista já se movimentou na pista.
 *  @param cyclist Ponteiro para ciclista que estamos verificando.
 *  @return verdadeiro caso já tenha se movimentado na pista, falso caso contrário
 *  @note na prática retorna cyclist->moved
 *  @see Cyclist
 */
bool alreadyMoved(Cyclist *cyclist);

/** @brief Verifica se já há algum outro ciclista aguardando atualização de outro ciclista.
 *  @param cyclist Ponteiro para ciclista que estamos verificando.
 *  @return verdadeiro caso não tenha ciclistas esperando, falso caso contrário
 *  @note na prática retorna !cyclist->isSomeoneWaiting
 *  @see Cyclist
 */
bool can_wait_for(Cyclist *cyclist);

/** @brief Verifica se, durante um turno de relocação, se esta já foi conclúida.
 *  @note  Usada por ciclistas que estão recebendo uma segunda chance e só podem se movimentar se houver espaço livre na coluna seguinte.
 *  @param nextPos Próxima posição da pista.
 *  @return verdadeiro caso todas as relocações já tenham sido concluidas, falso caso contrário.
 */
bool nextLaneReady(int nextPos);

/* Vulgo função mais importante e mais crítica do EP... */

/** @brief Avalia possibilidade de ultrapassagem.
 *  @param self ponteiro para a própria struct do ciclista que o chamou e quer tentar avançar.
 *  @param prevPos Posição atual do ciclista.
 *  @param nextPos Posição alvo objetivo do ciclista.
 *  @param waitForRelocation Modo extra que altera como iremos tentar avançar.
 *                           true se o modo de ultrapassagem é por criação de espaços livres. (geralmente 1 espaço livre.)
 *                           Usado no mecanismo de segunda chance de movimentação.
 *  @return verdadeiro se o ciclista puder avançar para a próxima posição, falso caso contrário. 
 */
bool can_overtake(Cyclist *self, int prevPos, int nextPos, bool waitForRelocation);

/** @brief Tenta descer a faixa do ciclista o máximo possível em sua posição.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void recalculate_lane(Cyclist *self);

/** @brief Tenta avançar o ciclista em uma posição na pista.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void move(Cyclist *self);

/** @brief Sinaliza para os demais ciclistas que este terminou de tentar se mover pela pista.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void signal_move_complete(Cyclist *self);

/** @brief Sinaliza para os demais ciclistas que este terminou de se realocar na coluna.
 *  @param self ponteiro para a própria struct do ciclista que o chamou.
 */
void signal_relocation_complete(Cyclist *self);

/** @brief Reseta atributos do ciclista.
 *  @param self Ponteiro.
 */
void reset_cyclist_atributtes(Cyclist *self);

/* >>>>>>>>>>>>>>>>>>>>>>>>> SINCRONIZAÇÃO EM BARREIRAS <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Sincroniza threads em uma dada barreira de indice round na array barrier
 * @param barrier Ponteiro para a array de barreiras.
 * @param round Indice da barreira
 * @note Esta função é intrinsecamente relacionada com a alternância tanto da entidade principal quanto dos ciclistas, por isso, round.
 */
void sinc_in_barrier(pthread_barrier_t *barrier, bool round);

/* >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Gera um número real aleatório entre 0 e 100
 * @return Número aleatório qualquer entre 0 e 100
 */
double get_normalized_probability();

/**
 * @brief Verifica se evento ocorre dado probabilidade alvo.
 * @param targetProbability Probabilidade alvo.
 * @see get_normalized_probability()
 * @return true se evento acontece.
 */
bool verify_event_probability(double targetProbability);

/**
 * @brief Troca elementos em array.
 * @param arr Ponteiro para array.
 * @param source Índice fonte.
 * @param target Índice alvo.
 */
void swap(int *arr, int source, int target);

/**
 * @brief Preenche um array dado com uma permutação aleatória.
 * @param arr Ponteiro para array.
 * @param n Tamanho do array.
 */
void get_random_array(int *arr, int n);

/**
 * @brief Imprime borda da pista.
 * @note  Usado apenas no modo debug.
 */
void print_lane_border();

/**
 * @brief Ordena por tempo final decrescente (usado exclusivamente no qsort do relatório final)
 */
int decreasing_order_if_finish_time(const void *a, const void *b);

/**
 * @brief Ordena por volta quebrada decrescente (usado exclusivamente no qsort do relatório final)
 */
int decreasing_order_of_broken_lap(const void *a, const void *b);

/* >>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES DE LIMPEZA <<<<<<<<<<<<<<<<<<<<<<<< */

/**
 * @brief Aguarda término de todas as threads de ciclistas.
 */
void join_cyclist_threads();

/**
 * @brief Libera memória e recursos da simulação.
 */
void clean_simulation_memory();

#endif /* EP2_H */
