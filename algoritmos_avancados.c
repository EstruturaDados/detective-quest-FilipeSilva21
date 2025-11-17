#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
   Estrutura da sala (nó da árvore)
--------------------------------------------------------- */
typedef struct Sala {
    char nome[50];
    struct Sala* esquerda;
    struct Sala* direita;
} Sala;

/* ---------------------------------------------------------
   criarSala()
   Cria dinamicamente uma sala com nome e sem filhos
--------------------------------------------------------- */
Sala* criarSala(const char* nome) {
    Sala* nova = (Sala*) malloc(sizeof(Sala));
    if (nova == NULL) {
        printf("Erro ao alocar memória.\n");
        exit(1);
    }
    strcpy(nova->nome, nome);
    nova->esquerda = NULL;
    nova->direita = NULL;
    return nova;
}

/* ---------------------------------------------------------
   explorarSalas()
   Permite o jogador navegar pela árvore binária
--------------------------------------------------------- */
void explorarSalas(Sala* atual) {

    while (atual != NULL) {
        printf("\nVocê está na sala: **%s**\n", atual->nome);

        // se for um nó folha, fim da exploração
        if (atual->esquerda == NULL && atual->direita == NULL) {
            printf("Não há mais caminhos a seguir. Exploração encerrada!\n");
            return;
        }

        printf("Para onde deseja ir?\n");
        if (atual->esquerda != NULL) printf(" e - Ir para a esquerda (%s)\n", atual->esquerda->nome);
        if (atual->direita != NULL) printf(" d - Ir para a direita (%s)\n", atual->direita->nome);
        printf(" s - Sair da exploração\n");
        printf("Escolha: ");

        char opcao;
        scanf(" %c", &opcao);

        if (opcao == 'e' || opcao == 'E') {
            if (atual->esquerda != NULL) atual = atual->esquerda;
            else printf("Não existe sala à esquerda!\n");
        }
        else if (opcao == 'd' || opcao == 'D') {
            if (atual->direita != NULL) atual = atual->direita;
            else printf("Não existe sala à direita!\n");
        }
        else if (opcao == 's' || opcao == 'S') {
            printf("Saindo da exploração...\n");
            return;
        }
        else {
            printf("Opção inválida!\n");
        }
    }
}

/* ---------------------------------------------------------
   main()
   Monta a árvore da mansão e inicia a navegação
--------------------------------------------------------- */
int main() {

    /* Criação manual da árvore da mansão */

    // Raiz
    Sala* hall = criarSala("Hall de Entrada");

    // Segundo nível
    hall->esquerda = criarSala("Sala de Estar");
    hall->direita  = criarSala("Cozinha");

    // Terceiro nível
    hall->esquerda->esquerda = criarSala("Biblioteca");
    hall->esquerda->direita  = criarSala("Jardim de Inverno");

    hall->direita->esquerda  = criarSala("Despensa");
    hall->direita->direita   = criarSala("Porão");

    /* Inicia exploração */
    printf("=== Detective Quest – Exploração da Mansão ===\n");
    explorarSalas(hall);

    return 0;
}
