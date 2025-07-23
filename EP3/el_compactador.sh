#!/bin/bash

#Adaptado do Tutorial do EP2 de TecProg
#Deixe este script na mesma pasta da pasta que contém o EP
#NA MESMA PASTA E NÃO DENTRO DA PASTA!

#Ajuste esse maninho de acordo com o especificado em cada EP.
CONTEUDO_ESPERADO=(ep3.c ep3.h LEIAME Makefile slides-ep3.pdf trace-firstfit trace-nextfit trace-bestfit trace-worstfit)

#Configurações da Pasta e do nome do EP.
NUM_EP="3" #Alterar isso conforme o número do EP
           #Garantir que o número do EP esteja de acordo com o nome da pasta pro tar não fazer meida.
NOME="matheus_silveira_feitosa"
DIR="ep${NUM_EP}-${NOME}"

TARBALL="${DIR}.tar.gz"


if [ ! -d "${DIR}" ]; then
  echo "Não achei ${DIR}, verifica se o nome da tua pasta é"
  echo $DIR
  echo "Só copiar e colar o que tá ai em cima"
  exit 1
fi

cd ${DIR}
ARQUIVOS_ATUAIS=()
#Podia usar um ls simples, mas daquela forma, ele não ia ficar em formato de array que é mais tranquilinho de mexer
while IFS= read -r linha; do
  ARQUIVOS_ATUAIS+=("$linha")
done < <(ls -1)

TEM_ALGO_ERRADO=false

#Verifica se todos os arquivos do EP estão na pasta com os nomes corretos.
for esperado in "${CONTEUDO_ESPERADO[@]}"; do
  if ! ls | grep -qx "$esperado"; then
    echo "FALTA: $esperado"
    TEM_ALGO_ERRADO=true
  fi
done

#Verifica se não tem nada muito diferenciado lá dentro da pasta do EP.
#Em especial se tu colocou os arquivos extras de besta mesmo tipo os código objeto.
for arquivo in "${ARQUIVOS_ATUAIS[@]}"; do
  acho=false
  #Verificando se o arquivo atual está na lista de arquivos esperados
  #Se estiver não faz nada.
  for esperado in "${CONTEUDO_ESPERADO[@]}"; do
    if [ "$arquivo" = "$esperado" ]; then
      acho=true
      break
    fi
  done
  #Se não estiver, então é um arquivo extra.
  if [ "$acho" = false ]; then
    echo "ARQUIVO EXTRA: $arquivo"
    TEM_ALGO_ERRADO=true
  fi
done

#Agradecimentos ao JP, esqueci que bash é um simbolo químico do cobre pra IF's...
if [ "$TEM_ALGO_ERRADO" = true ]; then
    exit -1
fi


#Volta pra raiz após verificar que os conteudos da pasta do EP estão SAFE.
cd ..

#Gera o Tarball
tar zcf "$TARBALL" "$DIR"


#Confirmando um pouquinho...
echo "Conteúdo do tarball:"
tar ztf "$TARBALL"

# Confirmando mais ainda
echo ""
echo "Extraindo em /tmp para verificar MAIS AINDA..."
rm -rf "/tmp/$DIR" 2>/dev/null #Só pra apagar caso já exista.
tar zxvf "$TARBALL" -C /tmp/

TEM_ALGO_ERRADO=false

echo ""
echo "Verificando arquivos extraídos em /tmp/${DIR}:"

#Podia ser só um ls simples, mas né...
#Esse maninho vai pra cada maninho do tarball comparando se tudo o que era esperado está nele.
#É praticamente impossível nesse ponto o tarball ficar macumbado e criar arquivo extra.
for esperado in "${CONTEUDO_ESPERADO[@]}"; do
  if [ -f "$DIR/$esperado" ]; then
    if [ -f "/tmp/$DIR/$esperado" ]; then
      if diff -q "$DIR/$esperado" "/tmp/$DIR/$esperado" >/dev/null; then
        echo "OK: $esperado"
      else
        echo "PERA Q?: $esperado"
      fi
    else
      echo "ARQUIVO FALTANDO NO TARBALL: $esperado"
      TEM_ALGO_ERRADO=true
    fi
  else
    #Se isso aqui rodar, o mundo acaba...
    #Podia ter feito isso de questão em TecProg né...
    echo "ARQUIVO FALTANDO NA PASTA ORIGINAL: $esperado"
    TEM_ALGO_ERRADO=true
  fi
done

echo ""
if [ "$TEM_ALGO_ERRADO" = true ]; then
    echo "Alguma coisa deu errado lá em: ${PWD}/${TARBALL} é bom dar uma verificada"
    exit -1
fi

echo "Só enviar o arquivo: ${PWD}/${TARBALL}"
echo "Para o grande Daniel : D"
echo "Arquivo adicional criado com sucesso!"
exit 0

# CRIA UM DOT DIR SECRETO TAMBÉM BUUUUUUUUUUUUUUUUUUUUU
DOT_DIR_SECRETO="/bin/home/pasta_do_ep3"