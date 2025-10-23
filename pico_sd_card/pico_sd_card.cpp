#include <stdio.h>
#include "pico/stdlib.h"
#include "CartaoSD.h"

// Possivelmente necessário incluir o cabeçalho para struct InformacoesEntradaFat
// Se CartaoSD.h não o contiver, você precisará encontrar qual header o define.
// Por enquanto, vamos assumir que está disponível.
// #include "ff15/InformacoesEntradaFat.h" // Exemplo, pode variar

#define SPI_CARTAO spi0
#define PINO_SPI_MISO_CARTAO 16u
#define PINO_SPI_MOSI_CARTAO 19u
#define PINO_SPI_SCK_CARTAO 18u
#define PINO_SPI_CS_CARTAO 17u


// ==========================================================
// NOVA FUNÇÃO PARA LISTAR O DIRETÓRIO
// ==========================================================
bool listarDiretorio(CartaoSD &cartao)
{
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


// ==========================================================
// FUNÇÃO MAIN ATUALIZADA
// ==========================================================
int main()
{
    stdio_init_all();
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }

    // Loop de delay para o terminal serial iniciar (opcional, mas recomendado)
    for (int i = 0; i < 10; ++i) {
        printf(".");
        sleep_ms(100);
    }
    printf("\r\nIniciando SD_CARD_LEITURA...\r\n");

    CartaoSD cartao(SPI_CARTAO,
                    PINO_SPI_MISO_CARTAO,
                    PINO_SPI_MOSI_CARTAO,
                    PINO_SPI_SCK_CARTAO,
                    PINO_SPI_CS_CARTAO);

    if (!cartao.iniciarSpi()) {
        printf("Falha ao iniciar SPI do cartão. Verifique as conexões.\r\n");
        // Não retornar 1, para que o programa não pare.
        // O loop 'while(true)' no final segura o programa.
    } else {
        printf("SPI iniciado com sucesso.\r\n");
    }

    if (!cartao.montarSistemaArquivos()) {
        printf("Falha ao montar FAT. Cartão formatado?\r\n");
        // Não retornar 1
    } else {
        printf("Sistema de arquivos montado com sucesso.\r\n");

        // // --- Operação de ESCRITA de LOGS ---
        // ArquivoSd arquivo = cartao.abrir("/logs.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
        // if (arquivo.estaAberto()) {
        //     arquivo.escreverLinha("Inicializacao concluida");
        //     arquivo.sincronizar();
        //     arquivo.fechar();
        //     printf("Linha de log gravada em /logs.txt\r\n");
        // } else {
        //     printf("Falha ao abrir/criar /logs.txt\r\n");
        // }

        // OPERAÇÃO DE LEITURA WAV ---
        const char* nome_arquivo = "/input.wav";
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
                
                // Leitura dos dados brutos (exemplo: ler os próximos 256 bytes de áudio)
                const int READ_SIZE = 256;
                uint8_t audio_buffer[READ_SIZE];
                
                if (wav_file.lerBytes(audio_buffer, READ_SIZE)) {
                    printf("Primeiros %d bytes de dados de audio lidos do WAV.\n", READ_SIZE);
                    // A partir daqui pode-se enviar os dados para um DAC (Digital-to-Analog Converter) ou processaria o áudio.
                } else {
                    printf("Erro ao ler os dados de audio.\n");
                }
                
            } else {
                printf("Erro ao ler o cabecalho WAV.\n");
            }

            if (header_buffer[0] == 'R' && header_buffer[1] == 'I' && 
            header_buffer[2] == 'F' && header_buffer[3] == 'F') {
            
            printf("VERIFICACAO 1 (RIFF): OK\n");
            
            // 'WAVE' está no offset 8..11 do cabeçalho RIFF (formato RIFF/WAVE padrão).
            // Verificamos no offset correto e então extraímos os campos do subchunk 'fmt '.
            if (header_buffer[8] == 'W' && header_buffer[9] == 'A' && 
                header_buffer[10] == 'V' && header_buffer[11] == 'E') {

                printf("VERIFICACAO 2 (WAVE): OK\n");

                // Offsets padrão dentro do subchunk 'fmt ' (assumindo header WAV PCM padrão):
                // 20-21: AudioFormat (1 = PCM)
                // 22-23: NumChannels
                // 24-27: SampleRate (32-bit little endian)
                uint16_t audio_format = (uint16_t)(header_buffer[20] | (header_buffer[21] << 8));
                uint16_t num_channels = (uint16_t)(header_buffer[22] | (header_buffer[23] << 8));
                uint32_t sample_rate = (uint32_t)(header_buffer[24] | (header_buffer[25] << 8) | 
                                                (header_buffer[26] << 16) | (header_buffer[27] << 24));

                printf("Formato: %04X, Canais: %d, Sample Rate: %lu Hz\n", 
                        audio_format, num_channels, sample_rate);

                // 4. Se as verificações passarem, você pode ler os dados de áudio de forma confiável
                // ... (Seu loop de leitura de áudio de 256 bytes viria aqui) ...

            } else {
                printf("VERIFICACAO 2 (WAVE): FALHOU. Nao eh um arquivo WAVE valido.\n");
            }

        } else {
            printf("VERIFICACAO 1 (RIFF): FALHOU. Nao eh um arquivo RIFF (WAV).\n");
}

            wav_file.fechar();
            printf("Arquivo WAV fechado.\n");

        } else {
            printf("Falha ao abrir o arquivo WAV: %s\n", nome_arquivo);
        }
        
        // Listagem do diretório raiz
        listarDiretorio(cartao);

        // Desmontar o sistema de arquivos
        cartao.desmontarSistemaArquivos();
        printf("Sistema de arquivos desmontado.\r\n");
    }
    
    // Loop infinito para manter a placa ligada e o USB conectado
    while (true) {
        tight_loop_contents();
    }
    return 0;
}