#include "DriverCartaoSd.h"

#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace cartao_sd {

namespace {
constexpr uint32_t TAMANHO_SETOR_BYTES = 512u;
constexpr uint8_t COMANDO_GO_IDLE = 0u;
constexpr uint8_t COMANDO_SEND_IF_COND = 8u;
constexpr uint8_t COMANDO_SEND_CSD = 9u;
constexpr uint8_t COMANDO_STOP_TRANSMISSION = 12u;
constexpr uint8_t COMANDO_SET_BLOCKLEN = 16u;
constexpr uint8_t COMANDO_READ_SINGLE = 17u;
constexpr uint8_t COMANDO_WRITE_SINGLE = 24u;
constexpr uint8_t COMANDO_APP_CMD = 55u;
constexpr uint8_t COMANDO_READ_OCR = 58u;
constexpr uint8_t COMANDO_APP_SEND_OP_COND = 41u;
constexpr uint8_t COMANDO_APP_SET_WR_BLK = 23u;
constexpr uint8_t TOKEN_INICIO_DADOS = 0xFEu;
constexpr uint8_t RESPOSTA_IDLE = 0x01u;
constexpr uint8_t RESPOSTA_PRONTA = 0x00u;
constexpr uint32_t ARGUMENTO_HCS = 0x40000000u;
constexpr uint8_t MASCARA_RESPOSTA_ERRO = 0x80u;
constexpr uint8_t MASCARA_RESPOSTA_ESCRITA = 0x1Fu;
constexpr uint8_t RESPOSTA_ESCRITA_OK = 0x05u;
constexpr uint32_t TEMPO_TIMEOUT_COMANDO_MS = 200u;
constexpr uint32_t TEMPO_TIMEOUT_DADOS_MS = 500u;
constexpr uint32_t TEMPO_TIMEOUT_INICIALIZACAO_MS = 1000u;
constexpr uint8_t CRC_CMD0 = 0x95u;
constexpr uint8_t CRC_CMD8 = 0x87u;
}

DriverCartaoSd::DriverCartaoSd(ControladorSpiCartao &controlador_spi)
    : controlador(controlador_spi),
      cartaoInicializado(false),
      cartaoAltaCapacidade(false),
      quantidadeSetores(0u) {}

bool DriverCartaoSd::iniciar() {
    if (cartaoInicializado) {
        return true;
    }

    if (!controlador.configurarHardware()) {
        return false;
    }

    controlador.ajustarFrequenciaBaixa();
    controlador.enviarClocksInicializacao();

    controlador.adquirirBarramento();

    uint8_t resposta_cmd0[1] = {0};
    if (!enviarComando(COMANDO_GO_IDLE, 0u, resposta_cmd0, sizeof(resposta_cmd0))) {
        controlador.liberarBarramento();
        return false;
    }

    if (resposta_cmd0[0] != RESPOSTA_IDLE) {
        controlador.liberarBarramento();
        return false;
    }

    uint8_t resposta_cmd8[5] = {0};
    if (!enviarComando(COMANDO_SEND_IF_COND, 0x000001AAu, resposta_cmd8, sizeof(resposta_cmd8))) {
        controlador.liberarBarramento();
        return false;
    }

    if (resposta_cmd8[0] != RESPOSTA_IDLE) {
        controlador.liberarBarramento();
        return false;
    }

    bool iniciou = false;
    absolute_time_t tempo_limite = make_timeout_time_ms(TEMPO_TIMEOUT_INICIALIZACAO_MS);

    while (absolute_time_diff_us(get_absolute_time(), tempo_limite) > 0) {
        uint8_t resposta_acmd[1] = {0};
        bool enviou = enviarComandoAplicativo(COMANDO_APP_SEND_OP_COND, ARGUMENTO_HCS, resposta_acmd, sizeof(resposta_acmd));
        if (!enviou) {
            controlador.liberarBarramento();
            return false;
        }

        if (resposta_acmd[0] == RESPOSTA_PRONTA) {
            iniciou = true;
            break;
        }

        sleep_ms(10);
    }

    if (!iniciou) {
        controlador.liberarBarramento();
        return false;
    }

    uint8_t resposta_cmd58[5] = {0};
    if (!enviarComando(COMANDO_READ_OCR, 0u, resposta_cmd58, sizeof(resposta_cmd58))) {
        controlador.liberarBarramento();
        return false;
    }

    if (resposta_cmd58[0] != RESPOSTA_PRONTA) {
        controlador.liberarBarramento();
        return false;
    }

    cartaoAltaCapacidade = (resposta_cmd58[1] & 0x40u) != 0;

    if (!cartaoAltaCapacidade) {
        uint8_t resposta_cmd16[1] = {0};
        if (!enviarComando(COMANDO_SET_BLOCKLEN, TAMANHO_SETOR_BYTES, resposta_cmd16, sizeof(resposta_cmd16))) {
            controlador.liberarBarramento();
            return false;
        }

        if (resposta_cmd16[0] != RESPOSTA_PRONTA) {
            controlador.liberarBarramento();
            return false;
        }
    }

    controlador.liberarBarramento();

    controlador.ajustarFrequenciaAlta();

    if (!atualizarQuantidadeSetores()) {
        return false;
    }

    cartaoInicializado = true;
    return true;
}

