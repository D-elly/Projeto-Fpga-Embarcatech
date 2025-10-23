#include "CartaoSD.h"

#include <stdio.h>
#include <string.h>

#include "FatFsPort.h"

namespace {

constexpr size_t TAMANHO_CAMINHO_TRABALHO = 512u;

void limparInformacoesEntrada(InformacoesEntradaFat &destino) {
    destino.tamanho_bytes = 0u;
    destino.data_modificacao = 0u;
    destino.hora_modificacao = 0u;
    destino.atributos = 0u;
    memset(destino.nome_curto, 0, sizeof(destino.nome_curto));
#if FF_USE_LFN
    memset(destino.nome_alternativo, 0, sizeof(destino.nome_alternativo));
    memset(destino.nome_completo, 0, sizeof(destino.nome_completo));
#endif
}

void converterFilinfoParaInformacoes(const FILINFO &origem, InformacoesEntradaFat &destino) {
    limparInformacoesEntrada(destino);
    destino.tamanho_bytes = static_cast<uint64_t>(origem.fsize);
    destino.data_modificacao = origem.fdate;
    destino.hora_modificacao = origem.ftime;
    destino.atributos = origem.fattrib;
    if (origem.altname[0] != 0) {
        strncpy(destino.nome_curto, origem.altname, sizeof(destino.nome_curto) - 1u);
    } else {
        strncpy(destino.nome_curto, origem.fname, sizeof(destino.nome_curto) - 1u);
    }
#if FF_USE_LFN
    strncpy(destino.nome_completo, origem.fname, sizeof(destino.nome_completo) - 1u);
    strncpy(destino.nome_alternativo, origem.altname, sizeof(destino.nome_alternativo) - 1u);
#endif
}

MKFS_PARM converterParametrosFormatacao(const ParametrosFormatacaoFat &origem) {
    MKFS_PARM parametros;
    parametros.fmt = origem.formato;
    parametros.n_fat = origem.quantidade_fats;
    parametros.align = origem.alinhamento_setores;
    parametros.n_root = origem.entradas_raiz;
    parametros.au_size = origem.tamanho_cluster_bytes;
    return parametros;
}

} // namespace

ArquivoSd::ArquivoSd() {
    aberto = false;
    ehDiretorio = false;
    ehEntradaEnumerada = false;
    modoAbertura = 0;
    ultimoResultado = FR_OK;
    memset(caminho, 0, sizeof(caminho));
    memset(&arquivo, 0, sizeof(arquivo));
    memset(&diretorio, 0, sizeof(diretorio));
    memset(&infoEntrada, 0, sizeof(infoEntrada));
}

bool ArquivoSd::validoParaArquivo() {
    if (!aberto) {
        CARTAO_SD_LOG("arquivo não está aberto\r\n");
        return false;
    }
    if (ehDiretorio) {
        CARTAO_SD_LOG("handle é de diretório\r\n");
        return false;
    }
    if (ehEntradaEnumerada) {
        CARTAO_SD_LOG("entrada de diretório não é arquivo aberto\r\n");
        return false;
    }
    return true;
}

bool ArquivoSd::validoParaDiretorio() {
    if (!aberto) {
        CARTAO_SD_LOG("diretório não está aberto\r\n");
        return false;
    }
    if (!ehDiretorio) {
        CARTAO_SD_LOG("handle não é diretório\r\n");
        return false;
    }
    if (ehEntradaEnumerada) {
        CARTAO_SD_LOG("entrada enumerada não é diretório aberto\r\n");
        return false;
    }
    return true;
}

bool ArquivoSd::fechar() {
    if (!aberto) {
        ultimoResultado = FR_OK;
        return true;
    }
    if (ehEntradaEnumerada) {
        registrarResultado(FR_OK);
        invalidar();
        return true;
    }
    FRESULT resultado = ehDiretorio ? f_closedir(&diretorio) : f_close(&arquivo);
    registrarResultado(resultado);
    if (resultado != FR_OK) {
        return false;
    }
    invalidar();
    return true;
}

