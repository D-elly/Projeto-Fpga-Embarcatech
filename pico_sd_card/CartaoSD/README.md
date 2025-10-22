# Biblioteca CartaoSD para Raspberry Pi Pico W

## Visão geral

A biblioteca **CartaoSD** encapsula o acesso a cartões SD via SPI no Raspberry Pi Pico W (RP2040), oferecendo uma camada orientada a objetos sobre o FatFs. Ela abstrai a troca de comandos SPI, o fluxo de inicialização do cartão e expõe uma interface de arquivos (`ArquivoSd`) que simplifica leitura, escrita, gerenciamento de diretórios e operações avançadas como encaminhamento de dados e formatação.

## Recursos principais

- Inicialização dupla de SPI com frequência segura de boot (400 kHz) e operação rápida (12,5 MHz) após a enumeração do cartão.
- Montagem, desmontagem e formatação de sistemas de arquivos FAT usando FatFs 0.15.
- Manipulação de arquivos e diretórios com a classe `ArquivoSd`, incluindo escrita formatada, leitura incremental, truncamento, expansão e encaminhamento (`f_forward`).
- Utilitários para gerenciamento de volume: rótulo, espaço livre, carimbo de data/hora e iteração de diretórios com contexto preservado.
- Driver em camadas (`ControladorSpiCartao` + `DriverCartaoSd`) que isola o hardware SPI das chamadas FatFs, mantendo SOLID e facilitando testes.
- Registro de logs opcional via UART com a macro `HABILITAR_LOG_CARTAO_SD`.

## Requisitos

- Hardware: Raspberry Pi Pico W (RP2040) com cartão SD conectado ao barramento SPI0.
- Software: Pico SDK 2.2.0 configurado, CMake 3.13+ e compilador arm-none-eabi.
- Dependências: `pico_stdlib`, `hardware_spi` e as fontes do FatFs incluídas na pasta `CartaoSD/src/ff15`.

## Ligações de pinos

### SPI do cartão SD

| Sinal | GPIO padrão | Observações |
| --- | --- | --- |
| SPI0 MISO | GP16 | Entrada de dados do cartão. |
| SPI0 MOSI | GP19 | Saída de dados para o cartão. |
| SPI0 SCK | GP18 | Clock SPI. |
| Chip Select | GP17 | Seleção do cartão (controlada pelo `ControladorSpiCartao`). |

> Ajuste os GPIOs conforme a fiação da sua placa, mantendo-os coerentes com o construtor `CartaoSD`.

## Integração no CMake

1. Garanta que a pasta `CartaoSD/` esteja ao lado do seu `CMakeLists.txt` principal.
2. No projeto, inclua o diretório e vincule a biblioteca estática:

```cmake
add_subdirectory(CartaoSD)

add_executable(seu_projeto
    main.cpp
    # outros arquivos
)

target_link_libraries(seu_projeto
    PRIVATE        
        pico_stdlib
        cartao_sd
)
```

3. Ative a UART como backend do `stdio` antes de usar `printf`:

```cmake
pico_enable_stdio_uart(main 1)

#ou

pico_enable_stdio_usb(main 1)

#nunca os dois ao mesmo tempo
```

## Fluxo básico de uso

1. Crie uma instância de `CartaoSD`, informando a interface SPI e os GPIOs conectados ao cartão.
2. Chame `iniciarSpi()` logo após configurar o `pico_stdlib`.
3. Monte o sistema de arquivos com `montarSistemaArquivos()`.
4. Use `abrir()` para manipular arquivos/diretórios através de `ArquivoSd`.
5. Feche arquivos (`fechar()`) e, ao final, libere a mídia com `desmontarSistemaArquivos()`.

## Exemplo: registrar logs em um arquivo

