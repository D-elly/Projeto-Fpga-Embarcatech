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


assign miso_out = mosi_in[15:4];

always_ff @(posedge clk_25mhz or posedge reset)
    begin
        if(reset == 1'b1) begin  //implementação do botão de desligar ou desativar comunicação
            state <= IDLE;
            shift_reg <= 16'b0000000000000000;
            bit_counter <= 4'b0000;
            data_ready <= 1'b0;
            audio_out <= 16'b0000000000000000;
        

        end else begin //a cada pulso de clock do pico, executar
            if (data_ready == 1'b1) begin
                data_ready <= 1'b0;
            end

            case(state)
                IDLE: begin //modo ociosos, sem novos dados chegando
                    if(active == 1'b1) begin
                        state <= RECEIVING;
                        shift_reg <= 16'b0000000000000000;
                        bit_counter <= 4'b0000;
                    end
                end  

                RECEIVING: begin //estado em que a comunicação está acontecendo, recebendo dados do pico
                    if(active == 1'b0) begin
                        state <= IDLE;
                        
                    end else if (sclk_posedge) begin
                        shift_reg <= {shift_reg[14:0], mosi_in};

                        if(bit_counter == 4'd15) begin
                            audio_out <= {shift_reg[14:0], mosi_in};
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