bool ArquivoSd::abrirParaAcrescentar() {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & MODO_ACRESCENTAR) == 0) {
        return true;
    }
    FSIZE_t tamanho_arquivo = f_size(&arquivo);
    FRESULT resultado_seek = f_lseek(&arquivo, tamanho_arquivo);
    registrarResultado(resultado_seek);
    if (resultado_seek == FR_OK) {
        return true;
    }
    CARTAO_SD_LOG("falha ao posicionar para fim do arquivo\r\n");
    return false;
}

bool ArquivoSd::escreverTexto(const char* texto) {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para escrita\r\n");
        return false;
    }
    if (!abrirParaAcrescentar()) {
        return false;
    }
    int quantidade_escrita = f_printf(&arquivo, "%s", texto);
    if (quantidade_escrita < 0) {
        registrarResultado(static_cast<FRESULT>(f_error(&arquivo)));
        return false;
    }
    registrarResultado(FR_OK);
    return true;
}

size_t ArquivoSd::escreverBytes(const uint8_t* dados, size_t tamanho) {
    if (!validoParaArquivo()) {
        return 0;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para escrita\r\n");
        return 0;
    }
    if (!abrirParaAcrescentar()) {
        return 0;
    }
    if (!dados || tamanho == 0) {
        registrarResultado(FR_OK);
        return 0;
    }

    UINT quantidade_escrita = 0;
    FRESULT resultado_escrita = f_write(&arquivo, dados, (UINT)tamanho, &quantidade_escrita);
    registrarResultado(resultado_escrita);
    if (resultado_escrita != FR_OK) {
        return 0;
    }
    return static_cast<size_t>(quantidade_escrita);
}

size_t ArquivoSd::lerBytes(uint8_t* buffer, size_t tamanho) {
    if (!validoParaArquivo()) {
        return 0;
    }
    UINT quantidade_lida = 0;
    FRESULT resultado_leitura = f_read(&arquivo, buffer, (UINT)tamanho, &quantidade_lida);
    registrarResultado(resultado_leitura);
    if (resultado_leitura != FR_OK) {
        return 0;
    }
    return static_cast<size_t>(quantidade_lida);
}

int ArquivoSd::lerCaractere() {
    if (!validoParaArquivo()) {
        return -1;
    }
    uint8_t caractere = 0;
    UINT quantidade_lida = 0;
    FRESULT resultado_leitura = f_read(&arquivo, &caractere, 1, &quantidade_lida);
    registrarResultado(resultado_leitura);
    if (resultado_leitura != FR_OK) {
        return -1;
    }
    if (quantidade_lida == 0) {
        return -1;
    }
    return static_cast<int>(caractere);
}

long ArquivoSd::disponivel() {
    if (!validoParaArquivo()) {
        return 0;
    }
    FSIZE_t tamanho_total = f_size(&arquivo);
    FSIZE_t posicao_atual = f_tell(&arquivo);
    if (tamanho_total < posicao_atual) return 0;
    return (long)(tamanho_total - posicao_atual);
}

int ArquivoSd::espiar() {
    if (!validoParaArquivo()) {
        return -1;
    }
    FSIZE_t posicao_atual = f_tell(&arquivo);
    uint8_t caractere = 0;
    UINT quantidade_lida = 0;
    FRESULT resultado_leitura = f_read(&arquivo, &caractere, 1, &quantidade_lida);
    registrarResultado(resultado_leitura);
    if (resultado_leitura != FR_OK) {
        return -1;
    }
    FRESULT resultado_seek = f_lseek(&arquivo, posicao_atual);
    registrarResultado(resultado_seek);
    if (resultado_seek != FR_OK) {
        return -1;
    }
    if (quantidade_lida == 0) {
        return -1;
    }
    return static_cast<int>(caractere);
}

bool ArquivoSd::buscar(long posicao) {
    if (!validoParaArquivo()) {
        return false;
    }
    if (posicao < 0) {
        return false;
    }
    FRESULT resultado_seek = f_lseek(&arquivo, (FSIZE_t)posicao);
    registrarResultado(resultado_seek);
    if (resultado_seek == FR_OK) {
        return true;
    }
    CARTAO_SD_LOG("falha ao buscar posição\r\n");
    return false;
}

