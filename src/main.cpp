#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>

int NUM_JOGADORES = 4; // Número de jogadores (será configurado pelo usuário)

// Semáforos
std::counting_semaphore<>* cadeiras_sem;
std::binary_semaphore inicio_rodada(0);
std::binary_semaphore fim_rodada(0);

// Controle
std::mutex cout_mutex;
std::atomic<bool> musica_parada{false};
std::atomic<bool> jogo_ativo{true};
std::atomic<int> jogadores_finalizaram{0};

// Jogadores ativos
std::vector<int> jogadores_ativos;
std::mutex jogadores_mutex;

// Cada jogador controla se está ativo
std::vector<bool> jogador_ativo;

// Função que simula o comportamento de cada jogador
void jogador_func(int id) {
    while (jogo_ativo && jogador_ativo[id]) {
        inicio_rodada.acquire();  // Espera a música parar

        if (!jogo_ativo || !jogador_ativo[id])
            break;

        bool conseguiu_cadeira = cadeiras_sem->try_acquire();

        if (!conseguiu_cadeira) {
            // Jogador perde
            {
                std::lock_guard<std::mutex> lock(jogadores_mutex);
                jogadores_ativos.erase(std::remove(jogadores_ativos.begin(), jogadores_ativos.end(), id), jogadores_ativos.end());
            }
            jogador_ativo[id] = false;
        }

        jogadores_finalizaram++;
        fim_rodada.release(); // Avisa coordenador
    }
}

// Função coordenadora para gerenciar o jogo
void coordenador_func() {
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "-----------------------------------------------\n";
        std::cout << "Bem-vindo ao Jogo das Cadeiras Concorrente!\n";
        std::cout << "-----------------------------------------------\n\n";
    }

    while (true) {
        int jogadores_rodada;
        {
            std::lock_guard<std::mutex> lock(jogadores_mutex);
            jogadores_rodada = jogadores_ativos.size();
        }

        if (jogadores_rodada == 1)
            break; // Só sobra um

        int cadeiras = jogadores_rodada - 1;

        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Iniciando rodada com " << jogadores_rodada
                      << " jogadores e " << cadeiras << " cadeiras.\n";
            std::cout << "A musica esta tocando... 🎵\n\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 2000 + 1000)); // Música

        // Música para
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "-----------------------------------------------\n";
        }

        // Prepara o semáforo de cadeiras
        delete cadeiras_sem;
        cadeiras_sem = new std::counting_semaphore<>(cadeiras);

        jogadores_finalizaram = 0;

        for (int id = 1; id <= NUM_JOGADORES; ++id) {
            if (jogador_ativo[id])
                inicio_rodada.release(); // Só libera vivos
        }

        // Espera todos vivos terminarem
        for (int i = 0; i < jogadores_rodada; ++i)
            fim_rodada.acquire();

        // Mostra resultado
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            int cadeiras_ocupadas = jogadores_rodada - 1;
            int index = 1;

            for (int id : jogadores_ativos) {
                if (cadeiras_ocupadas > 0) {
                    std::cout << "[Cadeira " << index++ << "]: Ocupada por P" << id << "\n";
                    cadeiras_ocupadas--;
                }
            }

            // Identifica o último eliminado da rodada
            int ultimo_eliminado = -1;
            for (int id = 1; id <= NUM_JOGADORES; ++id) {
                if (!jogador_ativo[id]) {
                    ultimo_eliminado = id;  // O último jogador eliminado
                }
            }

            // Se houver um eliminado, imprime o último
            if (ultimo_eliminado != -1) {
                std::cout << "Jogador P" << ultimo_eliminado << " nao conseguiu uma cadeira e foi eliminado!\n";
            }

            std::cout << "-----------------------------------------------\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Pausa
    }

    // Vencedor
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "🏆 Vencedor: Jogador P" << jogadores_ativos.front() << "! Parabens! 🏆\n";
        std::cout << "-----------------------------------------------\n";
        std::cout << "Obrigado por jogar o Jogo das Cadeiras Concorrente!\n";
    }

    jogo_ativo = false;

    for (int id = 1; id <= NUM_JOGADORES; ++id) {
        if (jogador_ativo[id])
            inicio_rodada.release();
    }
}

int main() {
    srand(time(nullptr));

    // Solicita o número de jogadores
    std::cout << "Digite o numero de jogadores: ";
    std::cin >> NUM_JOGADORES;

    // Inicializa o vetor de jogadores ativos
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_ativos.push_back(i);
    }

    jogador_ativo.resize(NUM_JOGADORES + 1, true);  // Usar vetor bool com o tamanho correto

    cadeiras_sem = new std::counting_semaphore<>(0);

    std::vector<std::thread> threads;
    for (int i = 1; i <= NUM_JOGADORES; ++i)
        threads.emplace_back(jogador_func, i);

    std::thread coordenador(coordenador_func);

    for (auto& t : threads)
        t.join();

    coordenador.join();

    delete cadeiras_sem;

    return 0;
}
