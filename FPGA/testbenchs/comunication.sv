`timescale 1ns / 1ps

// Tornado módulo top para o projeto: expõe portas com os nomes usados em port_map.lpf
module top #(
    parameter int unsigned clock_max = 25_000_000
)(
    input logic clk_25mhz,
    input logic com_sclk_in, // clock vindo do pico (mapeado em port_map.lpf)
    input logic com_mosi_in, // input data from pico (mapeado em port_map.lpf)
    input logic com_active,
    input logic reset,

    output logic data_ready,
    output logic [15:0] audio_out, // mantém saída para uso interno/testes
    output logic teste_mosi // porta de saída no FPGA para eco dos bits recebidos (mapeada em port_map.lpf)
);

// Use explicit localparam-based states to avoid Yosys enum handling issues
localparam logic [1:0] IDLE      = 2'd0;
localparam logic [1:0] RECEIVING = 2'd1;

logic [1:0] state;      
logic [15:0] shift_reg; 
logic [3:0]  bit_counter;

logic sclk_d1, sclk_d2; 

//tornando o clock do pico um clock conhecido pelo fpga
always_ff @(posedge clk_25mhz) begin 
    sclk_d1 <= com_sclk_in;
    sclk_d2 <= sclk_d1;
end 

logic sclk_posedge;  // detecta borda de subida do sclk sincronizada ao clk_25mhz
assign sclk_posedge = (sclk_d1 == 1'b1) && (sclk_d2 == 1'b0);

always_ff @(posedge clk_25mhz or posedge reset)
    begin
        if(reset == 1'b1) begin  // implementação do botão de desligar ou desativar comunicação
            state <= IDLE;
            shift_reg <= 16'b0;
            bit_counter <= 4'b0000;
            data_ready <= 1'b0;
            audio_out <= 16'b0;
            teste_mosi <= 1'b0;
        end else begin // a cada pulso de clk_25mhz, executar lógica sincronizada
            // limpando data_ready se estava ativado (pulse)
            if (data_ready == 1'b1) begin
                data_ready <= 1'b0;
            end

            case(state)
                IDLE: begin // modo ocioso, sem novos dados chegando
                    if(com_active == 1'b1) begin
                        state <= RECEIVING;
                        shift_reg <= 16'b0;
                        bit_counter <= 4'b0000;
                    end
                end  

                RECEIVING: begin // estado em que a comunicação está acontecendo
                    if(com_active == 1'b0) begin
                        state <= IDLE;
                    end else if (sclk_posedge) begin
                        // shift in o bit mais recente
                        shift_reg <= {shift_reg[14:0], com_mosi_in};

                        // ecoa o bit recebido para a porta física de teste
                        teste_mosi <= com_mosi_in;

                        if(bit_counter == 4'd15) begin
                            audio_out <= {shift_reg[14:0], com_mosi_in};
                            state <= RECEIVING;
                            data_ready <= 1'b1;
                            bit_counter <= 4'b0000;
                        end else begin
                            bit_counter <= bit_counter + 1'b1;
                        end
                    end
                end 

            endcase
        end 
    end 
endmodule
