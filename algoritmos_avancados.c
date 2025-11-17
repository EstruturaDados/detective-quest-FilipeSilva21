/*
 Detective Quest - Sistema de exploração, coleta de pistas e julgamento final
 Autor: Filipe silva
 Compilação: gcc -std=c11 -Wall -Wextra -o detective detective.c

 Estruturas principais:
 - Árvore binária de salas (Sala)
 - BST (árvore de busca) de pistas coletadas (NoPista)
 - Tabela hash (chaining) que mapeia pista -> suspeito (HashTable)
 
 Funções importantes (documentadas nos comentários):
 - criarSala()            : cria dinamicamente um cômodo (Sala)
 - explorarSalas()        : navega pela árvore de salas e coleta pistas
 - inserirPista()         : insere pista na BST (evita duplicatas)
 - inserirNaHash()        : insere associação pista -> suspeito na hash
 - encontrarSuspeito()    : consulta o suspeito associado a uma pista na hash
 - verificarSuspeitoFinal(): valida se a acusação tem >= 2 pistas a favor
 - várias funções utilitárias (listar, liberar memória, hash, etc.)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ----------------------------- Estruturas ----------------------------- */

/* Nó da árvore de salas (mapa da mansão) */
typedef struct Sala {
    char *nome;
    struct Sala *esq;
    struct Sala *dir;
} Sala;

/* Nó da BST de pistas coletadas (ordenada por string) */
typedef struct NoPista {
    char *pista;
    struct NoPista *esq;
    struct NoPista *dir;
} NoPista;

/* Entrada para chaining na hash (pista -> suspeito) */
typedef struct HashEntry {
    char *pista;
    char *suspeito;
    struct HashEntry *prox;
} HashEntry;

/* Tabela hash simples (vetor de ponteiros para HashEntry) */
typedef struct HashTable {
    HashEntry **buckets;
    size_t size; // número de buckets
} HashTable;

/* --------------------------- Utilitárias ------------------------------ */

/* strdup compatível (algumas implementações exigem definir _POSIX_C_SOURCE,
   aqui fornecemos nossa própria implementação para portabilidade). */
char* strdup_local(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*) malloc(n);
    if (!p) {
        fprintf(stderr, "Erro de memória em strdup_local\n");
        exit(EXIT_FAILURE);
    }
    memcpy(p, s, n);
    return p;
}

/* Trim: remove espaços e nova linha do começo e fim */
void trim_inplace(char *s) {
    // remover \n e \r do fim
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == '\r')) {
        s[len-1] = '\0'; len--;
    }
    // remover espaços no início
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    // remover espaços no fim
    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) {
        s[len-1] = '\0'; len--;
    }
}

/* --------------------------- Criação de salas ------------------------- */

/*
 criarSala()
 Cria dinamicamente um nó Sala com o nome informado.
 Retorna ponteiro para Sala alocada.
*/
Sala* criarSala(const char* nome) {
    Sala* s = (Sala*) malloc(sizeof(Sala));
    if (!s) {
        fprintf(stderr, "Falha na alocação de Sala\n");
        exit(EXIT_FAILURE);
    }
    s->nome = strdup_local(nome);
    s->esq = s->dir = NULL;
    return s;
}

/* Libera memória da árvore de salas (pós-ordem) */
void liberarSalas(Sala* raiz) {
    if (!raiz) return;
    liberarSalas(raiz->esq);
    liberarSalas(raiz->dir);
    free(raiz->nome);
    free(raiz);
}

/* --------------------------- BST de pistas ---------------------------- */

/*
 inserirPista()
 Insere uma pista (string) na árvore de pistas (BST) de forma ordenada.
 Evita duplicatas (se já existe, não insere novamente).
 Retorna a nova raiz da BST (pode ser a mesma).
*/
NoPista* inserirPista(NoPista* raiz, const char* pista) {
    if (!pista) return raiz;
    if (!raiz) {
        NoPista* n = (NoPista*) malloc(sizeof(NoPista));
        if (!n) { fprintf(stderr, "Erro de memória em inserirPista\n"); exit(EXIT_FAILURE); }
        n->pista = strdup_local(pista);
        n->esq = n->dir = NULL;
        return n;
    }
    int cmp = strcmp(pista, raiz->pista);
    if (cmp == 0) {
        // já coletada, não insere duplicata
        return raiz;
    } else if (cmp < 0) {
        raiz->esq = inserirPista(raiz->esq, pista);
    } else {
        raiz->dir = inserirPista(raiz->dir, pista);
    }
    return raiz;
}

