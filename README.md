# Aula_SO2_THREADS_BATHROOM

## Simulador de Banheiros com Threads (SO2)

Disciplina: Sistemas Operacionais II – SO2
Tema: Gerência de processos e threads

Este projeto é um **simulador de uso de banheiros** desenvolvido em **C** utilizando **pthread**, com foco em:  
- Concorrência  
- Exclusão mútua  
- Sincronização com variáveis de condição  
- Fila FIFO (justa)  

Foi criado como material didático para a disciplina **Sistemas Operacionais II (SO2)**.

---

## Funcionalidades

- Simulação de múltiplas pessoas concorrendo por 2 banheiros disponíveis.  
- Controle de acesso justo via sistema de tickets (FIFO).  
- Uso de threads para representar as pessoas.  
- Temporização de chegadas e tempo de uso com distribuição exponencial.  
- Modos de visualização:  
  - `-v` → snapshots a cada evento (chegada, entrada, saída).  
  - `-V` → painel ao vivo, atualizando a fila e os banheiros em tempo real.  
- Logs opcionais com `-d` (modo debug).  

---

## Como compilar

### Linux
bash
gcc -O2 -Wall -pthread -o banheiros banheiros.c -lm
Windows (MinGW)
bash
gcc -O2 -Wall -pthread -o banheiros.exe banheiros.c -lm

## Como executar:

Exemplo simples
./banheiros -n 10 -i 200 -t 600 -s 42 -v

Exemplo em modo painel ao vivo
./banheiros -n 30 -i 120 -t 500 -s 123 -d -V

Parâmetros disponíveis
Opção	Descrição
-n	Número de pessoas (threads)
-i	Tempo médio (ms) entre chegadas
-t	Tempo médio (ms) de uso do banheiro
-s	Seed do gerador de números aleatórios
-d	Ativa logs de debug
-v	Mostra snapshots a cada evento
-V	Mostra painel ao vivo (com atualização de tela)

## Conceitos trabalhados

- Concorrência com múltiplas threads.
- Exclusão mútua com pthread_mutex_t.
- Sincronização com pthread_cond_t.
- Fila justa (FIFO) utilizando tickets.
- Distribuições probabilísticas (exponencial) para chegadas e serviços.
- Visualização opcional do estado da simulação.