long ArquivoSd::posicao() {
    if (!validoParaArquivo()) {
        return -1;
    }
    return static_cast<long>(f_tell(&arquivo));
}

long ArquivoSd::tamanho() {
    if (!validoParaArquivo()) {
        return -1;
    }
    return static_cast<long>(f_size(&arquivo));
}

bool ArquivoSd::nome(char* destino, size_t capacidade) {
    if (!destino) return false;
    if (capacidade == 0) return false;
    if (!aberto) return false;
    size_t tamanho_nome = strlen(caminho);
    if (tamanho_nome + 1 > capacidade) return false;
    memcpy(destino, caminho, tamanho_nome + 1);
    return true;
}

bool ArquivoSd::eDiretorio() {
    if (!aberto) return false;
    return ehDiretorio;
}

ArquivoSd ArquivoSd::abrirProximaEntrada() {
    ArquivoSd handle_invalido;
    if (!validoParaDiretorio()) {
        return handle_invalido;
    }
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    FRESULT resultado_leitura = f_readdir(&diretorio, &informacao);
    registrarResultado(resultado_leitura);
    if (resultado_leitura != FR_OK) {
        return handle_invalido;
    }
    if (informacao.fname[0] == 0) {
        registrarResultado(FR_OK);
        return handle_invalido;
    }
    ArquivoSd entrada_handle;
    entrada_handle.aberto = true;
    entrada_handle.ehDiretorio = (informacao.fattrib & AM_DIR) ? true : false;
    entrada_handle.modoAbertura = MODO_LEITURA;
    entrada_handle.ehEntradaEnumerada = true;
    snprintf(entrada_handle.caminho, sizeof(entrada_handle.caminho), "%s/%s", caminho, informacao.fname);
    memcpy(&entrada_handle.infoEntrada, &informacao, sizeof(FILINFO));
    entrada_handle.ultimoResultado = resultado_leitura;
    return entrada_handle;
}

bool ArquivoSd::reiniciarDiretorio() {
    if (!validoParaDiretorio()) {
        return false;
    }
    FRESULT resultado = f_rewinddir(&diretorio);
    registrarResultado(resultado);
    return resultado == FR_OK;
}

bool ArquivoSd::estaAberto() {
    return aberto;
}

void ArquivoSd::invalidar() {
    aberto = false;
    ehDiretorio = false;
    ehEntradaEnumerada = false;
    caminho[0] = 0;
    modoAbertura = 0;
    ultimoResultado = FR_OK;
    memset(&infoEntrada, 0, sizeof(infoEntrada));
}

bool ArquivoSd::truncar() {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para truncar\r\n");
        return false;
    }
    FRESULT resultado = f_truncate(&arquivo);
    registrarResultado(resultado);
    return resultado == FR_OK;
}

bool ArquivoSd::sincronizar() {
    if (!validoParaArquivo()) {
        return false;
    }
    FRESULT resultado = f_sync(&arquivo);
    registrarResultado(resultado);
    return resultado == FR_OK;
}

bool ArquivoSd::encaminharDados(FuncaoEncaminhamentoFat funcao_encaminhamento, UINT bytes_transferir, UINT &bytes_processados) {
    if (!validoParaArquivo()) {
        return false;
    }
    if (funcao_encaminhamento == nullptr) {
        return false;
    }
    bytes_processados = 0u;
#if FF_USE_FORWARD
    FRESULT resultado = f_forward(&arquivo, funcao_encaminhamento, bytes_transferir, &bytes_processados);
    registrarResultado(resultado);
    return resultado == FR_OK;
#else
    (void)funcao_encaminhamento;
    (void)bytes_transferir;
    registrarResultado(FR_NOT_ENABLED);
    CARTAO_SD_LOG("f_forward desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool ArquivoSd::expandir(FSIZE_t tamanho_desejado, bool preencher_com_zero) {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para expandir\r\n");
        return false;
    }
    BYTE opcao = preencher_com_zero ? 1u : 0u;
