module dac_driver #( 
    parameter int unsigned clock_max = 25_000_000
)(
    //recepção de dados de audio(modificado ou original)
    input logic clk_25mhz,
    input logic data_ready, //trigger para sincronização de clock com outros módulo
    input logic [15:0] mosi_in, //audio de entrada
    input logic reset, //apenas para dar start e integrar com outros módulo auxilares

    //pinos do protocolo de comunicação com modulo dac fisico
    output logic sclk_out, 
    output logic mosi_out, //mandando bit a bit para slave spi
    output logic active_out,
    input logic miso_out //util p/ debbug, verificar se dac está convertendo certo
);

//modos da FSM p/ master SPI
typedef enum logic {
    DAC_IDLE, 
    DAC_TRANSFER
    //DAC_RECEIVING  //opcional, util para debug
}state_t;

state_t state;

//geração do clock para comunicação com slave SPI, tecnica vista em sala
localparam int sclk_div_count = 5; //para clock de 2mhz, um pulso a cada 
logic [$clog2(sclk_div_count) -1:0] count_clock = '0; //var de tamanho de 2Mhz, para contar pulsos
logic internal_clock = 1'b0;

logic [15:0] shift_reg; 
logic [3:0]  bit_counter;
logic busy;

always_ff @(posedge clk_25mhz or posedge reset) begin
    if(reset) begin
        count_clock <= '0;
        internal_clock <= 1'b0;

    end else if(state == DAC_TRANSFER) begin
        if(count_clock == sclk_div_count) begin  //contador de clock interno atingido
            count_clock <= '0;
            internal_clock <= ~internal_clock;

        end else begin  //continuar incrementação
            count_clock <= count_clock + 1;
        end
    end else begin  //se manter em modo DAC_IDLE
        count_clock <= '0;
        internal_clock <= 1'b0;
    end 
end

assign sclk_out = internal_clock;

//FSM para controle principal do master e slave SPI
always_ff @(posedge clk_25mhz or posedge reset) begin
    if(reset) begin 
        state <= DAC_IDLE;
        active_out <= 1'b1;
        mosi_out <= 1'b0;
        busy<= 1'b0;
        bit_counter <= 4'b0000;
    end

    busy <= 1'b0;
    case (state)
        DAC_IDLE: begin
            active_out <= 1'b1;
            mosi_out <= 1'b0;
            busy <= 1'b0;

            if(data_ready) begin 
                state <= DAC_TRANSFER;
                
                shift_reg <= {1'b1, 3'b000, mosi_in[15:4]};  //configurando mensagem p/ SPI slave com cabeçalo e truncamento de dados para saída
                bit_counter <= 4'b0000;
                active_out <= 1'b0;
                busy <= 1'b1;
            end 
        end

        DAC_TRANSFER: begin 
            busy <= 1'b1;
            active_out <= 1'b0;

            if(count_clock == sclk_div_count) begin 
                if(internal_clock == 1'b0) begin 
                    if(bit_counter < 16) begin 
                        mosi_out <= shift_reg[15];
                        shift_reg <= shift_reg << 1; //desliza bits, do maior para menor
                        bit_counter <= bit_counter + 1;
                        state <= DAC_TRANSFER;
                    
                    end else begin //após mandar uma amostra, volta para DAC_IDLE e espera próximo sinal data_ready
                        state <= DAC_IDLE;
                        active_out <= 1'b1;
                        busy <= 1'b0;
                    end
                end
            end

        end
    endcase        
end


endmodule
