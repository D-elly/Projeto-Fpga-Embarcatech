/* 
    Conferir analise do cabeçalho do arquivo de áudio WAV (pasta Analise_arquivo_WAV)
*/

#include <stdio.h>           // Inclui as funções padrão de entrada/saída
#include "pico/stdlib.h"     // Inclui as funções padrão da Pico SDK
#include "CartaoSD.h"        // Inclui a classe CartaoSD e ArquivoSd
#include "pico/util/queue.h" // Inclui a fila para comunicação 

// Definicoes e Variaveis globais 
#define SPI_CARTAO spi0
#define PINO_SPI_MISO_CARTAO 16u
#define PINO_SPI_MOSI_CARTAO 19u
#define PINO_SPI_SCK_CARTAO 18u
#define PINO_SPI_CS_CARTAO 17u

// Tamanho do buffer de leitura do SD
#define SD_READ_BLOCK_SIZE 1024

// Tamanho da Fila: Número de estruturas Sample16BitStereo que a fila pode armazenar
#define SAMPLE_QUEUE_CAPACITY 512

// Definição da Amostra: 16-bit estéreo (dois canais)
typedef struct {
    int16_t left;
    int16_t right;
} Sample16BitStereo;

// Fila global
queue_t sample_queue;

// Variável para indicar se a leitura do arquivo terminou
volatile bool end_of_file = false;

//comunicação pico -> FPGA

//#define SPI_CARTAO spi0 testar depois se é problemático
#define PINO_SPI_MISO_FPGA 0u
#define PINO_SPI_MOSI_FPGA 3u
#define PINO_SPI_SCK_FPGA 2u
#define PINO_SPI_CS_FPGA 1u

bool listarDiretorio(CartaoSD &cartao);
void setup_sample_queue();
void processar_amostra(Sample16BitStereo sample);
void ler_e_encher_fila(ArquivoSd *wav_file);

int main(){
    stdio_init_all();
    while (!stdio_usb_connected()) sleep_ms(100);

    // Loop de delay para o terminal serial iniciar
    for (int i = 0; i < 10; ++i) {
        printf(".");
        sleep_ms(100);
    } printf("\r\nIniciando SD_CARD_LEITURA...\r\n");

    // Inicia a fila antes de abrir o arquivo
    setup_sample_queue();
    
    CartaoSD cartao(SPI_CARTAO,
                    PINO_SPI_MISO_CARTAO,
                    PINO_SPI_MOSI_CARTAO,
                    PINO_SPI_SCK_CARTAO,
                    PINO_SPI_CS_CARTAO);

    if (!cartao.iniciarSpi()) printf("Falha ao iniciar SPI do cartão. Verifique as conexões.\r\n");
    else printf("SPI iniciado com sucesso.\r\n");

    if (!cartao.montarSistemaArquivos()) printf("Falha ao montar FAT. Cartão formatado?\r\n");
    else printf("Sistema de arquivos montado com sucesso.\r\n");
    
    // ------------------------------ Leitura WAV ---------------------------------------
    const char* nome_arquivo = "/input.wav";  // <------------
    printf("\nTentando abrir arquivo: %s\r\n", nome_arquivo);

    ArquivoSd wav_file = cartao.abrir(nome_arquivo, MODO_LEITURA);  // Abre arquivo no modo leitura definido no CartaoSD.h
    if (wav_file.estaAberto()) {
        printf("Arquivo WAV aberto com sucesso.\n");
        
        // Buffer para ler o cabeçalho (44 bytes para um header WAV padrão)
        const int WAV_HEADER_SIZE = 44;
        uint8_t header_buffer[WAV_HEADER_SIZE];
        
        // Lê os 44 bytes do cabeçalho
        if (wav_file.lerBytes(header_buffer, WAV_HEADER_SIZE)) {
            printf("Cabecalho WAV lido (%d bytes).\n", WAV_HEADER_SIZE);
            ler_e_encher_fila(&wav_file); // Inicio leitura continua e enchimento da fila

        } else printf("Erro ao ler o cabecalho WAV.\n");

        wav_file.fechar();
        printf("Arquivo WAV fechado.\n");

    } else printf("Falha ao abrir o arquivo WAV: %s\n", nome_arquivo);

    // ------------------------Simulando a reprodução de áudio-------------------------------
    printf("\n--- Loop de Consumo (Simulação de DAC) ---\r\n");
    Sample16BitStereo current_sample;
    int samples_consumed = 0;
    
    // Leitura contínua (e bloqueante) da fila
    while (!end_of_file || !queue_is_empty(&sample_queue)) {
        if (queue_try_remove(&sample_queue, &current_sample)) {
            processar_amostra(current_sample);
            samples_consumed++;
            
            // Exemplo de status
            if (samples_consumed % 10000 == 0) {
                 printf("Amostras consumidas: %d | Fila atual: %d\r\n", 
                        samples_consumed, queue_get_level(&sample_queue));
            }

        } else {
            // Se a fila estiver vazia E o arquivo ainda não terminou, espera por mais dados
            if (!end_of_file) {
                tight_loop_contents(); 
            }
        }
    }
    printf("Reprodução simulada concluída. Total de amostras consumidas: %d\r\n", samples_consumed);
    
    // -----------------------Listagem diretorio e Desmontagem------------------------------
    // Listagem do diretório raiz
    listarDiretorio(cartao);

    // Desmontar o sistema de arquivos
    cartao.desmontarSistemaArquivos();
    printf("Sistema de arquivos desmontado.\r\n");

    // Loop infinito para manter a placa ligada e o USB conectado
    while (true)  tight_loop_contents();
    return 0;
}

