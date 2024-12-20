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

std::vector<int> gen_rhs(int n, Range rg) {
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
        ctx->traceEverOn(true);
        return ctx;
    }
    DUT(): VSpMM(new_context()) {}
    ~DUT() {
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
    // uint8_t * lhs_ptr = (uint8_t*)&lhs_ptr_0;
    // uint8_t * lhs_col = (uint8_t*)&lhs_col_0;
    // uint8_t * lhs_data = (uint8_t*)&lhs_data_0;
    // uint8_t * rhs_data = (uint8_t*)&rhs_data_0_0;
    // uint8_t * out_data = (uint8_t*)&out_data_0_0;
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
        while(!rhs_ready) step();
        cur_rhs = rhs;
        send_rhs_tick = 0;
        tick_rhs(true);
        this->eval();
    }
    void receive_out(std::vector<int> & out) {
        out.resize(n * n);
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

struct Test {
    int n;
    std::unique_ptr<DUT> dut;
    Test(): dut(std::move(std::make_unique<DUT>())) {}
    virtual ~Test() = default;
    virtual std::string name() = 0;
    virtual bool run() = 0;
    void verify(std::vector<LHS> lhs, std::vector<std::vector<int>> rhs, std::vector<int> res) {
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

struct NsOnepass: public Test {
    using Test::Test;
    std::string name() override {
        return "ns-onepass";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs = LHS::new_with(false, false, &LHS::init_full, dut->n);
        auto rhs = gen_rhs(dut->n, {1, 2});
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs); dut->step();
        std::vector<int> out;
        dut->receive_out(out);
        verify({lhs}, {rhs}, out);
        return true;
    }
};

struct RhsDbBuf: public Test {
    using Test::Test;
    std::string name() override {
        return "rhs-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs = LHS::new_with(false, false, &LHS::init_full, dut->n);
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
        verify({lhs}, {rhs1}, out1);
        verify({lhs}, {rhs2}, out2);
        return true;
    }
};

struct OutDbBuf: public Test {
    using Test::Test;
    std::string name() override {
        return "out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs = LHS::new_with(false, false, &LHS::init_full, dut->n);
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
        verify({lhs}, {rhs1}, out1);
        verify({lhs}, {rhs2}, out2);
        return true;
    }
};

struct RhsOutDbBuf: public Test {
    using Test::Test;
    std::string name() override {
        return "rhs-out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs1 = LHS::new_with(false, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(false, false, &LHS::init_half, dut->n);
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
        verify({lhs1}, {rhs1}, out1);
        verify({lhs2}, {rhs2}, out2);
        return true;
    }
};

struct WSOnePass: public Test {
    using Test::Test;
    std::string name() override {
        return "ws-one-pass";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs1 = LHS::new_with(true, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(true, false, &LHS::init_half, dut->n);
        auto rhs = gen_rhs(dut->n, {1, 2});
        std::vector<int> out1;
        std::vector<int> out2;
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->receive_out(out1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out2); dut->step();
        verify({lhs1}, {rhs}, out1);
        verify({lhs2}, {rhs}, out2);
        return true;
    }
};

struct WSOutDbBuf: public Test {
    using Test::Test;
    std::string name() override {
        return "ws-out-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs1 = LHS::new_with(true, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(true, false, &LHS::init_half, dut->n);
        auto rhs = gen_rhs(dut->n, {1, 2});
        std::vector<int> out1;
        std::vector<int> out2;
        dut->send_rhs(rhs); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out1); dut->step();
        dut->receive_out(out2); dut->step();
        verify({lhs1}, {rhs}, out1);
        verify({lhs2}, {rhs}, out2);
        return true;
    }
};

struct WSPipe: public Test {
    using Test::Test;
    std::string name() override {
        return "ws-pipe";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs[2];
        for(int i = 0; i < 2; i++) {
            lhs[i] = LHS::new_with(i + 1 < 2, false, &LHS::init_full, dut->n);
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
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                verify({lhs[i]}, {rhs[j]}, out[i][j]);
            }
        }
        return true;
    }
};

struct OSOnePass : public Test {
    using Test::Test;
    std::string name() override {
        return "os-onepass";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs1 = LHS::new_with(false, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(false, true, &LHS::init_half, dut->n);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {1, 2});
        std::vector<int> out;
        dut->send_rhs(rhs1); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out); dut->step();
        verify({lhs1, lhs2}, {rhs1, rhs2}, out);
        return true;
    }
};

struct OSRhsDbBuf: public Test {
    using Test::Test;
    std::string name() override {
        return "os-rhs-dbbuf";
    }
    bool run() override {
        dut->timeout = n * 10;
        LHS lhs1 = LHS::new_with(false, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(false, true, &LHS::init_half, dut->n);
        auto rhs1 = gen_rhs(dut->n, {1, 2});
        auto rhs2 = gen_rhs(dut->n, {1, 2});
        std::vector<int> out;
        dut->send_rhs(rhs1); dut->step();
        dut->send_rhs(rhs2); dut->step();
        dut->send_lhs(lhs1); dut->step();
        dut->send_lhs(lhs2); dut->step();
        dut->receive_out(out); dut->step();
        verify({lhs1, lhs2}, {rhs1, rhs2}, out);
        return true;
    }
};

struct OSPipe: public Test {
    using Test::Test;
    std::string name() override {
        return "os-pipe";
    }
    bool run() override {
        dut->timeout = n * n * 10;
        LHS lhs[2][2];
        std::vector<int> rhs[2][2];
        std::vector<int> out[2][2];
        for(int i = 0; i < 2; i++) {
            for(int j = 0; j < 2; j++) {
                lhs[i][j] = LHS::new_with(false, j != 0, &LHS::init_full, dut->n);
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
        return true;
    }
};

struct WOSOnePass: public Test {
    using Test::Test;
    std::string name() override {
        return "wos-pipe";
    }
    bool run() override {
        dut->timeout = n * 4 * 10;
        LHS lhs1 = LHS::new_with(true, false, &LHS::init_full, dut->n);
        LHS lhs2 = LHS::new_with(true, true, &LHS::init_full, dut->n);
        LHS lhs3 = LHS::new_with(true, true, &LHS::init_full, dut->n);
        LHS lhs4 = LHS::new_with(true, true, &LHS::init_full, dut->n);
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
        verify({lhs1, lhs2, lhs3, lhs4}, {rhs, rhs, rhs, rhs}, out);
        return false;
    }
};

} // namespace

int main() {
    auto dut = std::make_unique<DUT>();
    dut->init();
    int num_el = dut->num_el;
    generate_gtkw_file("trace/SpMM/wave.gtkw", num_el);
    std::vector<Test*> tests {
        new NsOnepass(),
        // new RhsDbBuf(),
        // new OutDbBuf(),
        // new RhsOutDbBuf(),
        // new WSOnePass(),
        // new WSOutDbBuf(),
        // new WSPipe(),
        // new OSOnePass(),
        // new OSPipe(),
        // new WOSOnePass(),
    };
    int idx = 0;
    for(auto t: tests) {
        idx++;
        std::stringstream ss;
        ss << "trace/SpMM/";
        ss << std::setw(2) << std::setfill('0') << idx << "-" << t->name();
        t->start((ss.str() + ".vcd").c_str());
    }
    for(auto t: tests) {
        delete t;
    }
    return 0;
}