bool DriverCartaoSd::estaInicializado() const {
    return cartaoInicializado;
}

bool DriverCartaoSd::lerSetores(uint8_t *destino, uint32_t setor_inicial, uint32_t quantidade) {
    if (!cartaoInicializado) {
        return false;
    }

    if (destino == nullptr) {
        return false;
    }

    uint32_t indice = 0;

    while (indice < quantidade) {
        uint32_t setor_atual = setor_inicial + indice;
        uint8_t *destino_bloco = destino + (indice * TAMANHO_SETOR_BYTES);

        bool leu = lerBloco(destino_bloco, setor_atual);
        if (!leu) {
            return false;
        }

        indice = indice + 1;
    }

    return true;
}

bool DriverCartaoSd::escreverSetores(const uint8_t *origem, uint32_t setor_inicial, uint32_t quantidade) {
    if (!cartaoInicializado) {
        return false;
    }

    if (origem == nullptr) {
        return false;
    }

    uint32_t indice = 0;

    while (indice < quantidade) {
        uint32_t setor_atual = setor_inicial + indice;
        const uint8_t *origem_bloco = origem + (indice * TAMANHO_SETOR_BYTES);

        bool escreveu = escreverBloco(origem_bloco, setor_atual);
        if (!escreveu) {
            return false;
        }

        indice = indice + 1;
    }

    return true;
}

uint64_t DriverCartaoSd::obterQuantidadeSetores() const {
    return quantidadeSetores;
}

bool DriverCartaoSd::enviarComando(uint8_t comando, uint32_t argumento, uint8_t *resposta, size_t tamanho_resposta) {
    if (resposta == nullptr || tamanho_resposta == 0) {
        return false;
    }

    uint8_t pacote[6];
    pacote[0] = static_cast<uint8_t>(0x40u | comando);
    pacote[1] = static_cast<uint8_t>((argumento >> 24u) & 0xFFu);
    pacote[2] = static_cast<uint8_t>((argumento >> 16u) & 0xFFu);
    pacote[3] = static_cast<uint8_t>((argumento >> 8u) & 0xFFu);
    pacote[4] = static_cast<uint8_t>(argumento & 0xFFu);

    uint8_t crc = 0xFFu;
    if (comando == COMANDO_GO_IDLE) {
        crc = CRC_CMD0;
    } else if (comando == COMANDO_SEND_IF_COND) {
        crc = CRC_CMD8;
    }

    pacote[5] = crc;

    controlador.transferirBuffer(pacote, nullptr, sizeof(pacote));

    absolute_time_t tempo_limite = make_timeout_time_ms(TEMPO_TIMEOUT_COMANDO_MS);

    while (absolute_time_diff_us(get_absolute_time(), tempo_limite) > 0) {
        uint8_t valor = controlador.transferirByte(0xFFu);
        if ((valor & MASCARA_RESPOSTA_ERRO) == 0u) {
            resposta[0] = valor;

            size_t indice = 1;
            while (indice < tamanho_resposta) {
                resposta[indice] = controlador.transferirByte(0xFFu);
                indice = indice + 1;
            }

            return true;
        }
    }

    return false;
}

bool DriverCartaoSd::enviarComandoAplicativo(uint8_t comando, uint32_t argumento, uint8_t *resposta, size_t tamanho_resposta) {
    uint8_t resposta_cmd55[1] = {0};
    bool enviou_cmd55 = enviarComando(COMANDO_APP_CMD, 0u, resposta_cmd55, sizeof(resposta_cmd55));
    if (!enviou_cmd55) {
        return false;
    }

    return enviarComando(comando, argumento, resposta, tamanho_resposta);
}

bool DriverCartaoSd::aguardarPronto(uint32_t tempo_limite_ms) {
    absolute_time_t tempo_limite = make_timeout_time_ms(tempo_limite_ms);

    while (absolute_time_diff_us(get_absolute_time(), tempo_limite) > 0) {
        uint8_t valor = controlador.transferirByte(0xFFu);
        if (valor == 0xFFu) {
            return true;
        }
    }

    return false;
}

bool DriverCartaoSd::aguardarToken(uint8_t token, uint32_t tempo_limite_ms, uint8_t &valor_recebido) {
    absolute_time_t tempo_limite = make_timeout_time_ms(tempo_limite_ms);

    while (absolute_time_diff_us(get_absolute_time(), tempo_limite) > 0) {
        uint8_t valor = controlador.transferirByte(0xFFu);
        if (valor == token) {
            valor_recebido = valor;
            return true;
        }

        if (valor != 0xFFu) {
            valor_recebido = valor;
            return false;
        }
    }

    valor_recebido = 0xFFu;
    return false;
}