```cpp
#include "pico/stdlib.h"
#include "CartaoSD.h"

#define SPI_CARTAO spi0;
#define PINO_SPI_MISO_CARTAO 16u;
#define PINO_SPI_MOSI_CARTAO 19u;
#define PINO_SPI_SCK_CARTAO 18u;
#define PINO_SPI_CS_CARTAO 17u;

int main()
{
    stdio_init_all();

    CartaoSD cartao(SPI_CARTAO,
                    PINO_SPI_MISO_CARTAO,
                    PINO_SPI_MOSI_CARTAO,
                    PINO_SPI_SCK_CARTAO,
                    PINO_SPI_CS_CARTAO);

    if (!cartao.iniciarSpi()) {
        printf("Falha ao iniciar SPI do cartão\r\n");
        return 1;
    }

    if (!cartao.montarSistemaArquivos()) {
        printf("Falha ao montar FAT\r\n");
        return 1;
    }

    ArquivoSd arquivo = cartao.abrir("/logs.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
    if (arquivo.estaAberto()) {
        arquivo.escreverLinha("Inicializacao concluida");
        arquivo.sincronizar();
        arquivo.fechar();
    }

    cartao.desmontarSistemaArquivos();

    while (true) {
        tight_loop_contents();
    }

    return 0;
}
```

## Exemplo: listar entradas (pastas ou arquivos) de um diretório

```cpp
bool listarDiretorio(CartaoSD &cartao)
{
    ArquivoSd diretorio = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
    if (!diretorio.estaAberto()) {
        return false;
    }

    while (true) {
        ArquivoSd entrada = diretorio.abrirProximaEntrada();
        if (!entrada.estaAberto()) {
            break;
        }

        InformacoesEntradaFat info;
        if (entrada.obterInformacoes(info)) {
            // cada entrada representa um arquivo ou subdiretorio do caminho atual
            printf("%s\r\n", info.nome_curto);
        }
    }

    diretorio.fechar();
    return true;
}
```

## Configurações úteis

- **Logs em tempo de execução:** defina `HABILITAR_LOG_CARTAO_SD` antes de incluir `CartaoSD.h` para redirecionar mensagens de diagnóstico ao `printf`.
- **Carimbo de tempo FAT:** implemente `DWORD obterCarimboTempoFat()` em `FatFsTempo.cpp` conforme o RTC disponível para que o FatFs atribua data/hora correta aos arquivos.
- **Formatação:** utilize `formatar()` com um buffer de trabalho alinhado (consulte a documentação do FatFs para dimensionar `area_trabalho`).

## Constantes e tipos expostos

### `MODO_LEITURA`
Permite abrir arquivos ou diretórios apenas para leitura, evitando modificações na mídia.

```cpp
// abre arquivo apenas para leitura
ArquivoSd arquivo = cartao.abrir("/dados.txt", MODO_LEITURA);
```

### `MODO_ESCRITA`
Cria um arquivo novo ou sobrescreve o existente, posicionando o cursor no início.

```cpp
// grava relatório sobrescrevendo conteúdo antigo
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_ESCRITA);
```

### `MODO_ACRESCENTAR`
Mantém o conteúdo existente e posiciona o cursor no fim para continuar escrevendo.

```cpp
// adiciona linha ao histórico sem apagar dados anteriores
ArquivoSd historico = cartao.abrir("/historico.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
```

### `MODO_DIRETORIO`
Abre um diretório para navegação, possibilitando enumerar arquivos internos.

```cpp
// percorre diretório raiz para localizar arquivos de configuração
ArquivoSd raiz = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
```

### `CarimboTempoFat`
Armazena data e hora no formato próprio do FatFs para atualização de carimbo temporal.

```cpp
// define carimbo 2025-01-10 15:30:00 para um arquivo
CarimboTempoFat tempo{ .data = 0x7E4A, .hora = 0x7BC0 };
cartao.alterarHorario("/dados.txt", tempo);
```

### `InformacoesEntradaFat`
Recebe metadados de um arquivo ou diretório consultado no volume.

```cpp
// captura atributos do arquivo e imprime nome curto
InformacoesEntradaFat info{};
if (cartao.obterInformacoes("/dados.txt", info)) {
    printf("Arquivo: %s\r\n", info.nome_curto);
}
```

### `EstatisticaEspacoLivreFat`
Contém a quantidade de clusters livres e totais, além da geometria do volume.

