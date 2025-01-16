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

/*
function automatic int level(input int adder_idx);
    int count = 0;
    int temp = adder_idx + 1; // 加1后的值
    while (temp % 2 == 0) begin
        count++;
        temp = temp / 2; // 除以2
    end
    return count;
endfunction
*/

module RedUnit(
    input   logic               clock,
                                reset,
    input   data_t              data[`N-1:0],
    input   logic               split[`N-1:0],
    input   logic [`lgN-1:0]    out_idx[`N-1:0],
    input   logic               valid[`N-1:0],
    input   logic [`lgN-1:0]    halo_idx_in,
    input   logic               halo_valid_in,
    output  data_t              out_data[`N-1:0],
    output  logic               out_valid[`N-1:0],
    output  logic [`lgN-1:0]    halo_idx_out,
    output  logic               halo_valid_out,
    output  int                 delay,
    output  int                 num_el
);
    // num_el 总是赋值为 N
    assign num_el = `N;
    // delay 你需要自己为其赋值，表示电路的延迟
    assign delay = `lgN;

    logic add_en[`lgN-1:0][`N-2:0];
    logic bypass_en[`lgN-1:0][`N-2:0];
    data_t add_out[`lgN-1:0][`N-2:0];
    data_t bypass_out[`lgN-1:0][`N-2:0][1:0];
    logic [`lgN-1:0] vec_idx[`lgN-1:0][`N-1:0];

    int idx_count;
    int l_sel;
    int r_sel;

    int level[`N-1:0];
    initial begin
        for (int i = 0; i < `N; i++) begin
            int count = 0;
            int temp = i + 1;
            while (temp % 2 == 0) begin
                temp = temp / 2;
                count++;
            end
            level[i] = count;
        end
    end
    

    always_ff @( posedge clock ) begin
        if (reset) begin
            for (int i = 0; i < `lgN; i++) begin
                for (int j = 0; j < `N-1; j++) begin
                    add_en[i][j] <= 0;
                    bypass_en[i][j] <= 0;
                    add_out[i][j] <= 0;
                    bypass_out[i][j][0] <= 0;
                    bypass_out[i][j][1] <= 0;
                end
                for (int j = 0; j < `N; j++) begin
                    vec_idx[i][j] <= 0;
                end
            end
        end
        else begin
            for (int i = 0; i < `lgN; i++) begin
                if (i == 0) begin
                    // idx
                    idx_count = 0;
                    for (int adder_idx = 0; adder_idx < `N - 1; adder_idx++) begin
                        vec_idx[i][adder_idx] <= idx_count;
                        if (split[adder_idx] == 1) begin
                            idx_count = idx_count + 1;
                        end
                    end
                    vec_idx[i][`N-1] <= idx_count;

                    // others
                    for (int adder_idx = 0; adder_idx < `N - 1; adder_idx += 2) begin
                        add_en[i][adder_idx] <= 0;
                        bypass_en[i][adder_idx] <= 0;
                        if (split[adder_idx] == 1) begin
                            bypass_en[i][adder_idx] <= 1;
                            bypass_out[i][adder_idx][0] <= data[adder_idx];
                            bypass_out[i][adder_idx][1] <= data[adder_idx+1];
                        end
                        else begin
                            add_en[i][adder_idx] <= 1;
                            add_out[i][adder_idx] <= data[adder_idx] + data[adder_idx+1];
                        end
                    end
                end
                else begin
                    // pass vec_idx
                    for (int j = 0; j < `N; j++) begin
                        vec_idx[i][j] <= vec_idx[i-1][j];
                    end

                    for (int adder_idx = 0; adder_idx < `N - 1; adder_idx++) begin
                        if (level[adder_idx] < i) begin
                            // pass lower level data
                            add_en[i][adder_idx] <= add_en[i-1][adder_idx];
                            bypass_en[i][adder_idx] <= bypass_en[i-1][adder_idx];
                            add_out[i][adder_idx] <= add_out[i-1][adder_idx];
                            bypass_out[i][adder_idx] <= bypass_out[i-1][adder_idx];
                        end
                        else if (level[adder_idx] == i) begin
                            add_en[i][adder_idx] <= 1;
                            bypass_en[i][adder_idx] <= 0;

                            // left select
                            l_sel = adder_idx - (1 << (i-1));
                            for (int j = 0; j < i; j++) begin
                                int l_idx = adder_idx - (1 << j);
                                if (
                                    (vec_idx[i-1][l_idx] != vec_idx[i-1][adder_idx] && bypass_en[i-1][l_idx] == 0) ||
                                    (vec_idx[i-1][l_idx] == vec_idx[i-1][adder_idx] && add_en[i-1][l_idx] == 0 && bypass_en[i-1][l_idx] == 0)
                                ) begin
                                    if (j == 0) begin
                                        add_en[i][adder_idx] <= 0;
                                    end
                                    else begin
                                        l_sel = adder_idx - (1 << (j - 1));
                                    end
                                    break;
                                end
                            end

                            // right select
                            r_sel = adder_idx + (1 << (i-1));
                            for (int j = 0; j < i; j++) begin
                                int r_idx = adder_idx + (1 << j);
                                if (
                                    (vec_idx[i-1][r_idx] != vec_idx[i-1][adder_idx]) ||
                                    (vec_idx[i-1][r_idx] == vec_idx[i-1][adder_idx] && add_en[i-1][r_idx] == 0 && bypass_en[i-1][r_idx] == 0)
                                ) begin
                                    if (j == 0) begin
                                        add_en[i][adder_idx] <= 0;
                                    end
                                    else begin
                                        r_sel = adder_idx + (1 << (j - 1));
                                    end
                                    break;
                                end
                            end

                            if (bypass_en[i-1][l_sel] == 1 && bypass_en[i-1][r_sel] == 1) begin
                                add_out[i][adder_idx] <= bypass_out[i-1][l_sel][1] + bypass_out[i-1][r_sel][0];
                            end
                            else if (bypass_en[i-1][l_sel] == 1 && bypass_en[i-1][r_sel] == 0) begin
                                add_out[i][adder_idx] <= bypass_out[i-1][l_sel][1] + add_out[i-1][r_sel];
                            end
                            else if (bypass_en[i-1][l_sel] == 0 && bypass_en[i-1][r_sel] == 1) begin
                                add_out[i][adder_idx] <= add_out[i-1][l_sel] + bypass_out[i-1][r_sel][0];
                            end
                            else begin
                                add_out[i][adder_idx] <= add_out[i-1][l_sel] + add_out[i-1][r_sel];
                            end
                        end
                    end
                end
            end
        end
    end

    // output
    int vecID;
    int max_level;
    int out_adder_idx;
    always_comb begin
        for (int i = 0; i < `N; i++) begin
            vecID = vec_idx[`lgN-1][pipeline_out_idx[`lgN-1][i]];
            max_level = -1;
            out_adder_idx = pipeline_out_idx[`lgN-1][i];
            for (int j = pipeline_out_idx[`lgN-1][i] - 1; j >= 0; j--) begin
                if (vec_idx[`lgN-1][j] != vecID) begin
                    break;
                end
                else if (level[j] > max_level && add_en[`lgN-1][j] == 1) begin
                    max_level = level[j];
                    out_adder_idx = j;
                end
            end

            if (out_adder_idx == pipeline_out_idx[`lgN-1][i]) begin
                if (out_adder_idx % 2 == 0) begin
                    out_data[i] = bypass_out[`lgN-1][out_adder_idx][0];
                end
                else begin
                    out_data[i] = bypass_out[`lgN-1][out_adder_idx-1][1];
                end
            end
            else begin
                out_data[i] = add_out[`lgN-1][out_adder_idx];
            end
        end
    end
    
    // out_idx
    logic [`lgN-1:0] pipeline_out_idx[`lgN-1:0][`N-1:0];
    always_ff @( posedge clock ) begin
        for (int i = 0; i < `lgN; i++) begin
            if (i == 0) begin
                pipeline_out_idx[i] <= out_idx;
            end
            else begin
                pipeline_out_idx[i] <= pipeline_out_idx[i-1];
            end
        end
    end
    
    // out valid
    logic pipeline_valid[`lgN-1:0][`N-1:0];
    assign out_valid = pipeline_valid[`lgN-1];

    always_ff @(posedge clock) begin
        for (int i = 0; i < `lgN; i++) begin
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

    // halo
    logic [`lgN-1:0] pipeline_halo_idx[`lgN-1:0];
    logic pipeline_halo_valid[`lgN-1:0];

    always_ff @( posedge clock ) begin
        for (int i = 0; i < `lgN - 1; i++) begin
            pipeline_halo_idx[i+1] <= pipeline_halo_idx[i];
            pipeline_halo_valid[i+1] <= pipeline_halo_valid[i];
        end
        pipeline_halo_idx[0] <= halo_idx_in;
        pipeline_halo_valid[0] <= halo_valid_in;
    end

    always_comb begin
        halo_idx_out = pipeline_halo_idx[`lgN-1];
        halo_valid_out = pipeline_halo_valid[`lgN-1];
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

    logic [`lgN-1:0] halo_idx_in;
    logic [`lgN-1:0] halo_idx_out;
    logic halo_valid_in;
    logic halo_valid_out;

    logic [`lgN-1:0] halo_idx;
    logic halo_valid;
    data_t halo_data;

    always_comb begin
        mul_in1 = lhs_data;
        for (int i = 0; i < `N; i++) begin
            mul_in2[i] = rhs[lhs_col[i]];
        end
    end

    always_ff @( posedge clock ) begin
        for (int i = 0; i < `N; i++) begin
            if (halo_valid == 1 && halo_idx == i) begin
                out[i] <= red_out_valid[i] ? (red_out_data[i] + halo_data) : 0;
            end
            else begin
                out[i] <= (red_out_valid[i] == 1 && (halo_idx_out != i || halo_valid_out == 0)) ? red_out_data[i] : 0;
            end
        end
    end

    always_ff @( posedge clock ) begin
        halo_idx <= halo_idx_out;
        halo_valid <= halo_valid_out;
        halo_data <= red_out_data[halo_idx_out];
    end

    always_ff @( posedge clock ) begin
        if (lhs_start) begin
            counter <= 1;
            ptr <= lhs_ptr;
        end
        else if (counter > 0) begin
            counter <= counter + 1;
        end  
    end

    always_ff @( posedge clock ) begin
        for (int i = 0; i < `N; i++) begin
            red_split[i] <= 0;
            red_out_idx[i] <= 0;
            red_valid[i] <= 0;
            halo_valid_in <= 0;
            halo_idx_in <= 0;
        end
        if (lhs_start) begin
            for (int i = 0; i < `N; i++) begin
                if (lhs_ptr[i] < `N && (i == 0 || (i > 0 && lhs_ptr[i] != lhs_ptr[i-1]))) begin
                    red_split[lhs_ptr[i]] <= 1;
                    red_out_idx[i] <= lhs_ptr[i];
                    red_valid[i] <= 1;
                end
                else if (i > 0 && lhs_ptr[i-1] < `N - 1 && lhs_ptr[i] >= `N ) begin
                    red_split[`N-1] <= 1;
                    red_out_idx[i] <= `N - 1;
                    red_valid[i] <= 1;
                    halo_idx_in <= i;
                    halo_valid_in <= 1;
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
                else if (i > 0 && ptr[i-1] < (counter + 1) * `N - 1 && ptr[i] >= (counter + 1) * `N ) begin
                    red_split[`N-1] <= 1;
                    red_out_idx[i] <= `N - 1;
                    red_valid[i] <= 1;
                    halo_idx_in <= i;
                    halo_valid_in <= 1;
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
        .halo_idx_in(halo_idx_in),
        .halo_valid_in(halo_valid_in),
        .out_data(red_out_data),
        .out_valid(red_out_valid),
        .halo_idx_out(halo_idx_out),
        .halo_valid_out(halo_valid_out),
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
    
    data_t rhs_buffer[1:0][`N-1:0][`N-1:0];
    data_t pe_out[`N-1:0][`N-1:0];
    logic pe_start[`N-1:0];

    logic [$clog2(`N/4)+1:0] rhs_buffer_counter;
    logic [$clog2(`N/4)+1:0] out_buffer_counter;

    logic [`lgN+1:0] pe_counter;

    data_t out_buffer[1:0][`N-1:0][`N-1:0];

    logic [1:0] rhs_buffer_state[1:0]; // 0: available, 1: loading, 2: loaded , 3: calculating
    logic [1:0] out_buffer_state[1:0]; // 0: available, 1: calculating, 2: calculated, 3: outputting

    logic rhs_buffer_select;

    logic out_buffer_os[1:0];

    logic calc_os;

    always_ff @( posedge lhs_os ) begin
        if (lhs_os) begin
            calc_os <= 1;
        end
    end

    always_ff @( posedge clock ) begin
        if (pe_counter == `lgN + `N + 2 && lhs_os == 0) begin
            calc_os <= 0;
        end
    end

    always_ff @( posedge clock ) begin : lhs_ready_ff
        if (reset) begin
            lhs_ready_ns <= 0;
            lhs_ready_ws <= 0;
        end 
        else if (lhs_start) begin
            lhs_ready_ns <= 0;
            lhs_ready_ws <= 0;
        end
        else if (
            (rhs_buffer_state[0] == 2 || rhs_buffer_state[1] == 2) && 
            (out_buffer_state[0] == 0 || out_buffer_state[1] == 0) && 
            (rhs_buffer_state[0] != 3 && rhs_buffer_state[1] != 3) &&
            (out_buffer_state[0] != 1 && out_buffer_state[1] != 1)
        ) begin
            lhs_ready_ns <= 1;
            lhs_ready_ws <= 1;
        end
    end

    always_ff @( posedge clock ) begin
        if (pe_counter == `lgN + `N + 2) begin
            if (out_buffer_state[0] == 1) begin
                out_buffer_os[0] <= 1;
                out_buffer_os[1] <= 0;
            end
            else if (out_buffer_state[1] == 1) begin
                out_buffer_os[0] <= 0;
                out_buffer_os[1] <= 1;
            end
            else begin
                out_buffer_os[0] <= 0;
                out_buffer_os[1] <= 0;
            end

            lhs_ready_os <= 1;
        end
        if (lhs_os) begin
            lhs_ready_os <= 0;
        end
    end

    always_ff @( posedge clock ) begin
        if (reset) begin
            lhs_ready_wos <= 0;
        end 
        else if (lhs_start || lhs_os) begin
            lhs_ready_wos <= 0;
        end
        else if (pe_counter == `lgN + `N + 2) begin
            lhs_ready_wos <= 1;
        end
    end

    always_ff @(posedge clock) begin : rhs_ready_ff
        if (reset) begin
            rhs_ready <= 1;
        end
        else if (rhs_start && rhs_ready) begin
            rhs_ready <= 0;
        end
        else if (rhs_buffer_state[0] != 1 && rhs_buffer_state[1] != 1 && (rhs_buffer_state[0] == 0 || rhs_buffer_state[1] == 0)) begin
            rhs_ready <= 1;
        end
    end

    always_ff @( posedge clock ) begin : rhs_buffer_state_ff
        if (reset) begin
            rhs_buffer_state[0] <= 0;
            rhs_buffer_state[1] <= 0;
        end
        else if (rhs_start && rhs_ready) begin
            if (rhs_buffer_state[0] == 0) begin
                rhs_buffer_state[0] <= 1;
            end
            else if (rhs_buffer_state[1] == 0) begin
                rhs_buffer_state[1] <= 1;
            end
        end
        if (rhs_buffer_counter == `N / 4) begin
            if (rhs_buffer_state[0] == 1) begin
                rhs_buffer_state[0] <= 2;
            end
            else if (rhs_buffer_state[1] == 1) begin
                rhs_buffer_state[1] <= 2;
            end
        end
        if (lhs_start) begin
            if (rhs_buffer_state[0] == 2) begin
                rhs_buffer_state[0] <= 3;
            end
            else if (rhs_buffer_state[1] == 2) begin
                rhs_buffer_state[1] <= 3;
            end
        end
        if (pe_counter == `lgN + `N + 2) begin
            if (rhs_buffer_state[0] == 3) begin
                if (lhs_ws) begin
                    rhs_buffer_state[0] <= 2;
                end
                else begin
                    rhs_buffer_state[0] <= 0;
                end
            end
            else if (rhs_buffer_state[1] == 3) begin
                if (lhs_ws) begin
                    rhs_buffer_state[1] <= 2;
                end
                else begin
                    rhs_buffer_state[1] <= 0;
                end
            end
        end
    end

    always_ff @( posedge clock or posedge out_start ) begin : out_buffer_state_ff
        if (reset) begin
            out_buffer_state[0] <= 0;
            out_buffer_state[1] <= 0;
        end
        else if (lhs_start) begin
            if (lhs_os == 1) begin
                if (out_buffer_state[0] == 2 && out_buffer_os[0] == 1) begin
                    out_buffer_state[0] <= 1;
                end
                if (out_buffer_state[1] == 2 && out_buffer_os[1] == 1) begin
                    out_buffer_state[1] <= 1;
                end
            end
            else begin
                if (out_buffer_state[0] == 0) begin
                    out_buffer_state[0] <= 1;
                end
                else if (out_buffer_state[1] == 0) begin
                    out_buffer_state[1] <= 1;
                end
            end
        end
        if (pe_counter == `lgN + `N + 2) begin
            if (out_buffer_state[0] == 1) begin
                out_buffer_state[0] <= 2;
            end
            else if (out_buffer_state[1] == 1) begin
                out_buffer_state[1] <= 2;
            end
        end
        if (out_start) begin
            if (out_buffer_state[0] == 2) begin
                out_buffer_state[0] <= 3;
            end
            else if (out_buffer_state[1] == 2) begin
                out_buffer_state[1] <= 3;
            end
        end
        if (out_buffer_counter == `N / 4) begin
            if (out_buffer_state[0] == 3) begin
                out_buffer_state[0] <= 0;
            end
            else if (out_buffer_state[1] == 3) begin
                out_buffer_state[1] <= 0;
            end
        end
            
    end

    always_ff @(posedge clock) begin
        if (reset) begin
            rhs_buffer_counter <= 0;
        end
        else if (rhs_start && rhs_ready) begin
            rhs_buffer_counter <= 1;
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    if (rhs_buffer_state[0] == 0) begin
                        rhs_buffer[0][j][i] <= rhs_data[i][j];
                    end
                    else if (rhs_buffer_state[1] == 0) begin
                        rhs_buffer[1][j][i] <= rhs_data[i][j];
                    end
                end
            end
        end
        else if (rhs_buffer_counter < `N / 4 && rhs_buffer_counter > 0) begin
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    if (rhs_buffer_state[0] == 1) begin
                        rhs_buffer[0][j][i+rhs_buffer_counter*4] <= rhs_data[i][j];
                    end
                    else if (rhs_buffer_state[1] == 1) begin
                        rhs_buffer[1][j][i+rhs_buffer_counter*4] <= rhs_data[i][j];
                    end  
                end
            end
            rhs_buffer_counter <= rhs_buffer_counter + 1;
        end
        else if (rhs_buffer_counter == `N / 4) begin
            rhs_buffer_counter <= rhs_buffer_counter + 1;
        end
    end

    always_ff @( posedge clock or posedge lhs_os ) begin // modified
        if (
            (rhs_buffer_state[0] == 2 || rhs_buffer_state[1] == 2) && 
            (out_buffer_state[0] == 0 || out_buffer_state[1] == 0) && 
            (rhs_buffer_state[0] != 3 && rhs_buffer_state[1] != 3) &&
            (out_buffer_state[0] != 1 && out_buffer_state[1] != 1)
        ) begin
            rhs_buffer_select <= rhs_buffer_state[0] == 2 ? 0 : 1;
        end
    end

    always_ff @(posedge clock) begin : pe_start_ff
        if ((rhs_buffer_state[0] == 2 || rhs_buffer_state[1] == 2) && (out_buffer_state[0] == 0 || out_buffer_state[1] == 0) && lhs_start == 0) begin
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

    always_ff @(posedge clock) begin : pe_counter_ff
        if (reset) begin
            pe_counter <= 0;
        end
        else if (lhs_start) begin
            pe_counter <= 1;
        end
        else if (pe_counter > 0) begin
            pe_counter <= pe_counter + 1;
        end
    end

    always_ff @(posedge clock) begin
        if (lhs_start) begin
            for (int sel = 0; sel < 2; sel++) begin
                for (int i = 0; i < `N; i++) begin
                    for (int j = 0; j < `N; j++) begin
                        if (out_buffer_state[sel] == 0) begin
                            out_buffer[sel][i][j] <= 0;
                        end
                    end
                end
            end
        end
        else begin
            for (int i = 0; i < `N; i++) begin
                for (int j = 0; j < `N; j++) begin
                    if (pe_out[i][j] != 0) begin
                        if (out_buffer_state[0] == 1) begin
                            if (calc_os == 1 && out_buffer_os[0] == 1) begin
                                out_buffer[0][i][j] <= pe_out[i][j] + out_buffer[0][i][j];
                            end
                            else begin
                                out_buffer[0][i][j] <= pe_out[i][j];
                            end
                        end
                        else if (out_buffer_state[1] == 1) begin
                            if (calc_os == 1 && out_buffer_os[1] == 1) begin
                                out_buffer[1][i][j] <= pe_out[i][j] + out_buffer[1][i][j];
                            end
                            else begin
                                out_buffer[1][i][j] <= pe_out[i][j];
                            end
                        end
                    end
                end
            end
        end
    end

    always_ff @(posedge clock or posedge out_start) begin
        if (out_start) begin
            out_buffer_counter <= 1;
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    if (out_buffer_state[0] == 2) begin
                        out_data[i][j] <= out_buffer[0][j][i];
                    end
                    else if (out_buffer_state[1] == 2) begin
                        out_data[i][j] <= out_buffer[1][j][i];
                    end
                end
            end
        end
        if (out_buffer_counter < `N/4 && out_buffer_counter > 0) begin
            for (int i = 0; i < 4; i++) begin
                for (int j = 0; j < `N; j++) begin
                    if (out_buffer_state[0] == 3) begin
                        out_data[i][j] <= out_buffer[0][j][i+out_buffer_counter*4];
                    end
                    else if (out_buffer_state[1] == 3) begin
                        out_data[i][j] <= out_buffer[1][j][i+out_buffer_counter*4];
                    end
                end
            end
            out_buffer_counter <= out_buffer_counter + 1;
        end
        else if (out_buffer_counter == `N/4) begin
            out_buffer_counter <= out_buffer_counter + 1;
        end
    end

    always_ff @(posedge clock) begin
        if (reset) begin
            out_ready <= 0;
        end
        if (pe_counter == `lgN + `N + 2) begin
            out_ready <= 1;
        end
        else if (out_start || lhs_os) begin
            out_ready <= 0;
        end
    end

    generate
        for (genvar i = 0; i < `N; i++) begin
            PE pe_(
                .clock(clock),
                .reset(reset),
                .lhs_start(lhs_start),
                .lhs_ptr(lhs_ptr),
                .lhs_col(lhs_col),
                .lhs_data(lhs_data),
                .rhs(rhs_buffer[rhs_buffer_select][i]),
                .out(pe_out[i]),
                .delay(),
                .num_el()
            );
        end
    endgenerate

endmodule