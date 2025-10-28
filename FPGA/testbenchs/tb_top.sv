`timescale 1ns / 1ps

module tb_top;

    // --- 1. Constantes ---
    localparam int CLK_PERIOD           = 40;  // 40 ns = 25MHz
    localparam int SPI_PICO_SCLK_PERIOD = 500; // 500 ns = 2MHz (Clock do SPI vindo do Pico)

    // --- 2. Sinais do Testbench ---
    // Sinais para conectar às ENTRADAS do DUT (pedal_top)
    logic        tb_clk_25mhz;
    logic        tb_reset;
    logic        tb_spi_pico_sclk;
    logic        tb_spi_pico_mosi;
    logic        tb_spi_pico_cs;
    logic        tb_bypass_switch;
    logic        tb_spi_dac_miso; // Entrada MISO do DAC (podemos deixar flutuando ou em 0)

    // Sinais para conectar às SAÍDAS do DUT (pedal_top)
    wire         w_spi_dac_sclk;
    wire        w_spi_dac_mosi;
    wire         w_spi_dac_cs;

    // --- 3. Instanciar o DUT (Device Under Test) ---
    // Conecta os sinais 'tb_' às entradas e 'w_' às saídas do pedal_top
    top dut (
        .clk_25mhz      (tb_clk_25mhz),
        .reset          (tb_reset),
        
        //conexões fpga->dac modulo comunication
        .com_sclk_in  (tb_spi_pico_sclk),
        .com_mosi_in  (tb_spi_pico_mosi),
        .com_active    (tb_spi_pico_cs),
        
        //.bypass_switch  (tb_bypass_switch),
        
        //conexões modulo dac_driver
        .spi_sclk_out   (w_spi_dac_sclk),
        .spi_mosi_out  (w_spi_dac_mosi),
        .spi_active_out     (w_spi_dac_cs)

    );

    // --- 4. Gerador de Clock Principal ---
    initial begin
        tb_clk_25mhz = 1'b0;
        forever #(CLK_PERIOD / 2) tb_clk_25mhz = ~tb_clk_25mhz;
    end

    // --- 5. Tarefa para Simular o Pico enviando SPI ---
    // (A mesma tarefa do testbench anterior, adaptada para os nomes dos sinais)
    task send_spi_word(input [15:0] data_word);
        @(posedge tb_clk_25mhz); // Sincroniza com o clock principal
        
        tb_spi_pico_cs <= 1'b1; // Ativa o CS (assumindo ativo-alto)
        tb_spi_pico_sclk <= 1'b0;
        
        #(CLK_PERIOD * 2); 

        for (int i = 15; i >= 0; i--) begin
            tb_spi_pico_mosi <= data_word[i];       // Coloca o bit
            #(SPI_PICO_SCLK_PERIOD / 2);
            tb_spi_pico_sclk <= 1'b1;               // Sobe o clock SPI
            #(SPI_PICO_SCLK_PERIOD / 2);
            tb_spi_pico_sclk <= 1'b0;               // Desce o clock SPI
        end
        
        #(CLK_PERIOD * 2);
        tb_spi_pico_cs <= 1'b0; // Desativa o CS
    endtask

    // --- 6. Sequência de Teste Principal ---
    initial begin
        $dumpfile("dump_top.vcd"); // Nome diferente para não sobrescrever o anterior
        $dumpvars(0, tb_top); // Monitora todos os sinais deste testbench e do DUT

        $display("Iniciando simulação do pedal_top... Reset ativado.");
        // Inicializa todas as entradas
        tb_reset          <= 1'b1; // Ativa o reset
        tb_spi_pico_sclk  <= 1'b0;
        tb_spi_pico_mosi  <= 1'b0;
        tb_spi_pico_cs    <= 1'b0; // CS inativo
        tb_bypass_switch  <= 1'b1; // **IMPORTANTE: Coloca no modo BYPASS**
        tb_spi_dac_miso   <= 1'b0; // Deixa a entrada MISO flutuando (alta impedância)
        
        #100ns; // Espera 100 ns

        tb_reset <= 1'b0; // Libera o reset
        $display("Reset liberado. Módulo top em IDLE.");
        
        @(posedge tb_clk_25mhz);
        
        // --- TESTE PASSTHROUGH ---
        $display("TESTE PASSTHROUGH: Enviando 16'hC0DE...");
        
        // Inicia o envio SPI em paralelo
        fork
            send_spi_word(16'hC0DE);
        join_none

        // Espera o sinal INTERNO 'data_is_ready' do DUT pulsar.
        // Precisamos usar o caminho hierárquico para acessá-lo.
        @(posedge dut.data_is_ready); 
        $display("... Pulso 'data_is_ready' (interno do DUT) detectado!(%h)", dut.data_is_ready);
        $display("... Conteudo em mosi_out!(%h)", dut.spi_mosi_out);

        // Espera mais um ciclo para o MUX atualizar sua saída
        @(posedge tb_clk_25mhz);

        // Verifica o sinal INTERNO 'audio_selected_16b' do DUT.
        // Este é o sinal que DEVE ir para o dac_driver.
        $display("... Verificando sinal na entrada do dac_driver ('dut.output_audio')...");
        assert (dut.output_audio == 16'hC0DE)
            else $error("FALHA PASSTHROUGH: Valor esperado na entrada do DAC Driver era C0DE, mas foi %h", dut.output_audio);
        
        $display("... Valor correto (%h) chegou à entrada do dac_driver.", dut.output_audio);
        $display("Valor de saída truncado em dac_driver (%h)", dut.spi_mosi_out);
        $display("TESTE PASSTHROUGH: Concluído com sucesso!");
        
        // Espera um pouco antes de terminar
        #2000ns; 
        $display("Simulação do pedal_top concluída.");
        $finish; 
    end

endmodule