```cpp
// calcula espaço livre aproximado em bytes
EstatisticaEspacoLivreFat espaco{};
if (cartao.obterEspacoLivre("/", espaco)) {
    uint64_t bytes_livres = static_cast<uint64_t>(espaco.clusters_livres) *
                            espaco.setores_por_cluster *
                            espaco.bytes_por_setor;
    printf("Livre: %llu bytes\r\n", bytes_livres);
}
```

### `ParametrosFormatacaoFat`
Define a estratégia de formatação utilizada em `formatar()`.

```cpp
// prepara parâmetros para criar FAT32 com cluster de 32 KB
ParametrosFormatacaoFat parametros{};
parametros.formato = FM_FAT32;
parametros.quantidade_fats = 1u;
parametros.tamanho_cluster_bytes = 32u * 1024u;
```

### `ContextoBuscaFat`
Mantém estado entre chamadas de busca com curingas (`buscarPrimeiro`, `buscarProximo`).

```cpp
// encontra todos os arquivos .txt no diretório raiz
ContextoBuscaFat contexto{};
InformacoesEntradaFat item{};
if (cartao.buscarPrimeiro("/", "*.txt", item, contexto)) {
    do {
        printf("Encontrado: %s\r\n", item.nome_curto);
    } while (cartao.buscarProximo(item, contexto));
    cartao.finalizarBusca(contexto);
}
```

### `FuncaoEncaminhamentoFat`
Aliás para funções capazes de receber dados diretamente do FatFs via `encaminharDados()`.

```cpp
// encaminha blocos lidos para a UART sem copiar para buffers intermediários
UINT imprimirBytes(const BYTE* dados, UINT tamanho)
{
    printf("Transferindo %u bytes\r\n", tamanho);
    return tamanho; // informa ao FatFs que todos os bytes foram consumidos
}

ArquivoSd arquivo = cartao.abrir("/dados.bin", MODO_LEITURA);
UINT processados = 0u;
arquivo.encaminharDados(&imprimirBytes, 64u, processados);
```

## API pública detalhada

Nos exemplos a seguir considere que `CartaoSD cartao` já foi criado, `iniciarSpi()` executado e o sistema de arquivos montado com sucesso.

### Classe `CartaoSD`

#### `CartaoSD(spi_inst_t* instanciaSpi, uint8_t gpioMiso, uint8_t gpioMosi, uint8_t gpioSck, uint8_t gpioCs)`
Configura a pilha SD com os pinos utilizados.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u); // configura SPI padrão do projeto
```

#### `~CartaoSD()`
Garante desmontagem ao destruir o objeto.

```cpp
{
    CartaoSD cartao_temporario(spi0, 16u, 19u, 18u, 17u);
} // destrutor desmonta o volume automaticamente
```

#### `bool iniciarSpi()`
Prepara GPIOs e velocidade inicial do barramento e monta o volume FAT na unidade padrão.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.iniciarSpi();
cartao_local.montarSistemaArquivos();
```

#### `bool desmontarSistemaArquivos()`
Libera o volume antes de remover o cartão.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.iniciarSpi();
cartao_local.montarSistemaArquivos();
cartao_local.desmontarSistemaArquivos();
```

#### `bool existeCaminho(const char* caminho)`
Verifica rapidamente se um arquivo ou diretório está presente.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
if (!cartao_local.existeCaminho("/config.json")) {
    printf("Config ausente\r\n");
}
```

#### `bool criarDiretorio(const char* caminho)`
Cria diretório simples no volume.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.criarDiretorio("/logs");
```

#### `bool removerDiretorio(const char* caminho)`
Remove diretório vazio.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.removerDiretorio("/logs");
```

#### `bool removerDiretorioRecursivo(const char* caminho)`
Apaga diretório e todo o conteúdo interno.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.removerDiretorioRecursivo("/cache");
```

#### `bool removerArquivo(const char* caminho)`
Exclui um arquivo simples.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.removerArquivo("/dados_antigos.bin");
```

