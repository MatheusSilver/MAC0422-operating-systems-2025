#!/bin/bash

# >>>>>>>>>>>>>>>>>>>>>>>>> VERIFICAÇÃO INICIAL <<<<<<<<<<<<<<<<<<<<<<<<< #
if [[ $# -lt 2 ]]; then
    echo "Uso: $0 <quantidade de clientes> <tamanho do arquivo 1> <tamanho do arquivo 2> [<tamanho do arquivo 3> ... <tamanho do arquivo n>]"
    exit 1
fi

# >>>>>>>>>>>>>>>>>>>>>>>>> VARIÁVEIS GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< #
declare -a SERVER_NAMES
declare -a CLIENT_NAMES
declare -a TEST_FILE_SIZES
declare CLIENT_QTD
declare RESULTS_FILE # Usado mais para pegar o path do arquivo de resultados .data
declare MAX_DIGITS   # Usado para poder deixar bem formatado o número de dígitos do tamanho do arquivo caso por exemplo, usemos 5; 10; 100 num mesmo arquivo.

# >>>>>>>>>>>>>>>>>>>>>>>>> CONSTANTES GLOBAIS <<<<<<<<<<<<<<<<<<<<<<<<< #
SERVER_PORT="1500"   # Usado para identificar se um processo já está ocupando uma certa porta.
                     # Caso a porta esteja ocupada, não conseguiremos subir o servidor, então usamos isso pra tentar matar o servidor ocupando a porta.
TARGET_DIR="/tmp/"
STANDARD_EXIT_MESSAGE="provavelmente enviou um exit"
STANDARD_ACCEPT_MESSAGE="Passou pelo accept"

# >>>>>>>>>>>>>>>>>>>>>>>>> MAIN <<<<<<<<<<<<<<<<<<<<<<<<< #

function main() {
    # Sobre trap, é basicamente como se fosse aqueles exit_handler da Unity ou C++
    # Configura o script para executar a limpeza em caso de encerramento prévio ou encerramento normal.
    # Assim não fica arquivos sobrando em qualquer estado de execução.
    trap cleanup EXIT

    set_global_variables "$@"
    compile_scripts
    execute_tests
    plot_data
    exit 0
}

# >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES PRINCIPAIS <<<<<<<<<<<<<<<<<<<<<<<<< #

# Configura as variáveis a partir dos dados de entrada.
function set_global_variables() {
    SERVER_NAMES=( 
        "ep4-servidor-inet_processos" 
        "ep4-servidor-inet_threads" 
        "ep4-servidor-inet_muxes" 
        "ep4-servidor-unix_threads" 
    )
    CLIENT_NAMES=( "ep4-cliente-inet" "ep4-cliente-unix" )
    CLIENT_QTD=$1; shift # Lembrete do Shift, esse cara avança o contador de indices em um, assim o argumento 1 é apagado, o 2 vira o 1, o 3 vira o 2 e assim por diante.
    TEST_FILE_SIZES=("$@")
    RESULTS_FILE="${TARGET_DIR}ep4-resultados-${CLIENT_QTD}.data"

    # Pega qual é o valor com o maior número de digitos, apenas para formatar o arquivo .data ao final.
    # Não impacta em nada, mas pelo menos deixa ele idêntico ao do enunciado.   
    highest=${TEST_FILE_SIZES[0]}

    for value in "${TEST_FILE_SIZES[@]}"; do
        if (( value > highest )); then
            highest=$value
        fi
    done

    MAX_DIGITS=${#highest}
}

# Compila todos os scripts, caso algum deles não possa ser feito, encerramos o programa.
function compile_scripts() {
    local SOURCE_DIR="ep4-clientes+servidores/"
    for serverName in "${SERVER_NAMES[@]}"; do
        echo "Compilando ${serverName}"
        gcc -pthread -o "${TARGET_DIR}${serverName}" "${SOURCE_DIR}${serverName}.c" || { echo "Erro ao compilar ${serverName}.c"; exit 3; }
    done
    for client in "${CLIENT_NAMES[@]}"; do
        echo "Compilando ${client}"
        gcc -o "${TARGET_DIR}${client}" "${SOURCE_DIR}${client}.c" || { echo "Erro ao compilar ${client}.c"; exit 4; }
    done
}

# O que o próprio nome diz... Executa o passo a passo dos testes.
function execute_tests() {
    local first=true
    local serverInitTime
    local serverName
    local serverType

    first=true
    for size in "${TEST_FILE_SIZES[@]}"; do
        generate_message_test_file "$size"
        init_result_data "${size}" "${first}"

        for serverName in "${SERVER_NAMES[@]}"; do
            # Registra hora de início
            check_server_port
            serverType=$(echo "$serverName" | cut -d '-' -f3 | cut -d '_' -f1)
            serverInitTime=$(date +"%F %T")
            setup_server "$serverName"
            connect_clients "$CLIENT_QTD" "$size" "$serverType"
            wait_clients_to_finish "$serverType"
            record_result "$serverName" "$serverInitTime" "$CLIENT_QTD"
            kill_server "${serverName}"
        done

        first=false
    done
}

# Gera um arquivo de configuração para plotar os dados do experimento.
function plot_data() {
    # Poderia fazer mais bonitinho ou eficiente, mas né... São no máximo uns 5 prints
    echo -n ">>>>>>> Gerando o gráfico de ${CLIENT_QTD} clientes com arquivos de:"
    for size in "${TEST_FILE_SIZES[@]}"; do
        echo -n " ${size}MB"
    done
    echo
    local inputGPIFile="/tmp/ep4-resultados-${CLIENT_QTD}.gpi"
    local outputPDFFile="ep4-resultados-${CLIENT_QTD}.pdf"

    # Modifiquei a linha de: set format y "%M:%S"
    # para: set format y "%tM:%S"
    # Só coloquei um t ali para que valores de tempo com minutos superiores a 60 não saiam de forma errônea.

    cat > "$inputGPIFile" <<EOF
set ydata time
set timefmt "%M:%S"
set format y "%tM:%S"

set xlabel 'Dados transferidos por cliente (MB)'
set ylabel 'Tempo para atender ${CLIENT_QTD} clientes concorrentes'

set term pdfcairo
set output "${outputPDFFile}"

set grid
set key top left

plot "${RESULTS_FILE}" using 1:4 with linespoints title "Sockets da Internet: Mux de E/S",\
     "${RESULTS_FILE}" using 1:3 with linespoints title "Sockets da Internet: Threads",\
     "${RESULTS_FILE}" using 1:2 with linespoints title "Sockets da Internet: Processos",\
     "${RESULTS_FILE}" using 1:5 with linespoints title "Sockets Unix: Threads"
EOF

    gnuplot "$inputGPIFile"
}

# >>>>>>>>>>>>>>>>>>>>>>>>> FUNÇÕES AUXILIARES DOS TESTES <<<<<<<<<<<<<<<<<<<<<<<<< #

# Cria uma mensagem aleatória com um tamanho específico em MB.
function generate_message_test_file() {
    local fileSizeMB=$1
    local fileName="arquivo_de_${fileSizeMB}MB.txt"
    echo ">>>>>>> Gerando um arquivo texto de: ${fileSizeMB}MB..."
    base64 /dev/urandom | head -c $((fileSizeMB * 1024 * 1024)) > "${TARGET_DIR}${fileName}"
    echo >> "${TARGET_DIR}${fileName}"
}

# Prepara o começo das linhas de resultados.
function init_result_data() {
    local size=$1
    local isFirst=$2

    # Geralmente sou contra o printf em bash, mas nesse caso acho que não tem como...
    if [ "$isFirst" = true ]; then
        printf "%0${MAX_DIGITS}d" "$size" > "$RESULTS_FILE"
    else
        printf "\n%0${MAX_DIGITS}d" "$size" >> "$RESULTS_FILE"
    fi
}

# Verifica se não tem servidores atualmente abertos na porta onde nossos servidores avaliados irão ser abertos.
function check_server_port() {
    local pid
    # Verifica se ainda há algum servidor usando a porta específica do EP
    # Útil para caso eu mesmo, ou até algum outro cara que seja corrigido antes de mim
    # Esqueça de encerrar o servidor ao final da aplicação.

    # Excessivamente específico pois se só buscassemos pela porta 1500, em uma situação extremamente específica
    # onde tentamos matar o servidor no exato instante em que ele está subindo, teriamos uma falha na sinalização.
    if pid=$(lsof -i TCP:"${SERVER_PORT}" -sTCP:LISTEN -t); then
        # Releva as mensagens de debug

        # echo "Porta ${SERVER_PORT} está sendo usada pelo ${pid}. Sinalizando a morte..."
        kill -15 "${pid}"
        sleep 0.5

        if lsof -i TCP:"${SERVER_PORT}" -sTCP:LISTEN -t &>/dev/null; then
            echo "A porta ${SERVER_PORT} estava ocupada e não foi possível liberá-la. Encerrando."
            exit 3
        fi

        # echo "Porta ${SERVER_PORT} liberada."
    fi

    # Não precisa fazer para Sockets de domínio Unix pois o próprio Servidor Unix faz o unlink
}

# Inicializa o servidor.
function setup_server() {
    local serverName=$1
    echo "Subindo o servidor ${serverName}"
    "${TARGET_DIR}${serverName}" &>/dev/null &
    sleep 2  # dá tempo de subir
}

# Inicia um dado número de clientes para se conectarem no servidor recém aberto.
function connect_clients() {
    local numClients=$1
    local fileSize=$2
    local serverType=$3
    echo ">>>>>>> Fazendo ${numClients} clientes ecoarem um arquivo de: ${fileSize}MB..."
    # Ok... Eu poderia deixar o for dentro do if, mas acho que ia ficar feio, sem contar que isso não afeta tanto a performance
    # Então, preferi deixar bonito a custa de deixar "mais" rápido.
    for i in $(seq 1 "$numClients"); do
        if [[ "$serverType" == inet ]]; then
            "${TARGET_DIR}${CLIENT_NAMES[0]}" 127.0.0.1 < "${TARGET_DIR}arquivo_de_${fileSize}MB.txt" &>/dev/null &
        else
            "${TARGET_DIR}${CLIENT_NAMES[1]}" < "${TARGET_DIR}arquivo_de_${fileSize}MB.txt" &>/dev/null &
        fi
    done      
}

# Trava a execução do script até que todos os processos clientes sejam finalizados.
function wait_clients_to_finish() {
    local clientName
    local serverType=$1
    if [[ "$serverType" == inet ]]; then
        clientName="${CLIENT_NAMES[0]}"
    else
        clientName="${CLIENT_NAMES[1]}"
    fi
    
    echo "Esperando os clientes terminarem..."
    while pgrep -f "${clientName}" > /dev/null; do
        sleep 1
    done
}

# Verifica os resultados e salva eles no arquivo .data além de expressar na tela.
function record_result() {
    local serverName=$1
    local initTime=$2
    local numClients=$3
    local time
    echo "Verificando os instantes de tempo no journald..."
    time=$(get_time_in_journal "$serverName" "$initTime")
    echo -n " ${time}" >> "$RESULTS_FILE"

    check_finished_clients "$serverName" "$initTime" "$numClients"

    echo ">>>>>>> Tempo para servir os ${CLIENT_QTD} clientes com o ${serverName}: ${time}"
}

# Temos como alvo o nome do processo que é o servidor, e mandamos um sinal para ele ser finalizado.
function kill_server() {
    local serverName=$1
    local serverPid=$(pgrep -f "$serverName")
    echo "Enviando um sinal 15 para o servidor ${serverName}..."
    kill -15 "${serverPid}" &>/dev/null 
}

# >>>>>>>>>>>>>>>>>>>>>>>>> UTILIDADES GERAIS <<<<<<<<<<<<<<<<<<<<<<<<< #

# A maior parte do EP por algum motivo foi gasta aqui... Não é a toa que é a parte com maior número de anotações.
function get_time_in_journal() {
    local serverName=$1
    local initTime=$2
    local line
    local start
    local end

    # Conjunto de linhas de entradas e saidas registradas com respeito ao servidor atual em execução
    # sem o --output=short-iso, meu PC entrega algo desse tipo: Jun 12 09:41:14
    # com o --output=short-iso meu PC entrega 2025-06-12T09:41:14-0300 que é o mesmo do exemplo do EP.
    line=$(journalctl -q --since="$initTime" --output=short-iso | grep "$serverName")
    # Em teoria, o grep "$serverName" não deveria ser necessário, mas usamos mais por uma questão de segurança.
    # Vai que algum chinês mineirando bitcoin resolva colocar outra coisa pra rodar em secreto como Daemon.

    # Caso eu precise voltar nessa ideia no futuro:
    # Line contém varias linhas desse tipo:
    # 2025-06-12T00:11:04-0300 Matheus ep4-servidor-inet_processos[6655]: Cliente do PID 6655 provavelmente enviou um exit
    # então filtramos a que tem accept, com grep, pegamos a primeira entrada correspondendo ao primeiro processo que passou pelo
    # accept e começou a ser atendido com o head -n1, então, observando o espaço entre a data e o usuário, como estamos interessados
    # só na data pegamos apenas o primeiro termo da string com cut.
    # Por último, o dateutils.diff espera algo nesse estilo "2025-02-18 23:43:21"
    # Então, basta trocar o "T" por espaço em branco com sed e tá tudo certo : D
    start=$(echo "$line" | grep "${STANDARD_ACCEPT_MESSAGE}" | head -n1 | cut -d ' ' -f1 | sed 's/T/ /')
    end=$(echo "$line" | grep "${STANDARD_EXIT_MESSAGE}" | tail -n1 | cut -d ' ' -f1 | sed 's/T/ /')
    
    echo "$(dateutils.ddiff "$start" "$end" -f "%0M:%0S")"
}

# Assegura que todos os clientes foram devidamente finalizados
# Fazemos isso verificando se desde o momento em que começamos a executar as conexões, até o momento que terminamos
# O número de "exits" é igual ao número de clientes que fizemos a conexão.
function check_finished_clients() {
    local serverName=$1
    local initTime=$2
    local numClients=$3

    local finishedClients=$(journalctl -q --since="$initTime" | grep "$serverName" | grep "${STANDARD_EXIT_MESSAGE}" | wc -l)

    if [ "$finishedClients" -ne "$numClients" ]; then
        # Se essa linha for ativada, reza a lenda que o Ivan dá um belo dum zero no EP.
        echo "Total de clientes finalizados: ${finishedClients}"
        echo "Era esperado que ${numClients} fossem finalizados"
        exit 1
    fi

    echo ">>>>>>> ${finishedClients} clientes encerraram a conexão"
}

# >>>>>>>>>>>>>>>>>>>>>>>>> FINALIZAÇÃO E LIMPEZA <<<<<<<<<<<<<<<<<<<<<<<<< #

function cleanup() {
    
    delete_temp_files

    # Caso o servidor seja encerrado previamente, então fechamos o servidor e eventualmente todos os outros clientes conectados
    # O próprio cliente deveria fechar se perdesse a conexão, mas por garantia, mandamos o sinal para fechar do mesmo jeito.
    check_server_port
    close_all_clients
}

function delete_temp_files(){
    # Poderia fazer só um rm -f ep4-*           
    # Mas por segurança, preferi fazer de cada grupo individualmente.
    rm -f "${TARGET_DIR}"ep4-resultados-*.gpi
    rm -f "${TARGET_DIR}"ep4-resultados-*.data
    rm -f "${TARGET_DIR}"ep4-servidor*
    rm -f "${TARGET_DIR}"ep4-cliente*

    rm -f "${TARGET_DIR}"arquivo_de_*MB.txt
    rm -f "${TARGET_DIR}uds-echo.sock"
}

function close_all_clients(){
    if pgrep -f "^${TARGET_DIR}ep4-cliente" >/dev/null; then
        pkill -15 -f "^${TARGET_DIR}ep4-cliente"
    fi
}

main "$@" 