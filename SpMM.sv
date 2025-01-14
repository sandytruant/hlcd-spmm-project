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

module sub_(
    input   logic   clock,
    input   data_t  a,
    input   data_t  b,
    output  data_t  out
);
    always_ff @(posedge clock) begin
        out.data <= a.data - b.data;
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

module RedUnit(
    input   logic               clock,
                                reset,
    input   data_t              data[`N-1:0],
    input   logic               split[`N-1:0],
    input   logic [`lgN-1:0]    out_idx[`N-1:0],
    input   logic               valid[`N-1:0],
    output  data_t              out_data[`N-1:0],
    output  logic               out_valid[`N-1:0],
    output  int                 delay,
    output  int                 num_el
);
    // num_el 总是赋值为 N
    assign num_el = `N;
    // delay 你需要自己为其赋值，表示电路的延迟
    assign delay = `lgN + 1;

    data_t add_a[`lgN-1:0][`N-1:0];
    data_t add_b[`lgN-1:0][`N-1:0];
    data_t add_out[`lgN-1:0][`N-1:0];

    data_t sub_a[`N-1:0];
    data_t sub_b[`N-1:0];
    data_t sub_out[`N-1:0];

    logic pipeline_valid[`lgN:0][`N-1:0];

    assign out_data = sub_out;
    assign out_valid = pipeline_valid[`lgN];

    logic pipeline_split[`lgN-1:0][`N-1:0];
    logic [`lgN-1:0] pipeline_out_idx[`lgN-1:0][`N-1:0];

    always_ff @( posedge clock ) begin
        for (int i = 0; i < `lgN; i++) begin
            for (int j = 0; j < `N; j++) begin
                if (i == 0) begin
                    pipeline_split[i][j] <= split[j];
                    pipeline_out_idx[i][j] <= out_idx[j];
                end
                else begin
                    pipeline_split[i][j] <= pipeline_split[i-1][j];
                    pipeline_out_idx[i][j] <= pipeline_out_idx[i-1][j];
                end
            end
        end
    end

    always_ff @(posedge clock) begin
        for (int i = 0; i <= `lgN; i++) begin
            for (int j = 0; j < `N; j++) begin
                if (i == 0) begin
                    pipeline_valid[i][j] <= valid[j];
                end
                else begin
                    pipeline_valid[i][j] <= pipeline_valid[i-1][j];
                end
            end
        end
    end

    generate
        for (genvar i = 0; i < `lgN; i++) begin
            for (genvar j = 0; j < `N; j++) begin
                add_ add_(
                    .clock(clock),
                    .a(add_a[i][j]),
                    .b(add_b[i][j]),
                    .out(add_out[i][j])
                );
            end
        end
    endgenerate

    generate
        for (genvar j = 0; j < `N; j++) begin
            sub_ sub_(
                .clock(clock),
                .a(sub_a[j]),
                .b(sub_b[j]),
                .out(sub_out[j])
            );
        end
    endgenerate

    always_comb begin
        for (int i = 0; i < `lgN; i++) begin
            for (int j = 0; j < `N; j++) begin
                if (i == 0) begin
                    assign add_a[i][j] = data[j];
                    assign add_b[i][j] = j > 0 ? data[j-1] : 0;
                end
                else begin
                    assign add_a[i][j] = add_out[i-1][j];
                    assign add_b[i][j] = j >= (1<<i) ? add_out[i-1][j-(1<<i)] : 0;
                end
            end
        end
    end

    logic first_one[`N-1:0];
    logic found;
    logic [`lgN-1:0] last_split[`N-1:0];

    // Calculate the last split of j
    always_comb begin
        assign found = 0;
        for (int i = 0; i < `N; i++) begin
            first_one[i] = 0;
            last_split[i] = 0;
        end
        for (int j = 0; j < `N; j++) begin
            if (pipeline_split[`lgN-1][j] == 1) begin
                if (found == 0) begin
                    first_one[j] = 1;
                    found = 1;
                end
                if (j < `N-1) begin
                    last_split[j+1] = `lgN'(j);
                end
            end
            else begin
                if (j < `N-1) begin
                    last_split[j+1] = last_split[j];
                end
            end
        end
    end

    always_comb begin
        for (int j = 0; j < `N; j++) begin
            assign sub_a[j] = 0;
            assign sub_b[j] = 0;
            if (pipeline_split[`lgN-1][pipeline_out_idx[`lgN-1][j]] == 1) begin
                assign sub_a[j] = add_out[`lgN-1][pipeline_out_idx[`lgN-1][j]];
                if (first_one[pipeline_out_idx[`lgN-1][j]] == 1) begin
                    assign sub_b[j] = 0;
                end
                else begin
                    assign sub_b[j] = add_out[`lgN-1][last_split[pipeline_out_idx[`lgN-1][j]]];
                end
            end
        end
    end
            
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
    assign delay = `lgN + 2;

    logic [`lgN:0] counter;
    logic [`dbLgN-1:0] ptr[`N-1:0];
    data_t mul_in1[`N-1:0];
    data_t mul_in2[`N-1:0];
    data_t mul_out[`N-1:0];

    logic red_split[`N-1:0];
    logic [`lgN-1:0] red_out_idx[`N-1:0];
    data_t red_out_data[`N-1:0];

    logic red_valid[`N-1:0];
    logic red_out_valid[`N-1:0];

    always_comb begin
        mul_in1 = lhs_data;
        for (int i = 0; i < `N; i++) begin
            mul_in2[i] = rhs[lhs_col[i]];
            out[i] = red_out_valid[i] ? red_out_data[i] : 0;
        end
    end

    always_ff @( posedge clock ) begin
        if (lhs_start) begin
            counter <= 1;
            ptr <= lhs_ptr;
        end
        if (counter > 0) begin
            counter <= counter + 1;
        end  
    end

    always_ff @( posedge clock ) begin
        for (int i = 0; i < `N; i++) begin
            red_split[i] <= 0;
            red_out_idx[i] <= 0;
            red_valid[i] <= 0;
        end
        if (lhs_start) begin
            for (int i = 0; i < `N; i++) begin
                if (lhs_ptr[i] < `N && (i == 0 || (i > 0 && lhs_ptr[i] != lhs_ptr[i-1]))) begin
                    red_split[lhs_ptr[i]] <= 1;
                    red_out_idx[i] <= lhs_ptr[i];
                    red_valid[i] <= 1;
                end
            end
        end
        else if (counter > 0) begin
            for (int i = 0; i < `N; i++) begin
                if (ptr[i] >= counter * `N && ptr[i] < (counter + 1) * `N && (i == 0 || (i > 0 && ptr[i] != ptr[i-1]))) begin
                    red_split[ptr[i]-counter*`N] <= 1;
                    red_out_idx[i] <= ptr[i]-counter*`N;
                    red_valid[i] <= 1;
                end
            end
        end
    end

    generate
        for (genvar i = 0; i < `N; i++) begin
            mul_ mul_(
                .clock(clock),
                .a(mul_in1[i]),
                .b(mul_in2[i]),
                .out(mul_out[i])
            );
        end
    endgenerate

    RedUnit red_unit(
        .clock(clock),
        .reset(reset),
        .data(mul_out),
        .split(red_split),
        .out_idx(red_out_idx),
        .valid(red_valid),
        .out_data(red_out_data),
        .out_valid(red_out_valid),
        .delay(),
        .num_el()
    );
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

    // assign lhs_ready_ns = 0;
    assign lhs_ready_ws = 0;
    assign lhs_ready_os = 0;
    assign lhs_ready_wos = 0;
    
    data_t rhs_buffer[`N-1:0][`N-1:0];
    data_t pe_out[`N-1:0][`N-1:0];
    logic pe_start[`N-1:0];

    logic [$clog2(`N/4)+1:0] rhs_buffer_counter;
    logic [$clog2(`N/4)+1:0] out_buffer_counter;

    logic [`lgN+1:0] pe_counter;

    data_t out_buffer[`N-1:0][`N-1:0];

    always_ff @(posedge clock) begin
        if (reset) begin
            rhs_ready <= 1;
        end
        else begin
            rhs_ready <= 0;
        end
    end

    always_ff @(posedge clock) begin
        if (rhs_start) begin
            rhs_buffer_counter <= 1;
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    rhs_buffer[j][i] <= rhs_data[i][j];
                end
            end
        end
        else if (rhs_buffer_counter < `N/4 && rhs_buffer_counter > 0) begin
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    rhs_buffer[j][i+rhs_buffer_counter*4] <= rhs_data[i][j];
                end
            end
            rhs_buffer_counter <= rhs_buffer_counter + 1;
        end
        else if (rhs_buffer_counter == `N/4) begin
            rhs_buffer_counter <= rhs_buffer_counter + 1;
        end
    end

    always_ff @( posedge clock ) begin
        if (rhs_buffer_counter == `N/4) begin
            lhs_ready_ns <= 1;
        end
        else begin
            lhs_ready_ns <= 0;
        end
    end

    always_ff @(posedge clock) begin
        if (rhs_buffer_counter == `N/4) begin
            for (int i = 0; i < `N; i++) begin
                pe_start[i] <= 1;
            end
        end
        else begin
            for (int i = 0; i < `N; i++) begin
                pe_start[i] <= 0;
            end
        end
    end

    always_ff @(posedge clock) begin
        if (lhs_start) begin
            pe_counter <= 1;
        end
        if (pe_counter > 0) begin
            pe_counter <= pe_counter + 1;
        end
    end

    always_ff @(posedge clock) begin
        for (int i = 0; i < `N; i++) begin
            for (int j = 0; j < `N; j++) begin
                if (pe_out[i][j] != 0) begin
                    out_buffer[i][j] <= pe_out[i][j];
                end
            end
        end
    end

    always_ff @(posedge clock) begin
        if (pe_counter == `lgN + `N + 2) begin
            out_buffer_counter <= 1;
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    out_data[i][j] <= out_buffer[j][i];
                end
            end
        end
        else if (out_buffer_counter < `N/4 && out_buffer_counter > 0) begin
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    out_data[i][j] <= out_buffer[j][i+out_buffer_counter*4];
                end
            end
            out_buffer_counter <= out_buffer_counter + 1;
        end
        else if (out_buffer_counter == `N/4) begin
            out_buffer_counter <= out_buffer_counter + 1;
        end
    end

    always_ff @(posedge clock) begin
        if (out_buffer_counter == `N/4) begin
            out_ready <= 1;
        end
        else begin
            out_ready <= 0;
        end
    end

    generate
        for (genvar i = 0; i < `N; i++) begin
            PE pe_(
                .clock(clock),
                .reset(reset),
                .lhs_start(pe_start[i]),
                .lhs_ptr(lhs_ptr),
                .lhs_col(lhs_col),
                .lhs_data(lhs_data),
                .rhs(rhs_buffer[i]),
                .out(pe_out[i]),
                .delay(),
                .num_el()
            );
        end
    endgenerate

endmodule