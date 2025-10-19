module comunication #(
    parameter int unsigned clock_max 25_000_000
)(
    input wire sclk_in, //clock vindo do pico
    input wire mosi_in, //input data from pico 
    input wire active,
    input wire reset,

    output wire [15:0] audio_out,  //testar se pacotes de 16 bits fazem uma transnmissão lisa
    output logic data_ready
);

typedef enum logic {
        IDLE,    
        RECEIVING  
    } state_t;

state_t state;       
logic [15:0] shift_reg; 
logic [3:0]  bit_counter;

logic sclk_d1, sclk_d2; 

//tornando o clock do pico um clock conhecido pelo fpga
always_ff @(posedge clock_max) begin 
        sclk_d1 <= sclk_in;
        sclk_d2 <= sclk_d1;
end 

assign sclk_posedge = (sclk_d1 == 1'b1) && (sclk_d2 == 1'b0);

always_ff @(posedge clock_max or posedge reset)
    begin
        if(reset = '1') begin  //implementação do botão de desligar ou desativar comunicação
            state = IDLE;
            shift_reg <= 16'b0000000000000000;
            bit_counter <= 4'b0000;
            data_ready <= 1'b0;
        end

        else begin //a cada pulso de clock do pico, executar
            case(state)
                IDLE: begin //modo ociosos, sem novos dados chegando
                    if(active == '0') begin 
                        state => IDLE;
                    end else if(active == '1') begin
                        state => RECEIVING;
                        shift_reg <= 16'b0000000000000000;
                        bit_counter <= 4'b0000;
                    end
                end  

                RECEIVING: begin //estado em que a comunicação está acontecendo, recebendo dados do pico
                    if(active = '0') begin
                        state = IDLE;
                    end else if (sclk_posedge) begin
                        shift_reg <= {shift_reg[14:0], mosi_in};

                        if(bit_counter == 4d'15) begin
                            audio_out <= {shift_reg[14:0], mosi_in};
                            state <= RECEIVING;
                            shift_reg <= 16'b0000000000000000;
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
