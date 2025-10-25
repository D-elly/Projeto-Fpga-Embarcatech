#include <stdio.h>           // Inclui as funções padrão de entrada/saída
#include "pico/stdlib.h"     // Inclui as funções padrão da Pico SDK
#include "CartaoSD.h"        // Inclui a classe CartaoSD e ArquivoSd
#include "pico/util/queue.h" // Inclui a fila para comunicação 
#include "hardware/spi.h"    // Inclui a biblioteca SPI

// Definicoes SPI cartao SD e FPGA
// Configuração SPI do Cartão SD (SPI0)
#define SPI_CARTAO spi0
#define PINO_SPI_MISO_CARTAO 16u
#define PINO_SPI_MOSI_CARTAO 19u
#define PINO_SPI_SCK_CARTAO 18u
#define PINO_SPI_CS_CARTAO 17u

// Configuração SPI do FPGA (Usando SPI1 nos pinos do I2C da Bitdoglab)
#define SPI_FPGA spi1 
#define PINO_SPI_MISO_FPGA 0u   // GP0 - SCK para I2C, configurado como MISO
#define PINO_SPI_MOSI_FPGA 3u   // GP3 - SDA para I2C, configurado como MOSI
#define PINO_SPI_SCK_FPGA 2u    // GP2 - SCK para I2C, configurado como SCK
#define PINO_SPI_CS_FPGA 1u     // GP1 - SDA para I2C, configurado como CS

// Definicoes de audio e fila
#define SD_READ_BLOCK_SIZE 1024                 // Tamanho do buffer de leitura do SD
#define SAMPLE_QUEUE_CAPACITY 512               // Tamanho da Fila
#define SAMPLE_RATE 44100                       // Taxa de amostragem do arquivo WAV
#define SAMPLE_TIME_US (1000000 / SAMPLE_RATE)  // 1.000.000 us / 44100 Hz ≈ 22.67 µs

// Definição da Amostra: 16-bit estéreo (dois canais)
typedef struct {
    int16_t left;
    int16_t right;
} Sample16BitStereo;

queue_t sample_queue;               // Fila global
volatile bool end_of_file = false;  // Variável para indicar se a leitura do arquivo terminou

// Protótipos das funções
bool listarDiretorio(CartaoSD &cartao);
void setup_sample_queue();
void ler_e_encher_fila(ArquivoSd *wav_file);
void processar_amostra(Sample16BitStereo sample);
void setup_spi_fpga();

int main(){
    stdio_init_all();
    while (!stdio_usb_connected()) sleep_ms(100);
    printf("\r\nIniciando SD_CARD_LEITURA...\r\n");
    
    setup_sample_queue();   // Inicia a fila antes de abrir o arquivo
    setup_spi_fpga();       // Configura o SPI para o FPGA
    
    // Configuracao e montagem do cartao SD
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
    const char* nome_arquivo = "meu_audio.wav";  // <------------
    printf("\nTentando abrir arquivo: %s\r\n", nome_arquivo);

    ArquivoSd wav_file = cartao.abrir(nome_arquivo, MODO_LEITURA);  // Abre arquivo no modo leitura definido no CartaoSD.h
    if (wav_file.estaAberto()) {
        printf("Arquivo WAV aberto com sucesso.\n");
        
        // Buffer para ler o cabeçalho (44 bytes para um header WAV padrão)
        const int WAV_HEADER_SIZE = 44;
        uint8_t header_buffer[WAV_HEADER_SIZE];

        if (wav_file.lerBytes(header_buffer, WAV_HEADER_SIZE)) {
            printf("Cabecalho WAV lido (%d bytes).\n", WAV_HEADER_SIZE);
            ler_e_encher_fila(&wav_file); // Inicio leitura continua e enchimento da fila

        } else printf("Erro ao ler o cabecalho WAV.\n");

        wav_file.fechar();
        printf("Arquivo WAV fechado.\n");

    } else printf("Falha ao abrir o arquivo WAV: %s\n", nome_arquivo);

    // ------------------------ Consumo e transmissao do audio -------------------------------
    printf("\n--- Loop de Consumo (Transmissão SPI ao FPGA) ---\r\n");
    Sample16BitStereo current_sample;
    uint32_t samples_consumed = 0; // Mudado para uint32_t para evitar overflow
    
    // Loop de reprodução: termina quando o arquivo terminar E a fila esvaziar
    while (!end_of_file || !queue_is_empty(&sample_queue)) {
        // Tenta remover (não-bloqueante) uma amostra da fila
        if (queue_try_remove(&sample_queue, &current_sample)) {
            // Envia a amostra para o FPGA via SPI
            processar_amostra(current_sample);
            samples_consumed++;

            // Sincronização de Tempo (CRÍTICO: Impreciso!)  <---------------------
            // Tenta forçar a taxa de amostragem de 44.1 kHz
            sleep_us(SAMPLE_TIME_US);
            
            // Exemplo de status
            if (samples_consumed % 44100 == 0) { // A cada 1 segundo
                printf("Tempo: %lu s | Amostras: %lu | Fila atual: %d\r\n", samples_consumed / SAMPLE_RATE, samples_consumed, queue_get_level(&sample_queue));
            }

        } else {
            // A fila está vazia (underrun).
            if (!end_of_file) {
                printf("AVISO: Fila vazia (Underrun)! A leitura do SD precisa ser mais rápida.\r\n");
            }
            tight_loop_contents(); 
        }
    }
    printf("Transmissão SPI concluída. Total de amostras enviadas: %lu\r\n", samples_consumed);
    
    // ----------------------- Listagem diretorio e Desmontagem ------------------------------
    listarDiretorio(cartao);
    cartao.desmontarSistemaArquivos(); printf("Sistema de arquivos desmontado.\r\n");

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

            // Coloca cada amostra na fila. Tenta colocar na fila (bloqueia se a fila estiver cheia)
            for (size_t i = 0; i < num_samples; ++i) {
                queue_add_blocking(&sample_queue, &samples[i]);
            }
        }
        
        if (bytes_read < SD_READ_BLOCK_SIZE) {
            end_of_file = true;
            printf("Fim do arquivo alcançado. Total de dados lidos: %zu bytes.\r\n", total_bytes_read);
        }

    } while (bytes_read > 0);
}

