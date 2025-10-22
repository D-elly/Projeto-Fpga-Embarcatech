#ifndef CARTAOSD_H
#define CARTAOSD_H

#include <stddef.h>
#include <stdint.h>

#include "hardware/spi.h"

#include "ControladorSpiCartao.h"
#include "DriverCartaoSd.h"
#include "ff.h"

#ifdef HABILITAR_LOG_CARTAO_SD
#include <stdio.h>
#define CARTAO_SD_LOG(...) printf(__VA_ARGS__)
#else
#define CARTAO_SD_LOG(...)
#endif

constexpr uint8_t MODO_LEITURA = 0x01u;
constexpr uint8_t MODO_ESCRITA = 0x02u;
constexpr uint8_t MODO_ACRESCENTAR = 0x04u;
constexpr uint8_t MODO_DIRETORIO = 0x08u;

struct CarimboTempoFat {
    uint16_t data;
    uint16_t hora;
};

struct InformacoesEntradaFat {
    uint64_t tamanho_bytes;
    uint16_t data_modificacao;
    uint16_t hora_modificacao;
    uint8_t atributos;
    char nome_curto[FF_SFN_BUF + 1];
#if FF_USE_LFN
    char nome_alternativo[FF_SFN_BUF + 1];
    char nome_completo[FF_LFN_BUF + 1];
#endif
};

struct EstatisticaEspacoLivreFat {
    uint32_t clusters_livres;
    uint32_t clusters_totais;
    uint32_t setores_por_cluster;
    uint32_t bytes_por_setor;
};

struct ParametrosFormatacaoFat {
    BYTE formato;
    BYTE quantidade_fats;
    uint32_t alinhamento_setores;
    uint32_t entradas_raiz;
    uint32_t tamanho_cluster_bytes;
};

struct ContextoBuscaFat {
    DIR diretorio;
    bool ativo;
};

using FuncaoEncaminhamentoFat = UINT (*)(const BYTE*, UINT);

class ArquivoSd {
public:
    ArquivoSd();
    bool fechar();
    bool escreverTexto(const char* texto);
    size_t escreverBytes(const uint8_t* dados, size_t tamanho);
    size_t lerBytes(uint8_t* buffer, size_t tamanho);
    int lerCaractere();
    long disponivel();
    int espiar();
    bool buscar(long posicao);
    long posicao();
    long tamanho();
    bool nome(char* destino, size_t capacidade);
    bool eDiretorio();
    ArquivoSd abrirProximaEntrada();
    bool reiniciarDiretorio();
    bool estaAberto();
    bool truncar();
    bool sincronizar();
    bool encaminharDados(FuncaoEncaminhamentoFat funcao_encaminhamento, UINT bytes_transferir, UINT &bytes_processados);
    bool expandir(FSIZE_t tamanho_desejado, bool preencher_com_zero);
    bool escreverCaractere(char caractere);
    bool escreverLinha(const char* texto);
    template<typename... Argumentos>
    int escreverFormatado(const char* formato, Argumentos... argumentos) {
        if (!validoParaArquivo()) {
            return -1;
        }
        if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
            CARTAO_SD_LOG("arquivo não aberto para escrita\r\n");
            return -1;
        }
        if (!abrirParaAcrescentar()) {
            return -1;
        }
        int quantidade_escrita = f_printf(&arquivo, formato, argumentos...);
        if (quantidade_escrita < 0) {
            registrarResultado(static_cast<FRESULT>(f_error(&arquivo)));
            return -1;
        }
        registrarResultado(FR_OK);
        return quantidade_escrita;
    }
    bool lerLinha(char* destino, size_t capacidade);
    bool estaNoFim() const;
    BYTE obterCodigoErro() const;
    FRESULT resultadoOperacao() const;
    bool reiniciarPosicao();
    bool obterInformacoes(InformacoesEntradaFat &destino) const;
private:
    FIL arquivo;
    DIR diretorio;
    FILINFO infoEntrada;
    bool aberto;
    bool ehDiretorio;
    bool ehEntradaEnumerada;
    int modoAbertura;
    mutable FRESULT ultimoResultado;
    static constexpr size_t TAMANHO_MAXIMO_CAMINHO = 256u;
    static constexpr size_t TAMANHO_MAXIMO_NOME = 256u;
    char caminho[TAMANHO_MAXIMO_CAMINHO];
    bool validoParaArquivo();
    bool validoParaDiretorio();
    bool abrirParaAcrescentar();
    void invalidar();
    void registrarResultado(FRESULT resultado);
    friend class CartaoSD;
};

class CartaoSD {
public:
    CartaoSD(spi_inst_t* instanciaSpi, uint8_t gpioMiso, uint8_t gpioMosi, uint8_t gpioSck, uint8_t gpioCs);
    ~CartaoSD();
    bool iniciarSpi();
    bool montarSistemaArquivos();
    bool desmontarSistemaArquivos();
    bool existeCaminho(const char* caminho);
    bool criarDiretorio(const char* caminho);
    bool removerDiretorio(const char* caminho);
    bool removerDiretorioRecursivo(const char* caminho);
    bool removerArquivo(const char* caminho);
    ArquivoSd abrir(const char* caminho, int modo);
    bool renomear(const char* caminho_original, const char* caminho_destino);
    bool obterInformacoes(const char* caminho, InformacoesEntradaFat &destino);
    bool alterarAtributos(const char* caminho, uint8_t atributos, uint8_t mascara);
    bool alterarHorario(const char* caminho, const CarimboTempoFat &tempo);
    bool alterarDiretorioAtual(const char* caminho);
    bool alterarUnidadeAtual(const char* unidade);
    bool obterDiretorioAtual(char* destino, size_t capacidade);
    bool obterEspacoLivre(const char* caminho, EstatisticaEspacoLivreFat &destino);
    bool obterRotulo(const char* caminho, char* destino_rotulo, size_t capacidade, uint32_t &numero_serie);
    bool definirRotulo(const char* rotulo);
    bool formatar(const char* caminho, const ParametrosFormatacaoFat &parametros, void* area_trabalho, size_t tamanho_area);
    bool criarParticoes(uint8_t unidade_fisica, const LBA_t tabela_particoes[], void* area_trabalho);
    bool definirPaginaCodigo(uint16_t codigo_pagina);
    bool buscarPrimeiro(const char* caminho, const char* padrao, InformacoesEntradaFat &destino, ContextoBuscaFat &contexto);
    bool buscarProximo(InformacoesEntradaFat &destino, ContextoBuscaFat &contexto);
    bool finalizarBusca(ContextoBuscaFat &contexto);
    FRESULT resultadoOperacao() const;
private:
    cartao_sd::ControladorSpiCartao controladorSpi;
    cartao_sd::DriverCartaoSd driverSd;
    FATFS sistemaArquivos;
    bool montado;
    const char* unidadeLogica;
    mutable FRESULT ultimoResultado;
    bool garantirInicio();
    static constexpr uint32_t FREQUENCIA_SPI_BAIXA = 400000u;
    static constexpr uint32_t FREQUENCIA_SPI_ALTA = 12500000u;
};

#endif
