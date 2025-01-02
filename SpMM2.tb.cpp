#include "VSpMM.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>

// #define CHISEL

namespace {

struct Range {
    int start, stop;
    int gen() {
        return rand() % (stop - start) + start;
    }
};

struct LHS {
    bool ws, os;
    int n;
    std::vector<int> ptr;
    std::vector<int> col;
    std::vector<int> data;
    void resize(int n, int c) {
        this->n = n;
        ptr.resize(n);
        col.resize(c);
        data.resize(c);
    }
    void init_full(int n) {
        resize(n, n * n);
        for(int i = 0; i < n; i++) {
            ptr[i] = i * n + n - 1;
            for(int j = 0; j < n; j++) {
                col[i * n + j] = j;
                data[i * n + j] = i * n + j;
            }
        }
    }
    void init_half(int n) {
        resize(n, n * n / 2);
        for(int i = 0; i < n; i++) {
            ptr[i] = i * (n / 2) + (n / 2) - 1;
            for(int j = 0; j < n / 2; j++) {
                col[i * (n / 2) + j] = j;
                data[i * (n / 2) + j] = i;
            }
        }
    }
    void init_eye(int n) {
        resize(n, n);
        for(int i = 0; i < n; i++) {
            ptr[i] = i;
            col[i] = i;
            data[i] = 1;
        }
    }
    void init_linesep(int n) {
        resize(n, n  / 2 * n);
        for(int i = 0; i < n; i += 2) {
            int sep = rand() % n;
            ptr[i] = i / 2 * n + sep;
            ptr[i + 1] = i / 2 * n + n - 1;
            for(int j = 0; j < n; j++) {
                col[i / 2 * n + j] = j;
                data[i / 2 * n + j] = i * n + j;
            }
        }
    }
    void init_empty(int n) {
        resize(n, 1);
        for(int i = 0; i < n; i++) {
            ptr[i] = 0;
        }
        col[0] = 2;
        data[0] = 2;
    }
    void init_rand(int n, Range line_cnt) {
        std::vector<int> cnt(n);
        for(int i = 0; i < n; i++) {
            cnt[i] = line_cnt.gen();
        }
        cnt[0] = std::max(cnt[0], 1);
        std::vector<int> psum(n);
        std::partial_sum(cnt.begin(), cnt.end(), psum.begin());
        resize(n, psum[n - 1]);
        for(int i = 0; i < n; i++) {
            ptr[i] = psum[i] - 1;
            int buf[n];
            for(int j = 0; j < n; j++) {
                buf[j] = j;
                int p = rand()  % (j+1);
                std::swap(buf[p], buf[j]);
            }
            for(int j = 0; j < cnt[i]; j++) {
                int p = psum[i] - cnt[i] + j;
                col[p] = buf[j];
                data[p] = rand() % 10;
            }
        }
    }
    template<typename ... Args>
    static LHS new_with(bool ws, bool os, void (LHS::*func)(Args...), Args ... args) {
        LHS res;
        res.ws = ws;
        res.os = os;
        (res.*func)(args...);
        return res;
    }
};

static std::vector<int> gen_rhs(int n, Range rg) {
    std::vector<int> res(n * n);
    for(int i = 0; i < n * n; i++) {
        res[i] = rg.gen();
    }
    return res;
}

struct DUT: VSpMM {
protected:
    VerilatedVcdC* tfp = nullptr;
    uint64_t sim_clock = 0;
public: 
    static VerilatedContext * new_context() {
        auto ctx = new VerilatedContext;
        ctx->threads(1);
        ctx->traceEverOn(true);
        return ctx;
    }
    DUT(): VSpMM(new_context()) {}
    ~DUT() override {
        if(tfp) tfp->close();
        delete tfp;
    }
    void open_vcd(const char * file) {
        tfp = new VerilatedVcdC;
        this->trace(tfp, 99);
        tfp->open(file);
    }
    int n = -1;
    int timeout = -1;
    int random_sleep = 5;
#ifdef CHISEL
    uint8_t * lhs_ptr = (uint8_t*)&lhs_ptr_0;
    uint8_t * lhs_col = (uint8_t*)&lhs_col_0;
    uint8_t * lhs_data = (uint8_t*)&lhs_data_0;
    uint8_t * rhs_data_ = (uint8_t*)&rhs_data_0_0;
    uint8_t * out_data_ = (uint8_t*)&out_data_0_0;
#endif
    void init() {
        this->reset = 1;
        this->step(1);
        this->reset = 0;
        n = this->num_el;
    }
    void step(int num_clocks=1) {
        for(int i = 0; i < num_clocks; i++) {
            tick_lhs();
            tick_rhs();
            this->clock = 0;
            this->eval();
            if(this->tfp) {
                tfp->dump(sim_clock);
                sim_clock++;
            }
            this->clock = 1;
            this->eval();
            if(this->tfp) {
                tfp->dump(sim_clock);
                sim_clock++;
            }
            if(sim_clock / 2 >= timeout) {
                throw std::runtime_error("timeout");
            }
        }
    }
    LHS cur_lhs;
    int send_lhs_tick = -1;
    void tick_lhs(bool comb=false) {
        lhs_start = send_lhs_tick == 0;
        if(send_lhs_tick == -1) return;
        if(send_lhs_tick == 0) {
            for(int i = 0; i < n; i++) {
                lhs_ptr[i] = cur_lhs.ptr[i];
            }
            lhs_ws = cur_lhs.ws;
            lhs_os = cur_lhs.os;
        }
        for(int i = 0; i < n; i++) {
            int p = send_lhs_tick * n + i;
            if(p < cur_lhs.col.size()) {
                lhs_col[i] = cur_lhs.col[p];
                lhs_data[i] = cur_lhs.data[p];
            }
        }
        if(!comb) {
            if(cur_lhs.ptr[n - 1] <= send_lhs_tick * n) {
                send_lhs_tick = -1;
            } else {
                send_lhs_tick ++;
            }
        }
    }
    void send_lhs(LHS lhs) {
        int sleep = rand() % random_sleep;
        while(sleep--) step();
        bool ws = lhs.ws, os = lhs.os;
        if(!ws && !os) {
            while(!lhs_ready_ns) step();
        }
        else if(ws && !os) {
            while(!lhs_ready_ws) step();
        }
        else if(!ws && os) {
            while(!lhs_ready_os) step();
        }
        else if (ws && os) {
            while(!lhs_ready_wos) step();
        }
        cur_lhs = lhs;
        send_lhs_tick = 0;
        tick_lhs(true);
        this->eval();
    }
    std::vector<int> cur_rhs;
    int send_rhs_tick = -1;
    void tick_rhs(bool comb=false) {
#ifdef CHISEL
        uint8_t (*rhs_data)[n];
        *(uint8_t**)(&rhs_data) = rhs_data_;
#endif
        rhs_start = send_rhs_tick == 0;
        if(send_rhs_tick == -1) return;
        for(int i = 0; i < 4 * n; i++) {
            int p = send_rhs_tick * 4 * n + i;
            rhs_data[i / n][i % n] = cur_rhs[p];
        }
        if(!comb) {
            send_rhs_tick++;
            if(send_rhs_tick == n / 4) {
                send_rhs_tick = -1;
            }
        }
    }
    void send_rhs(std::vector<int> rhs) {
        int sleep = rand() % random_sleep;
        while(sleep--) step();
        while(!rhs_ready) step();
        cur_rhs = rhs;
        send_rhs_tick = 0;
        tick_rhs(true);
        this->eval();
    }
    void receive_out(std::vector<int> & out) {
#ifdef CHISEL
        uint8_t (*out_data)[n];
        *(uint8_t**)(&out_data) = out_data_;
#endif
        out.resize(n * n);
        int sleep = rand() % random_sleep;
        while(sleep--) step();
        while(!out_ready) step();
        out_start = 1;
        this->eval();
        for(int i = 0; i < n / 4; i++) {
            for(int j = 0; j < 4 * n; j++) {
                out[i * 4 * n + j] = out_data[j / n][j % n];
            }
            step();
            out_start = 0;
        }
        out_start = 0;
    }
};

static void generate_gtkw_file(const char * out, int num_el) {
    std::ofstream fout(out);
    fout << "[timestart] 0\n";
    fout << "[color] 0\nTOP.clock\n";
    fout << "TOP.lhs_ready_ns\n";
    fout << "TOP.lhs_ready_ws\n";
    fout << "TOP.lhs_ready_os\n";
    fout << "TOP.lhs_ready_wos\n";
    fout << "TOP.lhs_start\n";
    fout << "TOP.lhs_os\n";
    fout << "TOP.lhs_ws\n";
    fout << "TOP.rhs_ready\n";
    fout << "TOP.rhs_start\n";
    fout << "TOP.out_ready\n";
    fout << "TOP.out_start\n";
    fout.close();
}

using gen_lhs_func = std::function<LHS(bool, bool)>;

struct Test {
    int n;
    std::unique_ptr<DUT> dut;
    Test(): dut(std::move(std::make_unique<DUT>())) {}
    virtual ~Test() = default;
    virtual std::string name() = 0;
    virtual bool run() = 0;
    virtual int gen_num_lhs_gen() = 0;
    virtual gen_lhs_func* get_lhs_gen() = 0;
    bool verify(std::vector<LHS> lhs, std::vector<std::vector<int>> rhs, std::vector<int> res) {
        std::vector<int> gold(n * n);
        for(int i = 0; i < n; i++) {
            for(int j = 0; j < n; j++) {
                int sum = 0;
                for(int p = 0; p < lhs.size(); p++) {
                    for(int k = i ? lhs[p].ptr[i - 1] + 1 : 0; k <= lhs[p].ptr[i]; k++) {
                        sum += lhs[p].data[k] * rhs[p][lhs[p].col[k] * n + j];
                    }
                }
                gold[i * n + j] = sum % 256;
            }
        }
        bool ok = true;
        for(int i = 0; i < n * n; i++) {
            ok &= gold[i] == res[i];
        }
        if(!ok) {
            std::cout << "ERROR: \n";
            for(int p = 0; p < lhs.size(); p++) {
                std::cout << "group " << p << ":\n";
                for(int i = 0; i < n; i++) {
                    std::vector<int> lhs_row(n);
                    std::vector<bool> lhs_row_vld(n, false);
                    for(int k = i ? lhs[p].ptr[i - 1] + 1 : 0; k <= lhs[p].ptr[i]; k++) {
                        lhs_row[lhs[p].col[k]] = lhs[p].data[k];
                        lhs_row_vld[lhs[p].col[k]] = 1;
                    }
                    for(int j = 0; j < n; j++) {
                        if(lhs_row_vld[j]) {
                            std::cout << std::setw(4) << lhs_row[j];
                        } else {
                            std::cout << std::setw(4) << "";
                        }
                    }
                    std::cout << "  |  ";
                    for(int j = 0; j < n; j++) {
                        std::cout << std::setw(4) << rhs[p][i * n + j];
                    }
                    std::cout << "\n";
                }
            }
            std::cout << "Got: ";
            for(int j = 0; j < n; j++) {
                std::cout << std::setw(4) << "";
            }
            std::cout << "Expected:\n";
            for(int i = 0; i < n; i++) {
                for(int j = 0; j < n; j++) {
                    std::cout << std::setw(4) << res[i * n + j];
                }
                std::cout << "  |  ";
                for(int j = 0; j < n; j++) {
                    std::cout << std::setw(4) << gold[i * n + j];
                }
                std::cout << "\n";
            }
        }
        return ok;
    }

