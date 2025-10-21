module dac_driver #( 
    parameter int unsigned clock_max = 25_000_000
)(
    //recepção de dados de modulos eff
    input logic clk_25mhz, reset, 

    input logic data_ready, 
    input logic [15:0] audio_in,

    //pinos do protocolo de comunicação com modulo dac fisico
    input logic sclk_out 
    input logic mosi_out, 
    input logic active_out,
);

endmodule