/* Busca se pista já foi coletada; retorna 1 se encontrada, 0 caso contrário */
int buscaPista(NoPista* raiz, const char* pista) {
    if (!raiz || !pista) return 0;
    int cmp = strcmp(pista, raiz->pista);
    if (cmp == 0) return 1;
    if (cmp < 0) return buscaPista(raiz->esq, pista);
    return buscaPista(raiz->dir, pista);
}

/* Impressão em ordem (lexicográfica) das pistas coletadas */
void listarPistas(NoPista* raiz) {
    if (!raiz) return;
    listarPistas(raiz->esq);
    printf(" - %s\n", raiz->pista);
    listarPistas(raiz->dir);
}

/* Libera memória da BST de pistas (pós-ordem) */
void liberarPistas(NoPista* raiz) {
    if (!raiz) return;
    liberarPistas(raiz->esq);
    liberarPistas(raiz->dir);
    free(raiz->pista);
    free(raiz);
}

/* ------------------------------ Hash -------------------------------- */

/* Função hash djb2 (string -> unsigned long) */
unsigned long hash_djb2(const char* str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash;
}

/* Cria tabela hash com 'size' buckets */
HashTable* criarHash(size_t size) {
    HashTable* ht = (HashTable*) malloc(sizeof(HashTable));
    if (!ht) { fprintf(stderr, "Erro de memória criarHash\n"); exit(EXIT_FAILURE); }
    ht->size = size;
    ht->buckets = (HashEntry**) calloc(size, sizeof(HashEntry*));
    if (!ht->buckets) { fprintf(stderr, "Erro de memória criarHash buckets\n"); exit(EXIT_FAILURE); }
    return ht;
}

/*
 inserirNaHash()
 Insere a associação pista -> suspeito na tabela hash.
 Se a pista já existir, sobrescreve o suspeito (comportamento simples).
*/
void inserirNaHash(HashTable* ht, const char* pista, const char* suspeito) {
    if (!ht || !pista || !suspeito) return;
    unsigned long h = hash_djb2(pista) % ht->size;
    HashEntry* cur = ht->buckets[h];
    // procura se já existe
    for (; cur; cur = cur->prox) {
        if (strcmp(cur->pista, pista) == 0) {
            // substitui suspeito
            free(cur->suspeito);
            cur->suspeito = strdup_local(suspeito);
            return;
        }
    }
    // insere novo no início da lista
    HashEntry* e = (HashEntry*) malloc(sizeof(HashEntry));
    if (!e) { fprintf(stderr, "Erro de memória inserirNaHash\n"); exit(EXIT_FAILURE); }
    e->pista = strdup_local(pista);
    e->suspeito = strdup_local(suspeito);
    e->prox = ht->buckets[h];
    ht->buckets[h] = e;
}

/*
 encontrarSuspeito()
 Retorna o nome do suspeito associado à pista (ou NULL se não houver).
 A string retornada é a mesma armazenada na hash (não liberar externamente).
*/
const char* encontrarSuspeito(HashTable* ht, const char* pista) {
    if (!ht || !pista) return NULL;
    unsigned long h = hash_djb2(pista) % ht->size;
    for (HashEntry* cur = ht->buckets[h]; cur; cur = cur->prox) {
        if (strcmp(cur->pista, pista) == 0) return cur->suspeito;
    }
    return NULL;
}

/* Libera memória da hash (todas as entradas) */
void liberarHash(HashTable* ht) {
    if (!ht) return;
    for (size_t i = 0; i < ht->size; ++i) {
        HashEntry* cur = ht->buckets[i];
        while (cur) {
            HashEntry* tmp = cur->prox;
            free(cur->pista);
            free(cur->suspeito);
            free(cur);
            cur = tmp;
        }
    }
    free(ht->buckets);
    free(ht);
}

/* ------------------- Associação sala -> pista (regras) --------------- */

/*
 getPistaParaSala()
 Retorna a pista associada a uma sala dada seu nome (se existir).
 As regras são codificadas aqui — ajuste conforme quiser.
 Retorna uma string literal (não liberar).
*/
const char* getPistaParaSala(const char* nomeSala) {
    if (!nomeSala) return NULL;
    // Regras codificadas - aqui definimos as pistas associadas às salas
    if (strcmp(nomeSala, "Hall de Entrada") == 0) return "pegada molhada";
    if (strcmp(nomeSala, "Sala de Estar") == 0) return "fio de cabelo";
    if (strcmp(nomeSala, "Biblioteca") == 0) return "bilhete rasgado";
    if (strcmp(nomeSala, "Jardim de Inverno") == 0) return "marca de luva";
    if (strcmp(nomeSala, "Cozinha") == 0) return "cheiro de queimado";
    if (strcmp(nomeSala, "Despensa") == 0) return "chave estranha";
    if (strcmp(nomeSala, "Porão") == 0) return "mancha de tinta";
    if (strcmp(nomeSala, "Quarto Principal") == 0) return "anel riscado";
    if (strcmp(nomeSala, "Escritório") == 0) return "nota de dívida";
    // outras salas sem pista explícita:
    return NULL;
}