bool DriverCartaoSd::lerBloco(uint8_t *destino, uint32_t setor) {
    controlador.adquirirBarramento();

    uint8_t resposta_cmd[1] = {0};
    uint32_t argumento = ajustarArgumentoSetor(setor);

    bool enviou = enviarComando(COMANDO_READ_SINGLE, argumento, resposta_cmd, sizeof(resposta_cmd));
    if (!enviou || resposta_cmd[0] != RESPOSTA_PRONTA) {
        controlador.liberarBarramento();
        return false;
    }

    uint8_t token = 0u;
    bool recebeu_token = aguardarToken(TOKEN_INICIO_DADOS, TEMPO_TIMEOUT_DADOS_MS, token);
    if (!recebeu_token || token != TOKEN_INICIO_DADOS) {
        controlador.liberarBarramento();
        return false;
    }

    bool leu = controlador.transferirBuffer(nullptr, destino, TAMANHO_SETOR_BYTES);
    controlador.transferirByte(0xFFu);
    controlador.transferirByte(0xFFu);

    controlador.liberarBarramento();
    return leu;
}

bool DriverCartaoSd::escreverBloco(const uint8_t *origem, uint32_t setor) {
    controlador.adquirirBarramento();

    bool pronto = aguardarPronto(TEMPO_TIMEOUT_DADOS_MS);
    if (!pronto) {
        controlador.liberarBarramento();
        return false;
    }

    uint8_t resposta_cmd[1] = {0};
    uint32_t argumento = ajustarArgumentoSetor(setor);

    bool enviou = enviarComando(COMANDO_WRITE_SINGLE, argumento, resposta_cmd, sizeof(resposta_cmd));
    if (!enviou || resposta_cmd[0] != RESPOSTA_PRONTA) {
        controlador.liberarBarramento();
        return false;
    }

    controlador.transferirByte(TOKEN_INICIO_DADOS);

    bool escreveu = controlador.transferirBuffer(origem, nullptr, TAMANHO_SETOR_BYTES);
    controlador.transferirByte(0xFFu);
    controlador.transferirByte(0xFFu);

    uint8_t resposta_dados = controlador.transferirByte(0xFFu);
    bool aceitou = (resposta_dados & MASCARA_RESPOSTA_ESCRITA) == RESPOSTA_ESCRITA_OK;

    bool finalizou = aguardarPronto(TEMPO_TIMEOUT_DADOS_MS);

    controlador.liberarBarramento();

    return escreveu && aceitou && finalizou;
}

bool DriverCartaoSd::atualizarQuantidadeSetores() {
    uint8_t csd[16];
    bool leu_csd = lerCsd(csd, sizeof(csd));
    if (!leu_csd) {
        return false;
    }

    uint8_t versao = (csd[0] >> 6u) & 0x03u;

    if (versao == 1u) {
        uint32_t c_size = ((uint32_t)(csd[7] & 0x3Fu) << 16u) |
                          ((uint32_t)csd[8] << 8u) |
                          csd[9];
        quantidadeSetores = (static_cast<uint64_t>(c_size) + 1u) * 1024u;
        return true;
    }

    uint32_t c_size = (((uint32_t)(csd[6] & 0x03u)) << 10u) |
                      ((uint32_t)csd[7] << 2u) |
                      ((csd[8] & 0xC0u) >> 6u);

    uint32_t c_size_mult = ((csd[9] & 0x03u) << 1u) | ((csd[10] & 0x80u) >> 7u);
    uint32_t block_len = csd[5] & 0x0Fu;

    uint32_t mult = 1u << (c_size_mult + 2u);
    uint32_t block_len_bytes = 1u << block_len;
    uint64_t capacidade = (static_cast<uint64_t>(c_size) + 1u) * mult * block_len_bytes;

    quantidadeSetores = capacidade / TAMANHO_SETOR_BYTES;
    return quantidadeSetores != 0u;
}

bool DriverCartaoSd::lerCsd(uint8_t *dados_csd, size_t tamanho_csd) {
    if (dados_csd == nullptr || tamanho_csd != 16u) {
        return false;
    }

    controlador.adquirirBarramento();

    uint8_t resposta_cmd[1] = {0};
    bool enviou = enviarComando(COMANDO_SEND_CSD, 0u, resposta_cmd, sizeof(resposta_cmd));
    if (!enviou || resposta_cmd[0] != RESPOSTA_PRONTA) {
        controlador.liberarBarramento();
        return false;
    }

    uint8_t token = 0u;
    bool recebeu_token = aguardarToken(TOKEN_INICIO_DADOS, TEMPO_TIMEOUT_DADOS_MS, token);
    if (!recebeu_token || token != TOKEN_INICIO_DADOS) {
        controlador.liberarBarramento();
        return false;
    }

    bool leu = controlador.transferirBuffer(nullptr, dados_csd, tamanho_csd);
    controlador.transferirByte(0xFFu);
    controlador.transferirByte(0xFFu);

    controlador.liberarBarramento();
    return leu;
}

uint32_t DriverCartaoSd::ajustarArgumentoSetor(uint32_t setor) const {
    if (cartaoAltaCapacidade) {
        return setor;
    }

    return setor * TAMANHO_SETOR_BYTES;
}

} // namespace cartao_sd
