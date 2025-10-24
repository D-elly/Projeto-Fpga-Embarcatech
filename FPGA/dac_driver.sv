module dac_driver #( 
    parameter int unsigned clock_max = 25_000_000
)(
    //recepção de dados de modulos eff

    input logic data_ready, 
    input logic [15:0] mosi_in,

    //pinos do protocolo de comunicação com modulo dac fisico
    output logic sclk_out, 
    output logic [11:0] miso_out, 
    output logic active_out
);


logic audio_clock = 400;
assign miso_out = mosi_in[15:4];

endmodule