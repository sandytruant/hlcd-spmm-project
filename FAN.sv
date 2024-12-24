`ifndef N
`define N              16
`endif
`define W               8
`define lgN     ($clog2(`N))
`define dbLgN (2*$clog2(`N))

typedef struct packed { logic [`W-1:0] data; } data_t;

module add_(
    input   logic   clock,
    input   data_t  a,
    input   data_t  b,
    output  data_t  out
);
    always_ff @(posedge clock) begin
        out.data <= a.data + b.data;
    end
endmodule

module mul_(
    input   logic   clock,
    input   data_t  a,
    input   data_t  b,
    output  data_t out
);
    always_ff @(posedge clock) begin
        out.data <= a.data * b.data;
    end
endmodule

module n_to_2_mux #(
    parameter n = 16, 
    parameter logn = ($clog2(n))
) (
    input data_t l_in[n-1:0],
    input data_t r_in[n-1:0],
    input logic [logn-1:0] l_sel,
    input logic [logn-1:0] r_sel,
    output data_t l_out,
    output data_t r_out
);

    always_comb begin : sel_mux
        l_out = l_in[l_sel];
        r_out = r_in[r_sel];
    end

endmodule

module adder_switch (
    input logic clock,
    input data_t in[1:0],
    input logic add_en,
    output data_t out[1:0]
);

    always_ff @( posedge clock ) begin : adder
        if (add_en) begin
            out[0].data <= in[0].data + in[1].data;
        end
        else begin
            out[0].data <= in[0].data;
            out[1].data <= in[1].data;
        end
    end
    
endmodule


module RedUnit(
    input   logic               clock,
                                reset,
    input   data_t              data[`N-1:0],
    input   logic               split[`N-1:0],
    input   logic [`lgN-1:0]    out_idx[`N-1:0],
    output  data_t              out_data[`N-1:0],
    output  int                 delay,
    output  int                 num_el
);
    // num_el 总是赋值为 N
    assign num_el = `N;
    // delay 你需要自己为其赋值，表示电路的延迟
    assign delay = 0;

    parameter int n_adder = `N - 1;
    parameter int n_mux = `N / 4 - 1;

    data_t adder_in[n_adder - 1:0][1:0];
    logic add_en[n_adder - 1:0];
    data_t adder_out[n_adder - 1:0][1:0];

    function static int calc_level(int i);
        int level = 0;
        for (int j = i + 1; j % 2 == 0; j /= 2) begin
            level++;
        end
        return level;
    endfunction

    generate
        for (genvar i = 0; i < n_mux; i++) begin
            localparam int n = calc_level(i) + 2;
            localparam int logn = $clog2(n);
            data_t l_in[n-1:0];
            data_t r_in[n-1:0];
            data_t l_out;
            data_t r_out;
            logic [logn-1:0] l_sel;
            logic [logn-1:0] r_sel;
            n_to_2_mux #(
                .n(n),
                .logn(logn)
            ) mux (
                .l_in(l_in),
                .r_in(r_in),
                .l_sel(l_sel),
                .r_sel(r_sel),
                .l_out(l_out),
                .r_out(r_out)
            );
        end
    endgenerate

    generate
        for (genvar i = 0; i < n_adder; i++) begin
            adder_switch adder (
                .clock(clock),
                .in(adder_in[i]),
                .add_en(add_en[i]),
                .out(adder_out[i])
            );
        end
    endgenerate
    
    generate
        for (genvar i = 0; i < n_adder; i++) begin
            
        end
    endgenerate
endmodule

module PE(
    input   logic               clock,
                                reset,
    input   logic               lhs_start,
    input   logic [`dbLgN-1:0]  lhs_ptr [`N-1:0],
    input   logic [`lgN-1:0]    lhs_col [`N-1:0],
    input   data_t              lhs_data[`N-1:0],
    input   data_t              rhs[`N-1:0],
    output  data_t              out[`N-1:0],
    output  int                 delay,
    output  int                 num_el
);
    // num_el 总是赋值为 N
    assign num_el = `N;
    // delay 你需要自己为其赋值，表示电路的延迟
    assign delay = 0;

    generate
        for(genvar i = 0; i < `N; i++) begin
            assign out[i] = 0;
        end
    endgenerate
endmodule

module SpMM(
    input   logic               clock,
                                reset,
    /* 输入在各种情况下是否 ready */
    output  logic               lhs_ready_ns,
                                lhs_ready_ws,
                                lhs_ready_os,
                                lhs_ready_wos,
    input   logic               lhs_start,
    /* 如果是 weight-stationary, 这次使用的 rhs 将保留到下一次 */
                                lhs_ws,
    /* 如果是 output-stationary, 将这次的结果加到上次的 output 里 */
                                lhs_os,
    input   logic [`dbLgN-1:0]  lhs_ptr [`N-1:0],
    input   logic [`lgN-1:0]    lhs_col [`N-1:0],
    input   data_t              lhs_data[`N-1:0],
    output  logic               rhs_ready,
    input   logic               rhs_start,
    input   data_t              rhs_data [3:0][`N-1:0],
    output  logic               out_ready,
    input   logic               out_start,
    output  data_t              out_data [3:0][`N-1:0],
    output  int                 num_el
);
    // num_el 总是赋值为 N
    assign num_el = `N;

    assign lhs_ready_ns = 0;
    assign lhs_ready_ws = 0;
    assign lhs_ready_os = 0;
    assign lhs_ready_wos = 0;
    assign rhs_ready = 0;
    assign out_ready = 0;
endmodule
