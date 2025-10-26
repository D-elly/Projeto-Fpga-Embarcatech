`timescale 1ns / 1ps

module tb_dac_driver;

    // --- Constantes ---
    localparam int CLK_PERIOD = 40; // 40 ns = 25MHz

    // --- Sinais do Testbench ---
    logic        tb_clk_25mhz;
    logic        tb_reset;
    logic        tb_data_ready;
    logic [15:0] tb_audio_in;
    logic        tb_spi_miso_in; // Entrada para o DUT (simula SDO do DAC)

    // Fios para observar as saídas do DUT
    wire         w_sclk_out;
    wire         w_mosi_out;
    wire         w_active_out;

    // --- Instanciar o DUT ---
    dac_driver dut (
        .clk_25mhz    (tb_clk_25mhz),
        .reset        (tb_reset),
        .data_ready   (tb_data_ready),
        .mosi_in (tb_audio_in),
        .sclk_out     (w_sclk_out),
        .mosi_out     (w_mosi_out),
        .active_out   (w_active_out),
        .miso_out  (tb_spi_miso_in)
    );

    // --- Gerador de Clock ---
    initial begin
        tb_clk_25mhz = 1'b0;
        forever #(CLK_PERIOD / 2) tb_clk_25mhz = ~tb_clk_25mhz;
    end

    // --- Sequência de Teste Principal ---
    initial begin
        $dumpfile("dump_dac_driver.vcd");
        $dumpvars(0, tb_dac_driver); 

        $display("Iniciando simulação do dac_driver... Reset ativado.");
        tb_reset       <= 1'b1;
        tb_data_ready  <= 1'b0;
        tb_audio_in    <= 16'b0;
        tb_spi_miso_in <= 1'b0; // Flutuando
        #100ns;

        tb_reset <= 1'b0;
        $display("Reset liberado. DUT em IDLE.");
        @(posedge tb_clk_25mhz);

        // --- Teste 1: Enviar um valor ---
        $display("TESTE 1: Enviando áudio 16'hABCD...");
        tb_audio_in <= 16'hABCD;
        @(posedge tb_clk_25mhz); // Espera 1 ciclo para o valor estabilizar

        // Gera o pulso data_ready
        $display("Gerando pulso data_ready...");
        tb_data_ready <= 1'b1;
        @(posedge tb_clk_25mhz);
        tb_data_ready <= 1'b0;
        $display("Pulso data_ready enviado.");

        // Monitora a atividade SPI
        $display("Monitorando SPI...");
        // Espera o CS (active_out) ficar ativo (baixo)
        wait (w_active_out == 1'b0);
        $display("... CS ficou ativo (baixo).");

        // Espera o CS voltar a ficar inativo (alto) - indica fim da transferência
        wait (w_active_out == 1'b1);
        $display("... CS ficou inativo (alto). Transferência concluída.");
        $display("TESTE 1 Concluído.");
        
        // --- Teste 2: Enviar outro valor ---
        #500ns; // Pequena pausa
        
        $display("TESTE 2: Enviando áudio 16'h1234...");
        tb_audio_in <= 16'h1234;
        @(posedge tb_clk_25mhz); 

        $display("Gerando pulso data_ready...");
        tb_data_ready <= 1'b1;
        @(posedge tb_clk_25mhz);
        tb_data_ready <= 1'b0;
        $display("Pulso data_ready enviado.");

        $display("Monitorando SPI...");
        wait (w_active_out == 1'b0);
        $display("... CS ficou ativo (baixo).");
        wait (w_active_out == 1'b1);
        $display("... CS ficou inativo (alto). Transferência concluída.");
        $display("TESTE 2 Concluído.");

        // --- Fim ---
        #1000ns;
        $display("Simulação do dac_driver concluída.");
        $finish;
    end

endmodule