#### `ArquivoSd abrir(const char* caminho, int modo)`
Abre arquivos ou diretórios usando as flags `MODO_*`.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
ArquivoSd configuracao = cartao_local.abrir("/config.txt", MODO_LEITURA);
```

#### `bool renomear(const char* caminho_original, const char* caminho_destino)`
Renomeia arquivos ou move para outro diretório dentro do mesmo volume.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.renomear("/config.txt", "/config_antiga.txt");
```

#### `bool obterInformacoes(const char* caminho, InformacoesEntradaFat &destino)`
Consulta metadados de uma entrada conhecida.

```cpp
InformacoesEntradaFat dados_config{};
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.obterInformacoes("/config.txt", dados_config);
```

#### `bool alterarAtributos(const char* caminho, uint8_t atributos, uint8_t mascara)`
Atualiza atributos FAT como somente leitura.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.alterarAtributos("/config.txt", AM_RDO, AM_RDO);
```

#### `bool alterarHorario(const char* caminho, const CarimboTempoFat &tempo)`
Escreve um novo carimbo de data e hora no arquivo.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
CarimboTempoFat tempo{ .data = 0x7E4A, .hora = 0x7BC0 };
cartao_local.alterarHorario("/config.txt", tempo);
```

#### `bool alterarDiretorioAtual(const char* caminho)`
Define o diretório de trabalho corrente.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.alterarDiretorioAtual("/logs");
```

#### `bool obterDiretorioAtual(char* destino, size_t capacidade)`
Retorna o caminho do diretório corrente.

```cpp
char atual[32];
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.obterDiretorioAtual(atual, sizeof(atual));
```

#### `bool obterEspacoLivre(const char* caminho, EstatisticaEspacoLivreFat &destino)`
Consulta estatísticas de espaço do volume.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
EstatisticaEspacoLivreFat espaco{};
cartao_local.obterEspacoLivre("/", espaco);
```

#### `bool obterRotulo(const char* caminho, char* destino_rotulo, size_t capacidade, uint32_t &numero_serie)`
Obtém o rótulo e número de série do volume.

```cpp
char rotulo[12] = {0};
uint32_t numero = 0;
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.obterRotulo("/", rotulo, sizeof(rotulo), numero);
```

#### `bool definirRotulo(const char* rotulo)`
Define novo rótulo para a partição.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.definirRotulo("DADOS_SD");
```

#### `bool formatar(const char* caminho, const ParametrosFormatacaoFat &parametros, void* area_trabalho, size_t tamanho_area)`
Executa formatação completa do volume.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
ParametrosFormatacaoFat parametros{};
parametros.formato = FM_FAT32;
parametros.quantidade_fats = 1u;
parametros.tamanho_cluster_bytes = 32u * 1024u;
BYTE area_trabalho[FF_MAX_SS * 4] = {0};
cartao_local.formatar("0:", parametros, area_trabalho, sizeof(area_trabalho));
```

#### `bool criarParticoes(uint8_t unidade_fisica, const LBA_t tabela_particoes[], void* area_trabalho)`
Cria partições seguindo tabela fornecida.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
LBA_t particoes[] = { 10000u, 0u };
BYTE area_trabalho[FF_MAX_SS * 4] = {0};
cartao_local.criarParticoes(0u, particoes, area_trabalho);
```

#### `bool definirPaginaCodigo(uint16_t codigo_pagina)`
Seleciona página de código compatível.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
cartao_local.definirPaginaCodigo(850u);
```

#### `bool buscarPrimeiro(const char* caminho, const char* padrao, InformacoesEntradaFat &destino, ContextoBuscaFat &contexto)`
Inicia busca por arquivos que atendam a um padrão.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
ContextoBuscaFat busca{};
InformacoesEntradaFat primeiro{};
cartao_local.buscarPrimeiro("/", "*.csv", primeiro, busca);
```

#### `bool buscarProximo(InformacoesEntradaFat &destino, ContextoBuscaFat &contexto)`
Continua a busca iniciada em `buscarPrimeiro`.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
ContextoBuscaFat busca{};
InformacoesEntradaFat item{};
if (cartao_local.buscarPrimeiro("/", "*.csv", item, busca)) {
    while (cartao_local.buscarProximo(item, busca)) {
        printf("Outro arquivo CSV encontrado\r\n");
    }
}
```