#if FF_USE_EXPAND
    FRESULT resultado = f_expand(&arquivo, tamanho_desejado, opcao);
    registrarResultado(resultado);
    return resultado == FR_OK;
#else
    (void)tamanho_desejado;
    (void)opcao;
    registrarResultado(FR_NOT_ENABLED);
    CARTAO_SD_LOG("f_expand desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool ArquivoSd::escreverCaractere(char caractere) {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para escrita\r\n");
        return false;
    }
    if (!abrirParaAcrescentar()) {
        return false;
    }
    int resultado = f_putc(caractere, &arquivo);
    if (resultado < 0) {
        registrarResultado(static_cast<FRESULT>(f_error(&arquivo)));
        return false;
    }
    registrarResultado(FR_OK);
    return true;
}

bool ArquivoSd::escreverLinha(const char* texto) {
    if (!validoParaArquivo()) {
        return false;
    }
    if ((modoAbertura & (MODO_ESCRITA | MODO_ACRESCENTAR)) == 0) {
        CARTAO_SD_LOG("arquivo não aberto para escrita\r\n");
        return false;
    }
    if (!abrirParaAcrescentar()) {
        return false;
    }
    int resultado = f_puts(texto, &arquivo);
    if (resultado < 0) {
        registrarResultado(static_cast<FRESULT>(f_error(&arquivo)));
        return false;
    }
    registrarResultado(FR_OK);
    return true;
}

bool ArquivoSd::lerLinha(char* destino, size_t capacidade) {
    if (!validoParaArquivo()) {
        return false;
    }
    if (destino == nullptr) {
        return false;
    }
    if (capacidade == 0u) {
        return false;
    }
    char* leitura = f_gets(destino, static_cast<int>(capacidade), &arquivo);
    if (leitura == nullptr) {
        registrarResultado(static_cast<FRESULT>(f_error(&arquivo)));
        return false;
    }
    registrarResultado(FR_OK);
    return true;
}

bool ArquivoSd::estaNoFim() const {
    if (!aberto) {
        return true;
    }
    if (ehDiretorio) {
        return false;
    }
    return f_eof(&arquivo) != 0;
}

BYTE ArquivoSd::obterCodigoErro() const {
    if (!aberto) {
        return 0u;
    }
    if (ehDiretorio) {
        return 0u;
    }
    return f_error(const_cast<FIL*>(&arquivo));
}

FRESULT ArquivoSd::resultadoOperacao() const {
    return ultimoResultado;
}

bool ArquivoSd::reiniciarPosicao() {
    if (!validoParaArquivo()) {
        return false;
    }
    FRESULT resultado = f_lseek(&arquivo, 0u);
    registrarResultado(resultado);
    return resultado == FR_OK;
}

bool ArquivoSd::obterInformacoes(InformacoesEntradaFat &destino) const {
    if (!aberto) {
        return false;
    }
    if (ehEntradaEnumerada) {
        converterFilinfoParaInformacoes(infoEntrada, destino);
        return true;
    }
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    FRESULT resultado = f_stat(caminho, &informacao);
    const_cast<ArquivoSd*>(this)->registrarResultado(resultado);
    if (resultado != FR_OK) {
        return false;
    }
    converterFilinfoParaInformacoes(informacao, destino);
    return true;
}

void ArquivoSd::registrarResultado(FRESULT resultado) {
    ultimoResultado = resultado;
}

CartaoSD::CartaoSD(spi_inst_t* instanciaSpi, uint8_t gpioMiso, uint8_t gpioMosi, uint8_t gpioSck, uint8_t gpioCs)
    : controladorSpi(instanciaSpi, gpioMiso, gpioMosi, gpioSck, gpioCs, FREQUENCIA_SPI_BAIXA, FREQUENCIA_SPI_ALTA),
      driverSd(controladorSpi),
      montado(false),
      unidadeLogica("0:"),
      ultimoResultado(FR_OK) {
    memset(&sistemaArquivos, 0, sizeof(sistemaArquivos));
    cartao_sd::registrarDriverFatFs(&driverSd);
}

