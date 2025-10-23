#ifndef CONTROLADORSPICARTAO_H
#define CONTROLADORSPICARTAO_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"
#include "pico/mutex.h"

namespace cartao_sd {

class ControladorSpiCartao {
public:
    ControladorSpiCartao(spi_inst_t *instancia_spi,
                         uint8_t gpio_miso,
                         uint8_t gpio_mosi,
                         uint8_t gpio_sck,
                         uint8_t gpio_cs,
                         uint32_t frequencia_baixa_hz,
                         uint32_t frequencia_alta_hz);

    bool configurarHardware();
    void ajustarFrequenciaBaixa();
    void ajustarFrequenciaAlta();
    void enviarClocksInicializacao();
    void adquirirBarramento();
    void liberarBarramento();
    void desselecionarPulso();
    uint8_t transferirByte(uint8_t dado);
    bool transferirBuffer(const uint8_t *origem, uint8_t *destino, size_t quantidade);
    uint8_t obterGpioCs() const;

private:
    spi_inst_t *instanciaSpi;
    uint8_t gpioMiso;
    uint8_t gpioMosi;
    uint8_t gpioSck;
    uint8_t gpioCs;
    uint32_t frequenciaBaixaHz;
    uint32_t frequenciaAltaHz;
    bool hardwareInicializado;
    mutex_t mutexAcesso;

    void selecionar();
    void desselecionar();
};

} // namespace cartao_sd

#endif