#### `bool finalizarBusca(ContextoBuscaFat &contexto)`
Finaliza busca liberando o contexto mantido.

```cpp
CartaoSD cartao_local(spi0, 16u, 19u, 18u, 17u);
ContextoBuscaFat busca{};
InformacoesEntradaFat item{};
if (cartao_local.buscarPrimeiro("/", "*.csv", item, busca)) {
    cartao_local.finalizarBusca(busca);
}
```

### Classe `ArquivoSd`

#### `ArquivoSd()`
Cria um handle inicialmente inválido que pode receber um arquivo aberto posteriormente.

```cpp
ArquivoSd arquivo; // handle vazio aguardando abertura
arquivo = cartao.abrir("/dados.txt", MODO_LEITURA);
```

#### `bool fechar()`
Encerra o acesso ao recurso FatFs associado ao handle.

```cpp
ArquivoSd arquivo = cartao.abrir("/dados.txt", MODO_LEITURA);
arquivo.fechar(); // libera o arquivo antes de sair da função
```

#### `bool escreverTexto(const char* texto)`
Acrescenta uma string respeitando as configurações de modo de abertura.

```cpp
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
relatorio.escreverTexto("Sistema inicializado\r\n");
```

#### `size_t escreverBytes(const uint8_t* dados, size_t tamanho)`
Grava blocos binários sem conversão.

```cpp
uint8_t amostras[32] = {0};
ArquivoSd binario = cartao.abrir("/amostras.bin", MODO_ESCRITA);
binario.escreverBytes(amostras, sizeof(amostras));
```

#### `size_t lerBytes(uint8_t* buffer, size_t tamanho)`
Lê bytes sequenciais do arquivo.

```cpp
uint8_t buffer[16];
ArquivoSd origem = cartao.abrir("/config.bin", MODO_LEITURA);
origem.lerBytes(buffer, sizeof(buffer));
```

#### `int lerCaractere()`
Obtém um caractere de cada vez.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
int caractere = texto.lerCaractere(); // retorna -1 no fim do arquivo
```

#### `long disponivel()`
Retorna quantos bytes restam até o fim.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
long restantes = texto.disponivel();
```