CartaoSD::~CartaoSD() {
    desmontarSistemaArquivos();
}

bool CartaoSD::garantirInicio() {
    bool iniciou = driverSd.iniciar();
    if (!iniciou) {
        CARTAO_SD_LOG("falha ao iniciar comunicação com cartão\r\n");
        ultimoResultado = FR_NOT_READY;
        return false;
    }
    ultimoResultado = FR_OK;
    return true;
}

bool CartaoSD::iniciarSpi() {
    return garantirInicio();
}

bool CartaoSD::montarSistemaArquivos() {
    if (montado) {
        ultimoResultado = FR_OK;
        return true;
    }

    if (!garantirInicio()) {
        return false;
    }

    FRESULT resultado_montagem = f_mount(&sistemaArquivos, unidadeLogica, 1);
    ultimoResultado = resultado_montagem;
    if (resultado_montagem == FR_OK) {
        montado = true;
        return true;
    }

    CARTAO_SD_LOG("falha ao montar sistema de arquivos: %d\r\n", resultado_montagem);
    return false;
}

bool CartaoSD::desmontarSistemaArquivos() {
    if (!montado) {
        ultimoResultado = FR_OK;
        return true;
    }

    FRESULT resultado_desmontagem = f_unmount(unidadeLogica);
    ultimoResultado = resultado_desmontagem;
    if (resultado_desmontagem == FR_OK) {
        montado = false;
        return true;
    }

    CARTAO_SD_LOG("falha ao desmontar sistema de arquivos: %d\r\n", resultado_desmontagem);
    return false;
}

bool CartaoSD::existeCaminho(const char* caminho_consulta) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FILINFO info;
    memset(&info, 0, sizeof(info));
    FRESULT resultado_stat = f_stat(caminho_consulta, &info);
    ultimoResultado = resultado_stat;
    if (resultado_stat == FR_OK) {
        return true;
    }
    if (resultado_stat == FR_NO_FILE || resultado_stat == FR_NO_PATH) {
        return false;
    }
    CARTAO_SD_LOG("erro ao consultar caminho: %d\r\n", resultado_stat);
    return false;
}

bool CartaoSD::criarDiretorio(const char* caminho_criar) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FRESULT resultado_mkdir = f_mkdir(caminho_criar);
    ultimoResultado = resultado_mkdir;
    if (resultado_mkdir == FR_OK) {
        return true;
    }
    if (resultado_mkdir == FR_EXIST) {
        return true;
    }
    CARTAO_SD_LOG("falha ao criar diretório: %d\r\n", resultado_mkdir);
    return false;
}

bool CartaoSD::removerDiretorio(const char* caminho_remover) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FRESULT resultado_unlink = f_unlink(caminho_remover);
    ultimoResultado = resultado_unlink;
    if (resultado_unlink == FR_OK) {
        return true;
    }
    CARTAO_SD_LOG("falha ao remover diretório: %d\r\n", resultado_unlink);
    return false;
}

