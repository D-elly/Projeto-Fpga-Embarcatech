module dac_driver #( 
    parameter int unsigned clock_max = 25_000_000
)(
    //recepção de dados de audio(modificado ou original)
    input logic clk_25mhz,
    input logic data_ready, //trigger para sincronização de clock com outros módulo
    input logic [15:0] mosi_in, //audio de entrada
    input logic reset, //apenas para dar start e integrar com outros módulo auxilares

    //pinos do protocolo de comunicação com modulo dac fisico
    output logic spi_sclk_out, 
    output logic spi_mosi_out, //mandando bit a bit para slave spi
    output logic spi_active_out,
    input logic spi_miso_out //util p/ debbug, verificar se dac está convertendo certo
);

//modos da FSM p/ master SPI
// Use explicit localparam-based states to avoid Yosys enum handling issues
localparam logic [1:0] SPI_IDLE     = 2'd0;
localparam logic [1:0] SPI_TRANSFER = 2'd1;

logic [1:0] spi_state;

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

    end else if(spi_state == SPI_TRANSFER) begin
        if(count_clock == sclk_div_count) begin  //contador de clock interno atingido
            count_clock <= '0;
            internal_clock <= ~internal_clock;

        end else begin  //continuar incrementação
            count_clock <= count_clock + 1;
        end
    end else begin  //se manter em modo SPI_IDLE
        count_clock <= '0;
        internal_clock <= 1'b0;
    end 
end

assign spi_sclk_out = internal_clock;

//FSM para controle principal do master e slave SPI
always_ff @(posedge clk_25mhz or posedge reset) begin
    if (reset) begin
        spi_state <= SPI_IDLE;
        spi_active_out <= 1'b1;
        spi_mosi_out <= 1'b0;
        busy <= 1'b0;
        bit_counter <= 4'b0000;
        shift_reg <= 16'b0;
    end else begin

        // default values for this clock cycle
        busy <= 1'b0;

        case (spi_state)
        SPI_IDLE: begin
                spi_active_out <= 1'b1;
                spi_mosi_out <= 1'b0;
                busy <= 1'b0;

                if (data_ready) begin
                    spi_state <= SPI_TRANSFER;
                    // preparando a mensagem para enviar ao DAC (header + truncamento)
                    shift_reg <= {1'b1, 3'b000, mosi_in[15:4]};
                    bit_counter <= 4'b0000;
                    spi_active_out <= 1'b0;
                    busy <= 1'b1;
                end
            end

        SPI_TRANSFER: begin
                busy <= 1'b1;
                spi_active_out <= 1'b0;

                if (count_clock == sclk_div_count) begin
                    if (internal_clock == 1'b0) begin
                        if (bit_counter < 16) begin
                            spi_mosi_out <= shift_reg[15];
                            shift_reg <= shift_reg << 1; // desloca bits, do MSB para LSB
                            bit_counter <= bit_counter + 1;
                            spi_state <= SPI_TRANSFER;

                        end else begin
                            // depois de enviar a amostra completa, volta ao idle
                            spi_state <= SPI_IDLE;
                            spi_active_out <= 1'b1;
                            busy <= 1'b0;
                        end
                    end
                end

            end
        endcase

    end
end


endmodule