#### `int espiar()`
Lê o próximo byte sem avançar o cursor.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
int proximo = texto.espiar(); // ideal para verificar separadores
```

#### `bool buscar(long posicao)`
Reposiciona o ponteiro interno do arquivo.

```cpp
ArquivoSd log = cartao.abrir("/log.txt", MODO_LEITURA);
log.buscar(0); // retorna ao início do arquivo
```

#### `long posicao()`
Informa a posição atual do cursor.

```cpp
ArquivoSd log = cartao.abrir("/log.txt", MODO_LEITURA);
long indice = log.posicao();
```

#### `long tamanho()`
Informa o tamanho total do arquivo.

```cpp
ArquivoSd log = cartao.abrir("/log.txt", MODO_LEITURA);
long total = log.tamanho();
```

#### `bool nome(char* destino, size_t capacidade)`
Copia o caminho completo associado ao handle.

```cpp
char caminho[64];
ArquivoSd log = cartao.abrir("/log.txt", MODO_LEITURA);
log.nome(caminho, sizeof(caminho));
```

#### `bool eDiretorio()`
Indica se o handle representa um diretório.

```cpp
ArquivoSd log = cartao.abrir("/log.txt", MODO_LEITURA);
bool eh_diretorio = log.eDiretorio();
```

#### `ArquivoSd abrirProximaEntrada()`
Itera sobre o conteúdo de um diretório previamente aberto.

```cpp
ArquivoSd raiz = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
ArquivoSd item = raiz.abrirProximaEntrada();
```

#### `bool reiniciarDiretorio()`
Reposiciona o iterador de diretório para o início.

```cpp
ArquivoSd raiz = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
raiz.reiniciarDiretorio(); // reinicia listagem após modificar conteúdo
```

#### `bool estaAberto()`
Confere se o handle possui um recurso associado.

```cpp
ArquivoSd raiz = cartao.abrir("/", MODO_DIRETORIO | MODO_LEITURA);
ArquivoSd item = raiz.abrirProximaEntrada();
if (!item.estaAberto()) {
    printf("Fim do diretório\r\n");
}
```

#### `bool truncar()`
Reduz o arquivo ao tamanho atual do cursor.

```cpp
ArquivoSd ajustes = cartao.abrir("/ajustes.txt", MODO_ESCRITA);
ajustes.truncar(); // remove conteúdo residual
```

#### `bool sincronizar()`
Força a gravação dos dados pendentes no cartão.

```cpp
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
relatorio.sincronizar(); // garante escrita antes de reiniciar o sistema
```

#### `bool encaminharDados(FuncaoEncaminhamentoFat funcao, UINT bytes, UINT &bytes_processados)`
Entrega dados diretamente a uma função de processamento.

```cpp
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_LEITURA);
UINT total_processado = 0u;
relatorio.encaminharDados(&imprimirBytes, 128u, total_processado);
```

#### `bool expandir(FSIZE_t tamanho_desejado, bool preencher_com_zero)`
Reserva espaço contínuo antes de gravar dados.

```cpp
ArquivoSd captura = cartao.abrir("/captura.bin", MODO_ESCRITA);
captura.expandir(1024u, true); // garante 1 KB pré-alocado
```

#### `bool escreverCaractere(char caractere)`
Grava um único caractere de cada vez.

```cpp
ArquivoSd captura = cartao.abrir("/captura.bin", MODO_ESCRITA | MODO_ACRESCENTAR);
captura.escreverCaractere('A');
```

#### `bool escreverLinha(const char* texto)`
Inclui a linha seguida de CRLF automaticamente.

```cpp
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
relatorio.escreverLinha("Evento concluido");
```

#### `template<typename... Argumentos> int escreverFormatado(const char* formato, Argumentos... argumentos)`
Permite escrita formatada semelhante a `printf`.

```cpp
ArquivoSd relatorio = cartao.abrir("/log.txt", MODO_ESCRITA | MODO_ACRESCENTAR);
relatorio.escreverFormatado("Valor medio: %d\r\n", 42);
```

#### `bool lerLinha(char* destino, size_t capacidade)`
Lê até encontrar uma quebra de linha ou EOF.

```cpp
char linha[32];
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
texto.lerLinha(linha, sizeof(linha));
```

#### `bool estaNoFim() const`
Indica se o arquivo chegou ao fim da leitura.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
if (texto.estaNoFim()) {
    printf("Fim do arquivo alcançado\r\n");
}
```

#### `BYTE obterCodigoErro() const`
Retorna o código bruto do último erro ocorrido.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
BYTE codigo = texto.obterCodigoErro();
```

#### `FRESULT resultadoOperacao() const`
Informa o último resultado traduzido do FatFs.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
FRESULT resultado = texto.resultadoOperacao();
```

#### `bool reiniciarPosicao()`
Volta o cursor para o início do arquivo sem reabrir o handle.

```cpp
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
texto.reiniciarPosicao();
```

#### `bool obterInformacoes(InformacoesEntradaFat &destino) const`
Entrega os metadados da entrada associada ao handle.

```cpp
InformacoesEntradaFat detalhes{};
ArquivoSd texto = cartao.abrir("/mensagem.txt", MODO_LEITURA);
texto.obterInformacoes(detalhes);
```
## Boas práticas

- Prefira buffers estáticos e reutilizáveis para operações de leitura/escrita, evitando alocação dinâmica.
- Sempre chame `sincronizar()` após operações críticas para garantir que os dados sejam gravados na mídia.
- Use `resultadoOperacao()` para depurar erros retornados pelo FatFs e trate os códigos `FRESULT` adequadamente.

## Referências

- Projeto base do driver SPI + FatFs: [no-OS-FatFS-SD-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SPI-RPi-Pico/tree/master).
- Documentação oficial do FatFs: [elm-chan.org](http://elm-chan.org/fsw/ff/00index_e.html).