/* --------------------------- Exploração ------------------------------ */

/*
 explorarSalas()
 Navega interativamente pela árvore de salas.
 Ao visitar cada sala:
  - exibe o nome
  - verifica se existe pista associada e, se existir, coleta (insere na BST)
  - permite escolher 'e' (esquerda), 'd' (direita) ou 's' (sair)
 Parâmetros:
  - atual: nó atual (começar pelo Hall)
  - raizPistas: ponteiro para a raiz da BST de pistas (será atualizado)
  - ht: tabela hash (para exibir qual suspeito está associado, se desejar)
*/
void explorarSalas(Sala* atual, NoPista** raizPistas, HashTable* ht) {
    if (!atual) return;
    Sala* node = atual;
    char op;

    while (node != NULL) {
        printf("\nVocê está na sala: %s\n", node->nome);

        // verificar pista associada por regras
        const char* pista = getPistaParaSala(node->nome);
        if (pista) {
            printf("Pista encontrada: \"%s\"\n", pista);
            // verifica se já foi coletada
            if (!buscaPista(*raizPistas, pista)) {
                *raizPistas = inserirPista(*raizPistas, pista);
                // opcional: mostrar suspeito associado (se existir)
                const char* s = encontrarSuspeito(ht, pista);
                if (s) {
                    printf("-> Esta pista aponta para o(a) suspeito(a): %s\n", s);
                } else {
                    printf("-> Nenhum suspeito associado a esta pista.\n");
                }
            } else {
                printf("Você já coletou esta pista antes.\n");
            }
        } else {
            printf("Nenhuma pista encontrada nesta sala.\n");
        }

        // Se for nó folha sem filhos, avisa e pergunta se quer sair ou voltar
        if (node->esq == NULL && node->dir == NULL) {
            printf("Esta sala não tem caminhos adicionais (nó-folha).\n");
        }

        // opções de navegação
        printf("\nPara onde deseja ir?\n");
        if (node->esq) printf(" e - Ir para a esquerda (%s)\n", node->esq->nome);
        if (node->dir) printf(" d - Ir para a direita (%s)\n", node->dir->nome);
        printf(" s - Sair da exploração\n");
        printf("Escolha: ");

        if (scanf(" %c", &op) != 1) {
            // limpar entrada e sair em caso de erro
            int c; while ((c = getchar()) != '\n' && c != EOF) {}
            printf("Entrada inválida. Saindo da exploração.\n");
            return;
        }
        // limpar resto da linha
        int c; while ((c = getchar()) != '\n' && c != EOF) {}

        if (op == 'e' || op == 'E') {
            if (node->esq) node = node->esq;
            else printf("Não existe caminho à esquerda.\n");
        } else if (op == 'd' || op == 'D') {
            if (node->dir) node = node->dir;
            else printf("Não existe caminho à direita.\n");
        } else if (op == 's' || op == 'S') {
            printf("Saindo da exploração...\n");
            return;
        } else {
            printf("Opção inválida. Tente novamente.\n");
        }
    }
}

/* ---------------------- Verificação da acusação ---------------------- */

/*
 verificarSuspeitoFinal()
 Percorre as pistas coletadas (BST) e conta quantas delas apontam para
 o suspeito acusado (usando a tabela hash).
 Retorna o número de pistas que apontam para esse suspeito.
*/
int verificarSuspeitoFinal(NoPista* raizPistas, HashTable* ht, const char* acusado) {
    if (!acusado) return 0;
    // percurso em ordem (pode ser qualquer percurso, pois queremos contar)
    int contador = 0;
    // usar uma pilha recursiva simples
    if (!raizPistas) return 0;
    // Escrevemos uma função interna recursiva via ponteiro-função (C não possui nested funcs portáveis)
    // portanto implementamos auxiliar externa abaixo.
    // Para simplicidade, faremos um traversal recursivo com função auxiliar:
    // (definida mais abaixo)
    // Chamamos o auxiliar:
    extern void auxiliarContagem(NoPista*, HashTable*, const char*, int*);
    auxiliarContagem(raizPistas, ht, acusado, &contador);
    return contador;
}

