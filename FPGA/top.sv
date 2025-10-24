module top#(
    parameter int unsigned clock_max = 25_000_000,
    parameter int unsigned audio_clk = 400
)(
    //entradas globais
    input logic clk_25mhz, reset, 

    //pinos de entrada de dados SPI, comunication.sv
    input logic com_sclk_in, com_mosi_in, com_active,

    //pinos de saida de dados do dac_driver.sv
    output logic spi_audio_clk, 
    output logic [15:0] spi_mosi_out, 
    output logic spi_active_out,
    output logic [11:0] spi_miso_out
);

logic data_is_ready;  //sinal interno do top-level
logic [15:0] original_audio; //faixa de audio original de comunication.sv
logic [15:0] modified_audio; //faixa de saida de modulos de efeito
logic [15:0] output_audio;  //faixa que irá para saida do fpga


//copia modulo comunication
    comunication #(.clock_max(clock_max) 
        )u_comunication(
            .clk_25mhz(clk_25mhz), .sclk_in(com_sclk_in), 
            .mosi_in(com_mosi_in), .active(com_active),
            .reset(reset), .audio_out(original_audio),
            .data_ready(data_is_ready)
        );


logic modified_status; //armazena se aplicação de efeito terminou
//copia modulo eff_1
    eff_1 #(.clock_max(clock_max) 
        )u_eff_1(
            .clk_25mhz(clk_25mhz), .data_ready(data_is_ready), 
            .audio_in(original_audio), .audio_out(modified_audio), 
            .process_status(modified_status)
        );

//passagem da faixa de audio, original ou modificada, 
assign output_audio = (reset)?  modified_audio: original_audio;

//copia modulo dac_driver
    dac_driver #(.clock_max(clock_max) 
        )u_dac_driver(
            //entradas do fpga -> dac
            .data_ready(data_is_ready),
            .mosi_in(output_audio),

            //saidas do dac -> amplificador
            .sclk_out(spi_audio_clk), .miso_out(spi_miso_out), 
            .active_out(spi_active_out)
    );


endmodule