    bool start(const char * vcd_file) {
        std::cout << "START: " << name() << "\n";
        dut->open_vcd(vcd_file);
        dut->init();
        n = dut->num_el;
        try {
            bool res = run();
            std::cout << "FINISH: " << name() << "\n" << "\n";
            return res;
        } catch(std::runtime_error & err) {
            std::cout << "TIMEOUT\n";
            std::cout << "FINISH: " << name() << "\n" << "\n";
            return false;
        }
    }
};


static std::vector<gen_lhs_func> lhs_no_halo(int num_el) {
    return {
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_full, num_el);},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_half, num_el);},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_eye, num_el);},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_empty, num_el);},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_linesep, num_el);},
    };
}

static std::vector<gen_lhs_func> lhs_halo(int num_el) {
    int Q0 = 0, Q1 = num_el / 4, Q2 = num_el / 2, Q3 = num_el * 3 / 4, Q4 = num_el;
    return {
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q1});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q1, Q2});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q2, Q3});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q3, Q4});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q2});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q1, Q3});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q2, Q4});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q4});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q4});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q4});},
        [=](bool ws, bool os){return LHS::new_with(ws, os, &LHS::init_rand, num_el, Range{Q0, Q4});},
    };
}

struct NsOnepass: public Test {
    using Test::Test;
    gen_lhs_func gen[1];
    int gen_num_lhs_gen() override {return 1;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "ns-onepass";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs = gen[0](false, false);
        auto rhs = gen_rhs(dut->n, {1, 2});
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs); dut->step();
        std::vector<int> out;
        dut->receive_out(out);
        return verify({lhs}, {rhs}, out);
    }
};