bool CartaoSD::removerDiretorioRecursivo(const char* caminho_remover) {
    if (caminho_remover == nullptr || caminho_remover[0] == 0) {
        ultimoResultado = FR_INVALID_NAME;
        return false;
    }

    // Impede apagar a raiz inteira por engano
    if ((strcmp(caminho_remover, "/") == 0) ||
        (strcmp(caminho_remover, "0:") == 0) ||
        (strcmp(caminho_remover, "0:/") == 0)) {
        ultimoResultado = FR_DENIED;
        CARTAO_SD_LOG("remocao recursiva da raiz bloqueada\r\n");
        return false;
    }

    if (!montarSistemaArquivos()) {
        return false;
    }

    ArquivoSd diretorio = abrir(caminho_remover, MODO_DIRETORIO | MODO_LEITURA);
    if (!diretorio.estaAberto()) {
        CARTAO_SD_LOG("falha ao abrir diretório para remoção recursiva: %d\r\n", resultadoOperacao());
        return false;
    }

    for (;;) {
        ArquivoSd entrada = diretorio.abrirProximaEntrada();
        if (!entrada.estaAberto()) {
            break;
        }

        InformacoesEntradaFat info{};
        if (!entrada.obterInformacoes(info)) {
            entrada.fechar();
            continue;
        }

        const char* nome = info.nome_curto;
#if FF_USE_LFN
        if (info.nome_completo[0] != 0) {
            nome = info.nome_completo;
        }
#endif

        if ((strcmp(nome, ".") == 0) || (strcmp(nome, "..") == 0)) {
            entrada.fechar();
            continue;
        }

        char caminho_completo[TAMANHO_CAMINHO_TRABALHO];
        if (!entrada.nome(caminho_completo, sizeof(caminho_completo))) {
            entrada.fechar();
            diretorio.fechar();
            ultimoResultado = FR_INVALID_NAME;
            return false;
        }

        bool sucesso = false;
        if ((info.atributos & AM_DIR) != 0) {
            entrada.fechar();
            sucesso = removerDiretorioRecursivo(caminho_completo);
        } else {
            entrada.fechar();
            sucesso = removerArquivo(caminho_completo);
        }

        if (!sucesso) {
            diretorio.fechar();
            return false;
        }
    }

    diretorio.fechar();
    return removerDiretorio(caminho_remover);
}

bool CartaoSD::removerArquivo(const char* caminho_remover) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FRESULT resultado_unlink = f_unlink(caminho_remover);
    ultimoResultado = resultado_unlink;
    if (resultado_unlink == FR_OK) {
        return true;
    }
    CARTAO_SD_LOG("falha ao remover arquivo: %d\r\n", resultado_unlink);
    return false;
}

ArquivoSd CartaoSD::abrir(const char* caminho_abrir, int modo) {
    ArquivoSd handle;
    if (!montarSistemaArquivos()) {
        return handle;
    }
    if ((modo & MODO_DIRETORIO) != 0) {
        FRESULT resultado = f_opendir(&handle.diretorio, caminho_abrir);
        ultimoResultado = resultado;
        handle.registrarResultado(resultado);
        if (resultado != FR_OK) {
            return handle;
        }
        handle.aberto = true;
        handle.ehDiretorio = true;
        handle.modoAbertura = modo;
        strncpy(handle.caminho, caminho_abrir, sizeof(handle.caminho) - 1u);
        handle.caminho[sizeof(handle.caminho) - 1u] = 0;
        return handle;
    }
    BYTE flags_fatfs = 0;
    if (modo & MODO_LEITURA) flags_fatfs |= FA_READ;
    if (modo & MODO_ESCRITA) flags_fatfs |= FA_WRITE | FA_OPEN_ALWAYS;
    if (modo & MODO_ACRESCENTAR) flags_fatfs |= FA_WRITE | FA_OPEN_ALWAYS;
    FRESULT resultado = f_open(&handle.arquivo, caminho_abrir, flags_fatfs);
    ultimoResultado = resultado;
    handle.registrarResultado(resultado);
    if (resultado != FR_OK) {
        return handle;
    }
    handle.aberto = true;
    handle.ehDiretorio = false;
    handle.modoAbertura = modo;
    strncpy(handle.caminho, caminho_abrir, sizeof(handle.caminho) - 1u);
    handle.caminho[sizeof(handle.caminho) - 1u] = 0;
    if ((modo & MODO_ACRESCENTAR) == 0) {
        return handle;
    }
    bool posicionou_fim = handle.abrirParaAcrescentar();
    ultimoResultado = handle.resultadoOperacao();
    if (posicionou_fim) {
        return handle;
    }
    handle.fechar();
    ArquivoSd handle_invalido;
    return handle_invalido;
}

bool CartaoSD::renomear(const char* caminho_original, const char* caminho_destino) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FRESULT resultado = f_rename(caminho_original, caminho_destino);
    ultimoResultado = resultado;
    if (resultado == FR_OK) {
        return true;
    }
    CARTAO_SD_LOG("falha ao renomear caminho: %d\r\n", resultado);
    return false;
}

