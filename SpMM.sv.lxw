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
    input   logic  reset,
    output  data_t  out
);
    always_ff @(posedge clock or posedge reset ) begin
        if(reset)begin
            out.data <= 0;
        end
        else begin  
        out.data <= a.data + b.data;
        end
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

module DFF #(
    parameter WIDTH = `W  // 默认宽度与结构体中字段宽度一致
) (
    input logic                 clock,
    input logic                 reset,
    input data_t                d,      // 输入为结构体类型
    output data_t               q       // 输出为结构体类型
);
    always_ff @(posedge clock or posedge reset) begin
        if (reset) begin
            q.data <= '0;  // 对结构体中字段赋初值
        end else begin
            q.data <= d.data;  // 保持结构体赋值
        end
    end
endmodule


module RedUnit(
    input   logic               clock,
                                reset,
    input   logic               lhs_start,
    input   data_t              data[`N-1:0],
    input   wire                split[`N-1:0],
    input   logic [`lgN-1:0]    out_idx[`N-1:0],
    output  data_t              out_data[`N-1:0],
    output  int                 delay,
    output  int                 num_el,
    output  logic               halo_enable,
    output  data_t              halo_data
);
    // 固定参数
    assign num_el = `N;            // 元素数量
    assign delay = `lgN + 1;           // 树结构深度决定延迟

    // 定义中间结果存储的二叉树结构数组
    wire data_t stage [`lgN:0][`N-1:0]; // 每层的累积和结果
    wire data_t prefix_sum[`N-1:0];     // 前缀和结果   
    
    reg [`lgN-1:0] out_idx_reg[`lgN-1:0][`N-1:0]; // out_idx 的移位寄存器，延迟 lgN
    reg split_reg[`lgN-1:0][`N-1:0];             // split 的移位寄存器，延迟 lgN

    // 完成 out_idx 和 split 的移位寄存器
    always_ff @(posedge clock or posedge reset) begin
        if (reset && ! lhs_start ) begin
            for(int i = 0; i < `lgN; i = i + 1) begin
                for (int j = 0; j < `N; j = j + 1) begin
                    out_idx_reg[i][j] <= 0;
                    split_reg[i][j] <= 0;
                end
            end
        end else begin
            // 第 0 层赋值为输入
            for (int j = 0; j < `N; j = j + 1) begin
                out_idx_reg[0][j] <= out_idx[j];
                split_reg[0][j] <= split[j];
            end
        end

        // 逐级移位，延迟深度为 lgN
        for (int i = 1; i < `lgN; i = i + 1) begin
            if (reset) begin
                for (int j = 0; j < `N; j = j + 1) begin
                    out_idx_reg[i][j] <= 0;
                    split_reg[i][j] <= 0;
                end
            end else begin
                for (int j = 0; j < `N; j = j + 1) begin
                    out_idx_reg[i][j] <= out_idx_reg[i - 1][j];
                    split_reg[i][j] <= split_reg[i - 1][j];
                end
            end
        end
    end

    
    genvar i, level;

    // 初始化第 0 层
    generate
        for (i = 0; i < `N; i = i + 1) begin
            assign stage[0][i] = data[i];
        end
    endgenerate

    // 生成加法器逻辑，逐层计算中间的前缀和，并插入寄存器
    generate
        for (level = 0; level < `lgN; level = level + 1) begin : LEVEL_LOOP
            for (i = 0; i < `N; i = i + 1) begin : ADDER_INST
                if (i < (1 << level)) begin
                    // 前面部分保持不变，直接通过寄存器传递
                    DFF stage_reg_dff (
                        .clock(clock),
                        .reset(reset),
                        .d(stage[level][i]),
                        .q(stage[level + 1][i])
                    );
                end else begin
                    // 后面的部分执行累积加法
                    add_ add_inst (
                        .clock(clock),
                        .a(stage[level][i]),
                        .b(stage[level][i - (1 << level)]),
                        .reset(reset),
                        .out(stage[level + 1][i])
                    );
                end
            end
        end
    endgenerate

    // 最后一层的结果即为前缀和
    generate
        for (i = 0; i < `N; i = i + 1) begin
            assign prefix_sum[i] = stage[`lgN][i];
        end
    endgenerate

    // 使用 prefix_sum 计算 out_data
    integer k, j;
    always_ff @(posedge clock or posedge reset) begin
        for (k = 0; k < `N; k = k + 1) begin
            if (reset) begin
                out_data[k].data <= 0;
            end else if (out_idx_reg[`lgN - 1][k] == 0) begin
                // 如果索引为 0，则直接取 prefix_sum[0]
                out_data[k].data <= prefix_sum[0].data;
            end else begin
                for (j = out_idx_reg[`lgN - 1][k] - 1; j >= 0; j = j - 1) begin
                    if (split_reg[`lgN - 1][j] == 1) begin
                        out_data[k].data <= prefix_sum[out_idx_reg[`lgN - 1][k]].data - prefix_sum[j].data;
                        break;
                    end else if (j == 0) begin
                        out_data[k].data <= prefix_sum[out_idx_reg[`lgN - 1][k]].data;
                    end
                end
            end
        end
    end

    // 计算 halo_enable和out_data同步输出
    always @(posedge clock) begin
        if(!reset)begin
        if(split_reg[`lgN - 1][`N - 1] == 1 ) begin
            halo_enable <= 0;
            halo_data <= 0;
        end else begin
            halo_enable <= 1;
            for(j = `N - 1; j>=0; j = j - 1) begin
                if(split_reg[`lgN - 1][j] == 1) begin
                    halo_data <= prefix_sum[`N-1]-prefix_sum[j];
                    break;
                end
            end
        end
        end
        else begin
            halo_enable <= 0;
            halo_data <= 0;
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
    output  int                 num_el,
    output  logic[`N-1:0]       out_valid
);
    // num_el 总是赋值为 N
    assign num_el = `N;
    // delay 你需要自己为其赋值，表示电路的延迟
    assign delay = `lgN + 3 ;

    wire data_t mul_out[`N-1:0];

    reg [`dbLgN-1:0] lhs_ptr_reg[`N-1:0];//lhs_ptr的寄存器

    genvar i;
    generate
        for(i = 0; i < `N; i = i + 1) begin
            mul_ mul_inst (
                .clock(clock),
                .a(lhs_data[i]),
                .b(rhs[lhs_col[i]]),
                .out(mul_out[i])
            );
        end
     endgenerate   

    reg split[`N-1:0];
    reg [`lgN-1:0] out_idx[`N-1:0];
    data_t data[`N-1:0];

    always_ff @( posedge clock ) begin 
        data <= mul_out;
    end

    wire data_t out_data_temp[`N-1:0];
    data_t out_data[`N-1:0];
    assign out = out_data;

    wire halo_enable ;
    wire data_t halo_data ;
    RedUnit red_inst (
        .clock(clock),
        .reset(reset),
        .lhs_start(lhs_start),
        .data(mul_out),
        .split(split),
        .out_idx(out_idx),
        .out_data(out_data_temp),
        .delay(),
        .num_el(),
        .halo_enable(halo_enable),
        .halo_data(halo_data)
    );   
    //这里因为输入的对齐方式和RedUnit有点不一样（有关上升沿采样的问题）split和data差了一个时钟周期输入

    parameter  s_wait = 0,
                s_compute = 1,
                s_output = 2;

    reg [1:0] state = s_wait;
    reg [4:0] counter = 0;

    reg halo_enable_reg1 ;
    data_t halo_data_reg1 ;


    integer k;
    always_ff @( posedge clock or posedge reset ) begin 
        if(reset && ! lhs_start)begin
            state <= s_wait;
            counter <= 0;
        end
        else begin
        case (state)
            s_wait: begin
                if (lhs_start) begin
                    state <= s_compute;
                    counter <= 0;
                    lhs_ptr_reg <= lhs_ptr;

                end
            end
            s_compute: begin
                counter <= counter + 1;
                for (k = 0 ;k < `N ; k = k + 1 ) begin
                    split[k] = 0;
                    out_idx[k] = 0;
                end

                for (k = 0 ; k < `N ; k = k + 1  ) begin
                    if(lhs_ptr_reg[k] / `N ==counter)begin
                        split[lhs_ptr_reg[k] % `N] = 1;
                        out_idx[k] = lhs_ptr_reg[k] % `N;
                    end

                end

                if(out_valid[`N - 1] == 1 )begin
                    state <= s_wait;
                end
            end
        endcase
        end
    end

    //调整0行和halo
    always_ff @( posedge clock or posedge reset ) begin 
        if(reset)begin
            halo_data_reg1 <= 0;
            halo_enable_reg1 <= 0;
        end
        
        else begin
        halo_data_reg1 <= halo_data;
        halo_enable_reg1 <= halo_enable;
        

        for (k = 0 ; k < `N ; k = k + 1 ) begin
            out_data[k] = out_data_temp[k];
        end

        for (k = 1; k < `N ; k = k + 1 ) begin
            if(lhs_ptr_reg[k] == lhs_ptr_reg[k-1] && lhs_ptr_reg[k] / `N == counter - (`lgN + 1))begin
                out_data[k] = 0 ;
            end
        end
                
        if(halo_enable_reg1)begin
            for ( k =  0 ; k < `N ; k = k + 1 ) begin
                    if(lhs_ptr_reg[k] / `N == counter - (`lgN + 1))begin
                        out_data[k] = halo_data_reg1 + out_data_temp[k];
                        break;
                    end
                end
        end

        //标记有效的输出位数
        out_valid = 0;

        for (k = 0; k < `N ; k = k + 1 ) begin
            if(lhs_ptr_reg[k] / `N == counter - (`lgN + 1))begin
                out_valid[k] = 1 ;
            end
        end

        end
    end

endmodule

module SpMM(
    input   logic               clock,
                                reset,
    /* 输入在各种情况下是否 ready */
    output  logic               lhs_ready_ns,
                                lhs_ready_ws,
                                lhs_ready_os,
                                lhs_ready_wos,
    input   wire                lhs_start,
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

    //assign lhs_ready_ns = 1;
    //assign lhs_ready_ws = 1;
    //assign lhs_ready_os = 0;
    assign lhs_ready_wos = 0;
    //assign rhs_ready = 1;
    //assign output_ready = 0;

    data_t rhs[1:0][`N-1:0][`N-1:0];// rhs dbbuffer 0:load_a 1:load_b
    data_t out[1:0][`N-1:0][`N-1:0];// out dbbuffer 0:out_a 1:out_b
    wire data_t rhs_temp[1:0][`N-1:0][`N-1:0]; 
    logic rhs_valid[1:0]; //0:a 1:b
    logic out_valid[1:0]; //0:a 1:b

    logic in_data_choice ;//选择输入的数据 0:load_a 1:load_b
    wire data_t rhs_t[`N-1:0][`N-1:0]; //最终输入PE阵列的数据

    logic load_finish ;//rhs加载完成的脉冲信号
    logic compute_finish ;//计算完成的脉冲信号
    logic out_finish ;//输出完成的脉冲信号

    // 四个操作选择信号
    reg current_load = 0; //0:load_a 1:load_b
    reg current_compute = 0; //0:compute_a 1:compute_b
    reg current_store = 0; //0:store_a 1:store_b
    reg current_out = 0 ; //0:out_a 1:out_b

    wire [`N-1:0] PE_out_vaild[`N-1:0]; // PE输出的有效位数
    wire data_t PE_out_data[`N-1:0][`N-1:0];//PE输出的数据
    logic reset_col;

    reg ws ;
    reg os ;

    always @(posedge lhs_start) begin
        ws <= lhs_ws;
        os <= lhs_os;
    end

    genvar x, y;
    generate
        for(x = 0 ; x < `N ;x = x + 1)begin
            for(y = 0 ; y < `N ; y = y + 1)begin
                assign rhs_temp[0][x][y] = rhs[0][y][x];
                assign rhs_temp[1][x][y] = rhs[1][y][x];
                assign rhs_t[x][y] = in_data_choice ? rhs_temp[1][x][y] : rhs_temp[0][x][y];
            end
        end
    endgenerate

    genvar i;
    generate
        for(i = 0; i < `N; i = i + 1) begin
            PE pe_inst (
                .clock(clock),
                .reset(reset_col),
                .lhs_start(lhs_start),
                .lhs_ptr(lhs_ptr),
                .lhs_col(lhs_col),
                .lhs_data(lhs_data),
                .rhs(rhs_t[i]),
                .out(PE_out_data[i]),
                .delay(),
                .num_el(),
                .out_valid(PE_out_vaild[i])
            );
        end     
    endgenerate    

integer j,k;


//load_rhs 状态机
    parameter load_ready = 0,
                load_in = 1,
                load_wait = 2;

    reg [1:0] load_state = load_wait;
    reg [4:0] load_counter = 0;

    always_ff @( posedge clock ) begin 
        case (load_state)
            load_ready: begin
                if(rhs_start)begin
                    load_state <= load_in;
                    rhs_ready <= 0;
                    for(j = 0 ; j < 4 ; j = j + 1) begin
                        rhs[current_load][j] = rhs_data[j];
                    end
                    load_counter <= 1;
                end
            end

            load_in: begin
                if(load_counter < `N / 4)begin
                    for(j = 0 ; j < 4 ; j = j + 1) begin
                        rhs[current_load][load_counter * 4 + j] = rhs_data[j];
                    end
                    load_counter <= load_counter + 1;
                end
                else begin
                    load_state <= load_wait;
                    load_finish <= 1;
                end               
            end

            load_wait: begin
                load_finish <= 0;
                if(load_finish == 0 && rhs_valid[current_load] == 0)begin
                    rhs_ready <= 1;
                    load_state <= load_ready;
                end
            end

        endcase
    end

//compute 状态机
    parameter compute_ready = 0,
                compute_in = 1,
                compute_wait = 2;

    reg [1:0] compute_state = compute_wait;

    reg [4:0] compute_counter = 0;

    always_ff @( posedge clock ) begin 
        case (compute_state)
            compute_ready: begin
                if(lhs_start)begin
                    reset_col <= 0;
                    compute_state <= compute_in;
                    lhs_ready_ns <= 0;
                    lhs_ready_ws <= 0;
                    lhs_ready_os <= 0;
                end

                if(out_valid[1] == 1 || out_valid[0] == 1)begin
                    lhs_ready_os <= 1;
                end

            end

            compute_in: begin
                for(j = 0 ; j < `N ; j = j + 1) begin
                    for(k = 0 ; k < `N ; k = k + 1) begin
                        if(PE_out_vaild[j][k])begin
                            if(os == 1) begin
                                out[current_store][k][j] = out[current_store][k][j] + PE_out_data[j][k];
                            end
                            else begin
                                out[current_store][k][j] = PE_out_data[j][k];
                            end
                        end
                    end
                end

                if(PE_out_vaild[`N-1][`N-1] == 1)begin
                    compute_state <= compute_wait;
                    compute_finish <= 1;
                    reset_col <= 1;
                end
            end 

            compute_wait: begin
                compute_finish <= 0;
                reset_col <= 1;
                if(compute_finish == 0 && rhs_valid[current_compute] == 1)begin
                    lhs_ready_ns <= 1;
                    lhs_ready_ws <= 1;
                    compute_state <= compute_ready;
                end
            end
        endcase
    end       

//out 状态机
    parameter output_ready = 0,
                out_in = 1,
                out_wait = 2;

    reg [1:0] out_state = out_wait;

    reg [4:0] out_counter = 0;

    always_ff @( posedge clock or posedge out_start or posedge lhs_os ) begin 
        case (out_state)
            output_ready: begin
                if(out_start)begin
                    out_state <= out_in;
                    out_ready <= 0;
                    out_counter <= 1;
                    for(j = 0 ; j < 4 ; j = j + 1) begin
                        out_data[j] <= out[current_out][j];
                    end 
                end

                if(lhs_os == 1 && lhs_start == 1)begin
                    out_state <= out_wait;
                    out_ready <= 0;
                    out_valid[~current_store] <= 0;
                end   

            end

            out_in: begin
                if(out_counter < `N / 4)begin
                    for(j = 0 ; j < 4 ; j = j + 1) begin
                        out_data[out_counter * 4 + j] <= out[current_out][out_counter * 4 + j];
                    end
                    out_counter <= out_counter + 1;
                end
                else begin
                    out_state <= out_wait;
                    out_finish <= 1;
                end
            end 

            out_wait: begin
                out_finish <= 0;
                if(out_finish == 0 && out_valid[current_out] == 1)begin
                    out_ready <= 1;
                    out_state <= output_ready;
                end
                else begin
                    out_ready <= 0;
                end
            end
        endcase
    end     

//三个current和两个valid的更新
    always_ff @( posedge clock ) begin 

        in_data_choice <= current_compute;

        if(load_finish)begin
            current_load <= ~current_load;
        end

        if(compute_finish)begin
            if(ws == 0) begin
            current_compute <= ~current_compute;
            end
            current_store <= ~current_store;
        end

        if(lhs_os == 1 && lhs_start == 1)begin
            current_store <= ~current_store;
            out_valid[~current_store] <= 0;
        end

        if(out_finish)begin
            current_out <= ~current_out;
        end

        for(j = 0 ; j < 2 ; j = j + 1) begin
            if(current_load == j && load_finish)begin
                rhs_valid[j] <= 1;
            end

            if(current_compute == j && compute_finish)begin
                if( ws == 0 ) begin
                    rhs_valid[j] <= 0;
                end
            end

            if(current_store == j && compute_finish)begin
                    out_valid[j] <= 1;
            end

            if(current_out == j && out_finish)begin
                out_valid[j] <= 0;
            end
        end
    end
    
endmodule