// Funcao para listar o diretorio raiz do cartao SD
bool listarDiretorio(CartaoSD &cartao){
    printf("\r\n--- Conteudo do Cartao SD ---\r\n");

    // Tenta abrir o diretório raiz ("/") no modo leitura
    ArquivoSd diretorio = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
    if (!diretorio.estaAberto()) {
        printf("Falha ao abrir o diretorio raiz.\r\n");
        return false;
    }

    while (true) {
        // Abre a próxima entrada (arquivo ou subdiretório)
        ArquivoSd entrada = diretorio.abrirProximaEntrada();
        if (!entrada.estaAberto()) {
            // Não há mais entradas
            break;
        }

        InformacoesEntradaFat info;
        // Obtém informações da entrada
        if (entrada.obterInformacoes(info)) {
            // Imprime o nome curto (8.3)
            // Se for um diretório, adiciona uma barra (opcional)
            printf("%s%s\r\n", 
                info.nome_curto, 
                (info.atributos & 0x10) ? "/" : ""
            );
        }
    }

    diretorio.fechar();
    printf("-------------------------------\r\n");
    return true;
}

// Funcao para iniciar a fila
void setup_sample_queue() {
    // Inicializa a fila: tamanho do item (Sample16BitStereo) e capacidade (SAMPLE_QUEUE_CAPACITY)
    queue_init(&sample_queue, sizeof(Sample16BitStereo), SAMPLE_QUEUE_CAPACITY);
    printf("Fila de amostras inicializada (capacidade: %d samples).\r\n", SAMPLE_QUEUE_CAPACITY);
}

// Funcao para processar/enviar a amostra lida (pode ser enviada para PWM/PIO/DAC aqui)
void processar_amostra(Sample16BitStereo sample) {
}

// Funcao para ler dados do arquivo WAV e encher a fila
void ler_e_encher_fila(ArquivoSd *wav_file) {
    uint8_t read_buffer[SD_READ_BLOCK_SIZE];
    size_t bytes_read = 0;
    size_t total_bytes_read = 0;

    printf("Iniciando leitura dos dados e preenchimento da fila...\r\n");

    do {
        // Lê um bloco de bytes do arquivo SD
        bytes_read = wav_file->lerBytes(read_buffer, SD_READ_BLOCK_SIZE);
        total_bytes_read += bytes_read;

        if (bytes_read > 0) {
            // Converte os bytes lidos em amostras estéreo de 16 bits
            // O número de amostras estéreo é bytes_read / (2 canais * 2 bytes/amostra)
            size_t num_samples = bytes_read / sizeof(Sample16BitStereo);
            
            // Cast do buffer de bytes para o array de amostras estéreo
            Sample16BitStereo *samples = (Sample16BitStereo *)read_buffer;

            // Coloca cada amostra na fila
            for (size_t i = 0; i < num_samples; ++i) {
                // Tenta colocar na fila (bloqueia se a fila estiver cheia)
                // Se a fila estiver cheia, significa que o DAC não está consumindo rápido o suficiente.
                // Aqui usamos `true` (blocking) para garantir que todos os dados sejam colocados.
                queue_add_blocking(&sample_queue, &samples[i]);
            }
        }
        
        if (bytes_read < SD_READ_BLOCK_SIZE) {
            end_of_file = true;
            printf("Fim do arquivo alcançado. Total de dados lidos: %zu bytes.\r\n", total_bytes_read);
        }

    } while (bytes_read > 0);
}