struct RhsDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[1];
    int gen_num_lhs_gen() override {return 1;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "rhs-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs = gen[0](false, false);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {2, 3});
        dut->send_rhs(rhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs); dut->step();
        std::vector<int> out1;
        dut->receive_out(out1); dut->step();
        dut->send_lhs(lhs); dut->step();
        std::vector<int> out2;
        dut->receive_out(out2); dut->step();
        return verify({lhs}, {rhs1}, out1) & verify({lhs}, {rhs2}, out2);
    }
};

struct OutDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[1];
    int gen_num_lhs_gen() override {return 1;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs = gen[0](false, false);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {2, 3});
        dut->send_rhs(rhs1); dut->step();
        dut->send_lhs(lhs); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs); dut->step();
        std::vector<int> out1;
        dut->receive_out(out1); dut->step();
        std::vector<int> out2;
        dut->receive_out(out2); dut->step();
        return verify({lhs}, {rhs1}, out1) && verify({lhs}, {rhs2}, out2);
    }
};

struct RhsOutDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "rhs-out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs1 = gen[0](false, false);
        LHS lhs2 = gen[1](false, false);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {2, 3});
        dut->send_rhs(rhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        std::vector<int> out1;
        std::vector<int> out2;
        dut->receive_out(out1); dut->step();
        dut->receive_out(out2); dut->step();
        return verify({lhs1}, {rhs1}, out1) && verify({lhs2}, {rhs2}, out2);
    }
};