// Funcao para processar/enviar a amostra lida
// O FPGA espera 32 bits por amostra (16 bits L + 16 bits R), e o Raspberry Pi Pico é Little Endian.
void processar_amostra(Sample16BitStereo sample) {
    uint8_t spi_tx_buffer[4]; 

    // Canal Esquerdo (Left) (16 bits)
    // Little Endian: Byte menos significativo primeiro (LSB)
    spi_tx_buffer[0] = (uint8_t)(sample.left & 0xFF);
    spi_tx_buffer[1] = (uint8_t)((sample.left >> 8) & 0xFF);

    // Canal Direito (Right) (16 bits)
    spi_tx_buffer[2] = (uint8_t)(sample.right & 0xFF);
    spi_tx_buffer[3] = (uint8_t)((sample.right >> 8) & 0xFF);
    
    // Ativa o Chip Select (CS) - Nível baixo (0)
    gpio_put(PINO_SPI_CS_FPGA, 0); 
    
    // Envia 4 bytes (32 bits) via SPI
    spi_write_blocking(SPI_FPGA, spi_tx_buffer, 4);

    // Desativa o Chip Select (CS) - Nível alto (1)
    gpio_put(PINO_SPI_CS_FPGA, 1);
}

void setup_spi_fpga() {
    // Configura o SPI1 para comunicação com o FPGA
    printf("Configurando SPI para o FPGA (Pinos: MOSI=GP3, SCK=GP2, CS=GP1).\r\n");

    // Inicializa o periférico SPI1. Taxa de clock: 10MHz <----------pode ser ajustada
    spi_init(SPI_FPGA, 1000 * 1000 * 10); 

    // Configura os GPIOs para a função SPI.
    gpio_set_function(PINO_SPI_MISO_FPGA, GPIO_FUNC_SPI);
    gpio_set_function(PINO_SPI_MOSI_FPGA, GPIO_FUNC_SPI);
    gpio_set_function(PINO_SPI_SCK_FPGA, GPIO_FUNC_SPI);

    // Configura o CS (Chip Select) manualmente como GPIO, pois faremos o controle
    // para cada amostra (ou em blocos) para garantir a sincronia.
    gpio_init(PINO_SPI_CS_FPGA);
    gpio_set_dir(PINO_SPI_CS_FPGA, GPIO_OUT);
    gpio_put(PINO_SPI_CS_FPGA, 1); // CS inativo (nível alto)
    
    printf("SPI FPGA configurado.\r\n");
}