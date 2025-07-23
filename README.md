# Exercícios-Programa / MAC0422 (Sistemas Operacionais)

Estes programas, com exceção de alguns devidamente mencionados em seus cabeçalhos, foram desenvolvidos por **Matheus Silveira Feitosa**  
Instituto de Matemática e Estatística da Universidade de São Paulo (IME-USP)

## Índice

- [Exercícios-Programa / MAC0422 (Sistemas Operacionais)](#exercícios-programa--mac0422-sistemas-operacionais)
  - [Índice](#índice)
  - [Descrição do Repositório](#descrição-do-repositório)
    - [Geral](#geral)
  - [Separação dos programas e execuções](#separação-dos-programas-e-execuções)
  - [Dependências Gerais](#dependências-gerais)

## Descrição do Repositório

### Geral
Ao total, durante as aulas de MAC0422 ministradas no primeiro semestre pelo professor **Daniel Macêdo Batista**, foram pedidos 4 Exercícios Programa, cada um focando em tópicos específicos do assunto de Sistemas Operacionais, sendo estes tópicos:
- Escalonadores de Processos
- Funcionalidades e usos de Threads
- Gerenciamento de memória
- Comunicação entre Processos (IPC)

Considerando que algo similar a estes exercícios programa dificilmente serão dados novamente, objetiva-se com este reposítorio, além do backup, servir como apoio a futuros alunos desta matéria (em especial no cenário onde eu poderei vir a ser monitor dela)

## Separação dos programas e execuções
Como cada exercício programa aborda um assunto diferente, existem diversas dependências para cada um, para isso, recomendamos ler o **LEIAME** de cada exercício pois lá, estão explicados o que deve ser instalado em cada EP.

Mas em geral, para cada um deles, sua compilação pode ser feita através do comando abaixo:

```bash
make
```

E com isso o próprio Makefile cuidará de compilar o programa, em seguida, para executá-lo, basta fazer 

```bash
./ep <parâmetros_do_ep>
```

Onde os parâmetros estão explicados no LEIAME, caso o programa precise.

## Dependências Gerais
Todos os programas foram desenvolvidos e testados no Sistema Operacional Ubuntu 22.04.5.

Com isso, para facilitar, basta executar o script:
```bash
instala_dependencias.sh
```

Ou instalar cada uma de suas dependencias manualmente que você poderá compilar todos os programas.

********
