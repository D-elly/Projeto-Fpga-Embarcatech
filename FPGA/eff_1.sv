module eff_1 #(
    parameter int unsigned clock_max = 25_000_000
)( 
    input  logic clk_25mhz,
    input  logic data_ready,               // sinal para começar o processamento de áudio
    input  logic signed [15:0] audio_in,   // entrada PCM
    output logic signed [15:0] audio_out,  // saída com efeito
    output logic process_status            // status do processamento
);

    // Limite de clipping (ajustável)
    parameter signed [15:0] CLIP_LEVEL = 10000;

    always_ff @(posedge clk_25mhz) begin
        if (data_ready) begin
            process_status <= 1'b1;

            // Aplicando hard clipping
            if (audio_in > CLIP_LEVEL)
                audio_out <= CLIP_LEVEL;
            else if (audio_in < -CLIP_LEVEL)
                audio_out <= -CLIP_LEVEL;
            else
                audio_out <= audio_in;
        end else begin
            process_status <= 1'b0;
            audio_out <= 16'sd0; // ou manter o último valor, se preferir
        end
    end

endmodule