struct WSOnePass: public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "ws-one-pass";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs1 = gen[0](true, false);
        LHS lhs2 = gen[1](true, false);
        auto rhs = gen_rhs(dut->n, {1, 2});
        std::vector<int> out1;
        std::vector<int> out2;
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->receive_out(out1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out2); dut->step();
        return verify({lhs1}, {rhs}, out1) & verify({lhs2}, {rhs}, out2);
    }
};

struct WSOutDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "ws-out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs1 = gen[0](true, false);
        LHS lhs2 = gen[1](true, false);
        auto rhs = gen_rhs(dut->n, {1, 2});
        std::vector<int> out1;
        std::vector<int> out2;
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out1); dut->step();
        dut->receive_out(out2); dut->step();
        return verify({lhs1}, {rhs}, out1) && verify({lhs2}, {rhs}, out2);
    }
};

struct WSPipe: public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "ws-pipe";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs[2];
        for(int i = 0; i < 2; i++) {
            lhs[i] = gen[i](i + 1 < 2, false);
        }
        std::vector<int> rhs[2];
        for(int i = 0; i < 2; i++) {
            rhs[i] = gen_rhs(n, {i+1, i+2});
        }
        std::vector<int> out[2][2];
        dut->send_rhs(rhs[0]);
        dut->step();
        dut->send_rhs(rhs[1]);
        dut->step();
        for(int j = 0; j < 2; j++) {
            for(int i = 0; i < 2; i++) {
                dut->send_lhs(lhs[i]);
                dut->step();
            }
            for(int i = 0; i < 2; i++) {
                dut->receive_out(out[i][j]);
            }
        }
        bool result = true;
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                result &= verify({lhs[i]}, {rhs[j]}, out[i][j]);
            }
        }
        return result;
    }
};

