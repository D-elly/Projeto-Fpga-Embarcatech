module top#(
    parameter int unsigned clock_max = 25_000_000
)(
    //entradas globais
    input logic clk_25mhz, reset, 

    //pinos de saida de dados SP, dac_driver.sv

    //pinos de entrada de dados SPI, comunication.sv
    input logic com_sclk_in, com_mosi_in, com_active
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

//copia modulo eff_1
    eff_1 #(.clock_max(clock_max) 
        )u_eff_1(
            .clk_25mhz(clk_25mhz), .reset(reset), 
            .data_ready(data_is_ready), .audio_in(original_audio),
            .audio_out(modified_audio)
        );

logic audio_clk = 400; //clock de teste para frequência do audio

//copia modulo dac_driver
    dac_driver #(.clock_max(clock_max))(
            .clk_25mhz(clk_25mhz), .reset(reset), 
            .data_ready(data_is_ready), .audio_in(output_audio),
            .sclk_out(audio_clk), .mosi_out, 
            .active_out,
    );


//passagem da faixa de audio, original ou modificada, 
assign output_audio = (reset)? original_audio : modified_audio;



endmodule