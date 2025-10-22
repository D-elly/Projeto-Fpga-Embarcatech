#ifndef DRIVERCARTAOSD_H
#define DRIVERCARTAOSD_H

#include <stddef.h>
#include <stdint.h>

#include "ControladorSpiCartao.h"

namespace cartao_sd {

class DriverCartaoSd {
public:
    explicit DriverCartaoSd(ControladorSpiCartao &controlador_spi);

    bool iniciar();
    bool estaInicializado() const;
    bool lerSetores(uint8_t *destino, uint32_t setor_inicial, uint32_t quantidade);
    bool escreverSetores(const uint8_t *origem, uint32_t setor_inicial, uint32_t quantidade);
    uint64_t obterQuantidadeSetores() const;

private:
    ControladorSpiCartao &controlador;
    bool cartaoInicializado;
    bool cartaoAltaCapacidade;
    uint64_t quantidadeSetores;

    bool enviarComando(uint8_t comando, uint32_t argumento, uint8_t *resposta, size_t tamanho_resposta);
    bool enviarComandoAplicativo(uint8_t comando, uint32_t argumento, uint8_t *resposta, size_t tamanho_resposta);
    bool aguardarPronto(uint32_t tempo_limite_ms);
    bool aguardarToken(uint8_t token, uint32_t tempo_limite_ms, uint8_t &valor_recebido);
    bool lerBloco(uint8_t *destino, uint32_t setor);
    bool escreverBloco(const uint8_t *origem, uint32_t setor);
    bool atualizarQuantidadeSetores();
    bool lerCsd(uint8_t *dados_csd, size_t tamanho_csd);
    uint32_t ajustarArgumentoSetor(uint32_t setor) const;
};

} // namespace cartao_sd

#endif