struct OSOnePass : public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "os-onepass";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs1 = gen[0](false, false);
        LHS lhs2 = gen[1](false, true);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {1, 2});
        std::vector<int> out;
        dut->send_rhs(rhs1); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out); dut->step();
        return verify({lhs1, lhs2}, {rhs1, rhs2}, out);
    }
};

struct OSRhsDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[2];
    int gen_num_lhs_gen() override {return 2;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "os-rhs-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 1000;
        LHS lhs1 = gen[0](false, false);
        LHS lhs2 = gen[1](false, true);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {1, 2});
        std::vector<int> out;
        dut->send_rhs(rhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out); dut->step();
        return verify({lhs1, lhs2}, {rhs1, rhs2}, out);
    }
};

struct OSPipe: public Test {
    using Test::Test;
    gen_lhs_func gen[4];
    int gen_num_lhs_gen() override {return 4;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "os-pipe";
    }
    bool run() override {
        dut->timeout = n * n * 1000;
        LHS lhs[2][2];
        std::vector<int> rhs[2][2];
        std::vector<int> out[2][2];
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                lhs[i][j] = gen[i*2+j](false, j != 0);
            }
        }
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                rhs[i][j] = gen_rhs(n, {i * 2 + j + 1, i * 2 + j + 2});
            }
        }
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                for(int k = 0; k < 2; k++) {
                    dut->send_rhs(rhs[k][j]);
                    dut->send_lhs(lhs[i][k]);
                    dut->step();
                }
                dut->receive_out(out[i][j]);
            }
        }
        bool ok = true;
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                ok &= verify({lhs[i][0], lhs[i][1]}, {rhs[0][j], rhs[1][j]}, out[i][j]);
            }
        }
        return ok;
    }
};

struct WOSOnePass: public Test {
    using Test::Test;
    gen_lhs_func gen[4];
    int gen_num_lhs_gen() override {return 4;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "wos-pipe";
    }
    bool run() override {
        dut->timeout = n * 4 * 1000;
        LHS lhs1 = gen[0](true, false);
        LHS lhs2 = gen[1](true, true);
        LHS lhs3 = gen[2](true, true);
        LHS lhs4 = gen[3](true, true);
        std::vector<int> rhs = gen_rhs(n, {1, 2});
        dut->send_rhs(rhs);
        dut->send_lhs(lhs1);
        dut->step();
        dut->send_lhs(lhs2);
        dut->step();
        dut->send_lhs(lhs3);
        dut->step();
        dut->send_lhs(lhs4);
        dut->step();
        std::vector<int> out;
        dut->receive_out(out);
        return verify({lhs1, lhs2, lhs3, lhs4}, {rhs, rhs, rhs, rhs}, out);
    }
};

