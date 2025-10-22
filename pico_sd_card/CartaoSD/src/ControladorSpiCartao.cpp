#include "ControladorSpiCartao.h"

#include <string.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

namespace cartao_sd {

namespace {
constexpr uint8_t SPI_FILL_CHAR = 0xFFu;
}

ControladorSpiCartao::ControladorSpiCartao(spi_inst_t *instancia_spi,
                                           uint8_t gpio_miso,
                                           uint8_t gpio_mosi,
                                           uint8_t gpio_sck,
                                           uint8_t gpio_cs,
                                           uint32_t frequencia_baixa_hz,
                                           uint32_t frequencia_alta_hz)
    : instanciaSpi(instancia_spi),
      gpioMiso(gpio_miso),
      gpioMosi(gpio_mosi),
      gpioSck(gpio_sck),
      gpioCs(gpio_cs),
      frequenciaBaixaHz(frequencia_baixa_hz),
      frequenciaAltaHz(frequencia_alta_hz),
      hardwareInicializado(false) {
    mutex_init(&mutexAcesso);
}

bool ControladorSpiCartao::configurarHardware() {
    if (hardwareInicializado) {
        return true;
    }

    gpio_init(gpioCs);
    gpio_set_dir(gpioCs, GPIO_OUT);
    gpio_put(gpioCs, 1);

    spi_init(instanciaSpi, frequenciaBaixaHz);
    gpio_set_function(gpioMiso, GPIO_FUNC_SPI);
    gpio_set_function(gpioMosi, GPIO_FUNC_SPI);
    gpio_set_function(gpioSck, GPIO_FUNC_SPI);
    spi_set_format(instanciaSpi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    hardwareInicializado = true;
    return true;
}

void ControladorSpiCartao::ajustarFrequenciaBaixa() {
    if (!hardwareInicializado) {
        return;
    }

    spi_set_baudrate(instanciaSpi, frequenciaBaixaHz);
}

void ControladorSpiCartao::ajustarFrequenciaAlta() {
    if (!hardwareInicializado) {
        return;
    }

    spi_set_baudrate(instanciaSpi, frequenciaAltaHz);
}

void ControladorSpiCartao::enviarClocksInicializacao() {
    if (!hardwareInicializado) {
        return;
    }

    gpio_put(gpioCs, 1);

    uint8_t sequencia[10];
    memset(sequencia, 0xFF, sizeof(sequencia));
    transferirBuffer(sequencia, nullptr, sizeof(sequencia));
}

void ControladorSpiCartao::selecionar() {
    gpio_put(gpioCs, 0);
    spi_write_blocking(instanciaSpi, &SPI_FILL_CHAR, 1);
}

void ControladorSpiCartao::desselecionar() {
    gpio_put(gpioCs, 1);
    spi_write_blocking(instanciaSpi, &SPI_FILL_CHAR, 1);
}

void ControladorSpiCartao::adquirirBarramento() {
    mutex_enter_blocking(&mutexAcesso);
    selecionar();
}

void ControladorSpiCartao::liberarBarramento() {
    desselecionar();
    mutex_exit(&mutexAcesso);
}

void ControladorSpiCartao::desselecionarPulso() {
    desselecionar();
    sleep_us(2);
    selecionar();
}

uint8_t ControladorSpiCartao::transferirByte(uint8_t dado) {
    uint8_t recebido = 0u;
    spi_write_read_blocking(instanciaSpi, &dado, &recebido, 1);
    return recebido;
}

bool ControladorSpiCartao::transferirBuffer(const uint8_t *origem, uint8_t *destino, size_t quantidade) {
    if (quantidade == 0) {
        return true;
    }

    if (origem == nullptr && destino == nullptr) {
        return false;
    }

    const uint8_t *ponteiro_origem = origem;
    uint8_t *ponteiro_destino = destino;
    size_t indice = 0;

    while (indice < quantidade) {
        uint8_t dado_envio = SPI_FILL_CHAR;
        if (ponteiro_origem != nullptr) {
            dado_envio = ponteiro_origem[indice];
        }

        uint8_t recebido = SPI_FILL_CHAR;
        spi_write_read_blocking(instanciaSpi, &dado_envio, &recebido, 1);

        if (ponteiro_destino != nullptr) {
            ponteiro_destino[indice] = recebido;
        }

        indice = indice + 1;
    }

    return true;
}

uint8_t ControladorSpiCartao::obterGpioCs() const {
    return gpioCs;
}

} // namespace cartao_sd