bool CartaoSD::obterInformacoes(const char* caminho, InformacoesEntradaFat &destino) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    FRESULT resultado = f_stat(caminho, &informacao);
    ultimoResultado = resultado;
    if (resultado != FR_OK) {
        return false;
    }
    converterFilinfoParaInformacoes(informacao, destino);
    return true;
}

bool CartaoSD::alterarAtributos(const char* caminho, uint8_t atributos, uint8_t mascara) {
    if (!montarSistemaArquivos()) {
        return false;
    }
#if FF_USE_CHMOD
    FRESULT resultado = f_chmod(caminho, atributos, mascara);
    ultimoResultado = resultado;
    return resultado == FR_OK;
#else
    (void)caminho;
    (void)atributos;
    (void)mascara;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_chmod desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::alterarHorario(const char* caminho, const CarimboTempoFat &tempo) {
    if (!montarSistemaArquivos()) {
        return false;
    }
#if FF_USE_CHMOD
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    informacao.fdate = tempo.data;
    informacao.ftime = tempo.hora;
    FRESULT resultado = f_utime(caminho, &informacao);
    ultimoResultado = resultado;
    return resultado == FR_OK;
#else
    (void)caminho;
    (void)tempo;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_utime desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::alterarDiretorioAtual(const char* caminho) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FRESULT resultado = f_chdir(caminho);
    ultimoResultado = resultado;
    return resultado == FR_OK;
}

bool CartaoSD::alterarUnidadeAtual(const char* unidade) {
    FRESULT resultado = f_chdrive(unidade);
    ultimoResultado = resultado;
    return resultado == FR_OK;
}

bool CartaoSD::obterDiretorioAtual(char* destino, size_t capacidade) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    if (destino == nullptr || capacidade == 0u) {
        return false;
    }
    FRESULT resultado = f_getcwd(destino, static_cast<UINT>(capacidade));
    ultimoResultado = resultado;
    return resultado == FR_OK;
}

bool CartaoSD::obterEspacoLivre(const char* caminho, EstatisticaEspacoLivreFat &destino) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    FATFS* referencia = nullptr;
    DWORD clusters_livres = 0u;
    FRESULT resultado = f_getfree(caminho, &clusters_livres, &referencia);
    ultimoResultado = resultado;
    if (resultado != FR_OK || referencia == nullptr) {
        return false;
    }
    destino.clusters_livres = clusters_livres;
    destino.clusters_totais = (referencia->n_fatent > 2u) ? static_cast<uint32_t>(referencia->n_fatent - 2u) : 0u;
    destino.setores_por_cluster = referencia->csize;
#if FF_MAX_SS != FF_MIN_SS
    destino.bytes_por_setor = referencia->ssize;
#else
    destino.bytes_por_setor = FF_MAX_SS;
#endif
    return true;
}