struct WOSDbBuf: public Test {
    using Test::Test;
    gen_lhs_func gen[4];
    int gen_num_lhs_gen() override {return 4;};
    gen_lhs_func* get_lhs_gen() override {return gen;};
    std::string name() override {
        return "wos-pipe";
    }
    bool run() override {
        dut->timeout = n * 4 * 1000;
        LHS lhs1 = gen[0](true, false);
        LHS lhs2 = gen[1](false, true);
        LHS lhs3 = gen[2](true, false);
        LHS lhs4 = gen[3](false, true);
        std::vector<int> rhs1 = gen_rhs(n, {1, 2});
        std::vector<int> rhs2 = gen_rhs(n, {1, 2});
        dut->send_rhs(rhs1);
        dut->step();
        dut->send_rhs(rhs2);
        dut->step();
        dut->send_lhs(lhs1);
        dut->step();
        dut->send_lhs(lhs2);
        dut->step();
        std::vector<int> out1;
        dut->receive_out(out1);
        dut->step();
        dut->send_lhs(lhs3);
        dut->step();
        dut->send_lhs(lhs4);
        dut->step();
        std::vector<int> out2;
        dut->receive_out(out2);
        return verify({lhs1, lhs2}, {rhs1, rhs1}, out1) && verify({lhs3, lhs4}, {rhs2, rhs2}, out2);
    }
};

using test_gen_func = std::function<Test*()>;

struct TestInfo {
    test_gen_func gen;
    bool dbbuf, ws, os;
};

static const std::vector<TestInfo> testInfo {
    {[](){return new NsOnepass;}, 
        false, false, false},
    {[](){return new RhsDbBuf;}, 
        true, false, false},
    {[](){return new OutDbBuf;}, 
        true, false, false},
    {[](){return new RhsOutDbBuf;}, 
        true, false, false},
    {[](){return new WSOnePass;}, 
        false, true, false},
    {[](){return new WSOutDbBuf;}, 
        true, true, false},
    {[](){return new WSPipe;}, 
        true, true, false},
    {[](){return new OSOnePass;}, 
        false, false, true},
    {[](){return new OSPipe;}, 
        true, false, true},
    {[](){return new WOSOnePass;}, 
        false, true, true},
    {[](){return new WOSDbBuf;},
        true, true, true}
};

struct MetaTest {
    std::unique_ptr<Test> test;
    bool dbbuf, ws, os, halo;
    std::string name() {
        std::stringstream ss;
        ss << "test";
        if(dbbuf) ss << "+dbbuf";
        if(ws) ss << "+ws";
        if(os) ss << "+os";
        if(halo) ss << "+halo";
        return ss.str();
    }
};

} // namespace

#ifndef SCORE_PREFIX
#define SCORE_PREFIX "score/"
#endif

int main(int argc, char ** argv) {
    auto dut = std::make_unique<DUT>();
    dut->init();
    int num_el = dut->num_el;
    generate_gtkw_file("trace/SpMM2/wave.gtkw", num_el);
    std::vector<MetaTest> tests;
    auto gen_no_halo = lhs_no_halo(num_el);
    auto gen_halo = lhs_halo(num_el); 
    auto choose_lhs_gen = [&](bool halo){
        if(halo) return gen_halo[rand() % gen_halo.size()];
        else return gen_no_halo[rand() % gen_no_halo.size()];
    };
    for(auto info: testInfo) {
        for(auto halo: {false, true}) {
            for(int t = 0; t < 20; t++) {
                auto test = info.gen();
                auto cnt = test->gen_num_lhs_gen();
                auto lhs_gen = test->get_lhs_gen();
                for(int i = 0; i < cnt; i++) {
                    lhs_gen[i] = choose_lhs_gen(halo);
                }
                tests.push_back((MetaTest) {
                    .test=std::move(std::unique_ptr<Test>(test)),
                    .dbbuf=info.dbbuf,
                    .ws=info.ws,
                    .os=info.os,
                    .halo=halo
                });
            }
        }
    }
    int idx = 0;
    std::ofstream out(SCORE_PREFIX "SpMM2.tb.out");
    for(auto & t: tests) {
        idx++;
        std::stringstream ss;
        ss << "trace/SpMM2/";
        ss << std::setw(3) << std::setfill('0') << idx << "-" << t.name();
        std::cout << ss.str() << std::endl;
        bool success = t.test->start((ss.str() + ".vcd").c_str());
        out << (t.halo * 1 + t.dbbuf * 2 + t.ws * 4 + t.os * 8) << " " << success << std::endl;
    }
    out.close();
    return 0;
}