/* Auxiliar recursivo para contar pistas que apontam para 'acusado' */
void auxiliarContagem(NoPista* raiz, HashTable* ht, const char* acusado, int* contador) {
    if (!raiz) return;
    auxiliarContagem(raiz->esq, ht, acusado, contador);
    const char* s = encontrarSuspeito(ht, raiz->pista);
    if (s && strcmp(s, acusado) == 0) {
        (*contador)++;
    }
    auxiliarContagem(raiz->dir, ht, acusado, contador);
}

/* ----------------------------- Main --------------------------------- */

int main(void) {
    printf("=== Detective Quest (modo texto) ===\n");
    printf("Bem-vindo(a). Explore a mansão, colete pistas e acuse o culpado.\n");

    /* ---------- Montar mapa fixo (árvore de salas) ---------- */
    // Exemplo de mapa:
    //                Hall de Entrada
    //               /               \
    //        Sala de Estar         Cozinha
    //        /         \           /     \
    //  Biblioteca  JardimInv   Despensa  Porão
    // (podemos adicionar mais salas se desejado)
    Sala* hall = criarSala("Hall de Entrada");
    hall->esq = criarSala("Sala de Estar");
    hall->dir = criarSala("Cozinha");

    hall->esq->esq = criarSala("Biblioteca");
    hall->esq->dir = criarSala("Jardim de Inverno");

    hall->dir->esq = criarSala("Despensa");
    hall->dir->dir = criarSala("Porão");

    // adicionamos duas salas extras conectadas para mostrar mais opções
    hall->esq->esq->esq = criarSala("Quarto Principal");
    hall->dir->esq->esq = criarSala("Escritório");

    /* ---------- Criar e popular tabela hash (pista -> suspeito) ---------- */
    HashTable* ht = criarHash(101); // 101 buckets (primo razoável)

    // Definir associações: ajuste conforme enredo do jogo
    inserirNaHash(ht, "pegada molhada", "Sr. Avelar");
    inserirNaHash(ht, "fio de cabelo", "Sra. Beatriz");
    inserirNaHash(ht, "marca de luva", "Sr. Avelar");
    inserirNaHash(ht, "bilhete rasgado", "Srta. Clara");
    inserirNaHash(ht, "chave estranha", "Sra. Beatriz");
    inserirNaHash(ht, "mancha de tinta", "Sr. Avelar");
    inserirNaHash(ht, "cheiro de queimado", "Sr. Dourado");
    inserirNaHash(ht, "anel riscado", "Srta. Clara");
    inserirNaHash(ht, "nota de dívida", "Sr. Dourado");

    /* ---------- BST de pistas coletadas (inicialmente vazia) ---------- */
    NoPista* raizPistas = NULL;

    /* ---------- Exploração (interativa) ---------- */
    explorarSalas(hall, &raizPistas, ht);

    /* ---------- Fase final: listar pistas e acusar ---------- */
    printf("\n=== Fase final: Pistas coletadas ===\n");
    if (!raizPistas) {
        printf("Você não coletou nenhuma pista.\n");
    } else {
        printf("Pistas coletadas (ordem alfabética):\n");
        listarPistas(raizPistas);
    }

    // pedir acusação
    char buf[256];
    printf("\nDigite o nome do suspeito que deseja acusar (ex.: \"Sr. Avelar\"): ");
    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("Erro ao ler entrada. Encerrando.\n");
        // liberar e sair
        liberarPistas(raizPistas);
        liberarHash(ht);
        liberarSalas(hall);
        return 0;
    }
    trim_inplace(buf);
    if (strlen(buf) == 0) {
        printf("Nenhum suspeito informado. Encerrando sem julgamento.\n");
        liberarPistas(raizPistas);
        liberarHash(ht);
        liberarSalas(hall);
        return 0;
    }

    // verificar quantas pistas apontam para o acusado
    int contador = verificarSuspeitoFinal(raizPistas, ht, buf);
    printf("\nVocê acusou: %s\n", buf);
    printf("Número de pistas coletadas que apontam para %s: %d\n", buf, contador);

    if (contador >= 2) {
        printf("\nResultado: ACUSAÇÃO SUSTENTADA!\n");
        printf("%s tem pelo menos %d pistas que o(a) ligam ao crime.\n", buf, contador);
    } else {
        printf("\nResultado: ACUSAÇÃO INSUFICIENTE.\n");
        printf("São necessárias ao menos 2 pistas apontando para o acusado, mas apenas %d foram encontradas.\n", contador);
    }

    /* ---------- Limpeza de memória ---------- */
    liberarPistas(raizPistas);
    liberarHash(ht);
    liberarSalas(hall);

    printf("\nObrigado por jogar Detective Quest (modo texto).\n");
    return 0;
}
