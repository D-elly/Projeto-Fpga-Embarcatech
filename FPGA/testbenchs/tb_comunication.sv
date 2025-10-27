`timescale 1ns / 1ps

// Módulo de Testbench
// Não tem entradas nem saídas
module tb_comunication;

    // --- 1. Constantes para os Clocks ---
    // Clock principal de 25MHz (Período = 1 / 25MHz = 40 ns)
    localparam int CLK_PERIOD = 40; 
    
    // Clock do SPI (SCLK) de 2MHz (Período = 500 ns)
    // (1 / (16 * 20kHz)) = 3.125us (mínimo)
    // (1 / 2MHz) = 500ns (muito mais rápido, ótimo)
    localparam int SCLK_PERIOD = 500; 

    // --- 2. Sinais do Testbench ---
    // Entradas para o DUT
    logic        tb_clk;
    logic        sclk_in;
    logic        mosi_in;
    logic        active;
    logic        reset;


    // Saídas do DUT
    wire [15:0] audio_out;
    wire        data_ready;


    // --- 3. Instanciar o Módulo (DUT) ---
    // Conecta os sinais do testbench às portas do módulo
    comunication dut (
        .clk_25mhz  (tb_clk),
        
        .sclk_in   (sclk_in),
        .mosi_in   (mosi_in),
        .active    (active),
        .reset     (reset),
        .audio_out (audio_out),
        .data_ready(data_ready)
    );

    // --- 4. Gerador de Clock Principal (25MHz) ---
    initial begin
        tb_clk = 0;
        // 'forever' cria um loop infinito que gera o clock
        forever #(CLK_PERIOD / 2) tb_clk = ~tb_clk;
    end

    // --- 5. Tarefa (Task) para simular o Pico enviando 16 bits ---
    // Uma 'task' é como uma função para simulação
    task send_spi_word(input [15:0] data_word);
        // Espera o próximo ciclo de clock para sincronizar
        @(posedge tb_clk);
        
        // Ativa o "Chip Select" (ativo-alto, como no seu código)
        active <= 1'b1; 
        sclk_in <= 1'b0;
        
        // Espera alguns ciclos para o DUT ver o 'active'
        #(CLK_PERIOD * 2); 

        // Loop para enviar os 16 bits, do mais significativo (MSB) ao menos (LSB)
        for (int i = 15; i >= 0; i--) begin
            
            // Coloca o bit de dado na linha
            mosi_in <= data_word[i];
            
            // Espera meio período do SCLK
            #(SCLK_PERIOD / 2);
            
            // Sobe o SCLK (o DUT captura o dado aqui)
            sclk_in <= 1'b1;
            
            // Espera meio período do SCLK
            #(SCLK_PERIOD / 2);
            
            // Desce o SCLK
            sclk_in <= 1'b0;
        end
        
        // Espera um pouco antes de desativar
        #(CLK_PERIOD * 2);
        active <= 1'b0; // Desativa o "Chip Select"
    endtask


    // --- 6. Sequência de Teste Principal ---
    initial begin
        // Abre o arquivo de formas de onda (opcional, mas recomendado)
        $dumpfile("dump.vcd");
        $dumpvars(0, tb_comunication);

        // Inicia a simulação com o reset ativado
        $display("Iniciando simulação... Reset ativado.");
        reset     <= 1'b1;
        active    <= 1'b0;
        sclk_in   <= 1'b0;
        mosi_in   <= 1'b0;
        #100ns; // Espera 100 ns

        // Libera o reset
        reset <= 1'b0;
        $display("Reset liberado. Módulo em IDLE.");
        @(posedge tb_clk);
        
        $display("TESTE 1: Enviando 16'hA5A5...");
    
    // 'fork' inicia um bloco paralelo
    fork
        // 'join_none' diz: não espere esta tarefa terminar,
        // continue executando o código principal.
        send_spi_word(16'hA5A5);
    join_none

    // AGORA, o 'send_spi_word' está rodando em paralelo,
    // e esta linha @(posedge) está escutando AO MESMO TEMPO.
    @(posedge data_ready);
    $display("... Pulso 'data_ready' recebido!");
    $display("... Valor em 'audio_out' = %h", audio_out);

    // O resto da sua lógica de assert estava correta
    assert (audio_out == 16'hA5A5)
        else $error("FALHA TESTE 1: Valor esperado era A5A5, mas foi %h", audio_out);

    @(posedge tb_clk);
    assert (data_ready == 1'b0)
        else $error("FALHA TESTE 1: 'data_ready' não voltou para 0!");

    $display("TESTE 1: Concluído com sucesso!");
    
    #(SCLK_PERIOD * 4);

    // --- TESTE 2: CORRIGIDO COM FORK...JOIN_NONE ---
    $display("TESTE 2: Enviando 16'hBEEF...");
    
    fork
        send_spi_word(16'hBEEF);
    join_none
    
    @(posedge data_ready);
    $display("... Pulso 'data_ready' recebido!");
    $display("... Valor em 'audio_out' = %h", audio_out);
    
    assert (audio_out == 16'hBEEF)
        else $error("FALHA TESTE 2: Valor esperado era BEEF, mas foi %h", audio_out);
    
    $display("TESTE 2: Concluído com sucesso!");
    
    #1000ns;
    $display("Simulação concluída.");
    $finish;
    end

endmodule