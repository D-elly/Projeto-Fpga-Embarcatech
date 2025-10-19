module top#(
    parameter int unsigned clock_max 25_000_000,
)(
    input wire sclk_in, //clock vindo do pico
    input wire mosi_in, //input data from pico 
    input wire active,
    input wire reset,

    output wire [16:0] audio_out,
    output wire[1:0] count_bit
);

process (clock_max, reset)
    begin
        if(reset = '1') begin  //implementação do botão de desligar ou desativar comunicação
            state = idle;
            shift_reg <= (others => '0');
            bit_counter <= (others => '0');
        end

        elseif rising_edge(clock_max) then //a cada pulso de clock do pico, executar
            case(state)
                idle: //modo ociosos, sem novos dados chegando
                    if(active = '0') begin 
                        state => idle;
                    end else if(active = '1') begin
                        state => receiving;
                        bit_counter <= (others => '0');
                        shift_reg <= (others => '0');
                    end 
                receiving: //estado em que a comunicação está acontecendo, recebendo dados do pico
                    if(active = '0') begin
                        state = idle;
                    end else if rising_edge(sclk_in) begin
                        if()

            endcase
    end 

endmodule
