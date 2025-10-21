module eff_1(
    parameter int unsigned clock_max = 25_000_000
)( 
    input logic clk_25mhz, reset, 
    input logic data_ready, //sinal para começar o processamento de audio, vindo do comunication.sv
    input logic [15:0] audio_in,
    output logic audio_out
);


endmodule