bool CartaoSD::obterRotulo(const char* caminho, char* destino_rotulo, size_t capacidade, uint32_t &numero_serie) {
    if (!montarSistemaArquivos()) {
        return false;
    }
    if (destino_rotulo == nullptr || capacidade == 0u) {
        return false;
    }
#if FF_USE_LABEL
    TCHAR rotulo_local[FF_SFN_BUF + 1u];
    DWORD serie = 0u;
    FRESULT resultado = f_getlabel(caminho, rotulo_local, &serie);
    ultimoResultado = resultado;
    if (resultado != FR_OK) {
        return false;
    }
    size_t capacidade_rotulo = sizeof(rotulo_local) / sizeof(rotulo_local[0]);
    size_t tamanho_rotulo = 0u;
    while (tamanho_rotulo < capacidade_rotulo && rotulo_local[tamanho_rotulo] != 0) {
        tamanho_rotulo++;
    }
    if (tamanho_rotulo + 1u > capacidade) {
        return false;
    }
    for (size_t indice = 0; indice <= tamanho_rotulo && indice < capacidade_rotulo; ++indice) {
        destino_rotulo[indice] = static_cast<char>(rotulo_local[indice]);
    }
    numero_serie = serie;
    return true;
#else
    (void)caminho;
    (void)destino_rotulo;
    (void)capacidade;
    numero_serie = 0u;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_getlabel desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::definirRotulo(const char* rotulo) {
    if (!montarSistemaArquivos()) {
        return false;
    }
#if FF_USE_LABEL
    FRESULT resultado = f_setlabel(rotulo);
    ultimoResultado = resultado;
    return resultado == FR_OK;
#else
    (void)rotulo;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_setlabel desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::formatar(const char* caminho, const ParametrosFormatacaoFat &parametros, void* area_trabalho, size_t tamanho_area) {
    if (montado) {
        if (!desmontarSistemaArquivos()) {
            return false;
        }
    }
    if (!garantirInicio()) {
        return false;
    }
    MKFS_PARM configuracao = converterParametrosFormatacao(parametros);
    if (area_trabalho == nullptr || tamanho_area == 0u) {
        return false;
    }
    FRESULT resultado = f_mkfs(caminho, &configuracao, area_trabalho, static_cast<UINT>(tamanho_area));
    ultimoResultado = resultado;
    return resultado == FR_OK;
}

bool CartaoSD::criarParticoes(uint8_t unidade_fisica, const LBA_t tabela_particoes[], void* area_trabalho) {
    if (montado) {
        if (!desmontarSistemaArquivos()) {
            return false;
        }
    }
    if (!garantirInicio()) {
        return false;
    }
    if (tabela_particoes == nullptr) {
        return false;
    }
#if FF_MULTI_PARTITION
    FRESULT resultado = f_fdisk(unidade_fisica, tabela_particoes, area_trabalho);
    ultimoResultado = resultado;
    return resultado == FR_OK;
#else
    (void)unidade_fisica;
    (void)tabela_particoes;
    (void)area_trabalho;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_fdisk desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::definirPaginaCodigo(uint16_t codigo_pagina) {
#if FF_CODE_PAGE == 0
    FRESULT resultado = f_setcp(codigo_pagina);
    ultimoResultado = resultado;
    return resultado == FR_OK;
#else
    (void)codigo_pagina;
    ultimoResultado = FR_NOT_ENABLED;
    CARTAO_SD_LOG("f_setcp desabilitado na configuracao do FatFs\r\n");
    return false;
#endif
}

bool CartaoSD::buscarPrimeiro(const char* caminho, const char* padrao, InformacoesEntradaFat &destino, ContextoBuscaFat &contexto) {
    if (!montarSistemaArquivos()) {
        contexto.ativo = false;
        return false;
    }
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    FRESULT resultado = f_findfirst(&contexto.diretorio, &informacao, caminho, padrao);
    ultimoResultado = resultado;
    if (resultado != FR_OK) {
        contexto.ativo = false;
        return false;
    }
    if (informacao.fname[0] == 0) {
        contexto.ativo = false;
        return false;
    }
    converterFilinfoParaInformacoes(informacao, destino);
    contexto.ativo = true;
    return true;
}

bool CartaoSD::buscarProximo(InformacoesEntradaFat &destino, ContextoBuscaFat &contexto) {
    if (!contexto.ativo) {
        return false;
    }
    FILINFO informacao;
    memset(&informacao, 0, sizeof(informacao));
    FRESULT resultado = f_findnext(&contexto.diretorio, &informacao);
    ultimoResultado = resultado;
    if (resultado != FR_OK) {
        return false;
    }
    if (informacao.fname[0] == 0) {
        contexto.ativo = false;
        return false;
    }
    converterFilinfoParaInformacoes(informacao, destino);
    return true;
}

bool CartaoSD::finalizarBusca(ContextoBuscaFat &contexto) {
    if (!contexto.ativo) {
        ultimoResultado = FR_OK;
        return true;
    }
    FRESULT resultado = f_closedir(&contexto.diretorio);
    contexto.ativo = false;
    ultimoResultado = resultado;
    return resultado == FR_OK;
}

FRESULT CartaoSD::resultadoOperacao() const {
    return ultimoResultado;
}
