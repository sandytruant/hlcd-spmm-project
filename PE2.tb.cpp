#include "VPE.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <cmath>
#include <iomanip>
#include <memory>
#include <iostream>
#include <fstream>
#include <numeric>

struct DUT: public VPE {
protected:
    VerilatedVcdC * tfp = nullptr;
    uint64_t sim_clock = 0;
public:
    using VPE::VPE;
    ~DUT() {
        if(tfp) tfp->close();
        delete tfp;
    }
    void open_vcd(const char * file) {
        tfp = new VerilatedVcdC;
        this->trace(tfp, 99);
        tfp->open(file);
    }
    void init() {
        this->reset = 1;
        this->step(1);
        this->reset = 0;
    }
    void step(int num_clocks=1) {
        for(int i = 0; i < num_clocks; i++) {
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
        }
    }
    // uint8_t * lhs_ptr = (uint8_t*)&lhs_ptr_0;
    // uint8_t * lhs_col = (uint8_t*)&lhs_col_0;
    // uint8_t * lhs_data = (uint8_t*)&lhs_data_0;
    // uint8_t * rhs = (uint8_t*)&rhs_0;
    // uint8_t * out = (uint8_t*)&out_0;
};

struct Range {
    int start, stop;
    int gen() {
        return rand() % (stop - start) + start;
    }
};

struct Data {
    int n;
    int input_cycles;
    std::vector<int> ptr;
    std::vector<int> col;
    std::vector<int> data;
    std::vector<int> rhs;
    std::vector<int> res;
    void make_res() {
        for(int i = 0; i < n; i++) {
            int sum = 0;
            for(int p = i ? ptr[i - 1] + 1 : 0; p <= ptr[i]; p++) {
                sum = (sum + data[p] * rhs[col[p]]) % 256;
            }
            res[i] = sum % 256;
        }
        input_cycles = (col.size() + n - 1) / n;
    }
    int feed_iter = 0;
    void feed_next(DUT * dut) {
        if(feed_iter == 0) {
            for(int i = 0; i < n; i++) {
                dut->lhs_ptr[i] = ptr[i];
            }
            // dut->lhs_start = 1;
        }
        // else dut->lhs_start = 0;
        for(int i = 0; i < n; i++) {
            dut->rhs[i] = rhs[i];
        }
        for(int i = 0; i < n; i++) {
            int p = feed_iter * n + i;
            if(p < col.size()) {
                dut->lhs_col[i] = col[p];
                dut->lhs_data[i] = data[p];
            }
        }
        feed_iter++;
    }
    std::vector<int> dut_out;
    bool ok = true;
    int cmp_iter = 0;
    void compare_next(DUT * dut) {
        for(int i = 0; i < n; i++) {
            if(cmp_iter * n <= ptr[i] && ptr[i] < cmp_iter * n + n) {
                dut_out[i] = dut->out[i];
                if(dut_out[i] != res[i]) {
                    ok = false;
                }
            }
        }
        cmp_iter++;
    }
    void report_error() {
        for(int i = 0; i < n; i++) {
            std::cout         << "MAT[" << std::setw(2) << i << "]: ";
            std::vector<int> line(n, 0);
            std::vector<bool> vld(n, false);
            for(int j = i ? ptr[i - 1] + 1 : 0; j <= ptr[i]; j++) {
                line[col[j]] = data[j];
                vld[col[j]] = true;
            }
            for(int j = 0; j < n; j++) {
                if(vld[j]) {
                    std::cout << std::setw(4) << line[j] << " ";
                } else {
                    std::cout << "    " << " ";
                }
            }
            std::cout << "   |   ";
            std::cout << rhs[i] << "\n";
        }
        std::cout         << "got prod:";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4) << dut_out[i] << " ";
        }
        std::cout << "\n" << "expect:  ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4) << res[i] << " ";
        }
        std::cout << "\n" << "\n";
    }
    void resize(int n, int c) {
        this->n = n;
        ptr.resize(n);
        col.resize(c);
        data.resize(c);
        rhs.resize(n);
        res.resize(n);
        dut_out.resize(n);
    }
    void init_full(int n) {
        resize(n, n * n);
        for(int i = 0; i < n; i++) {
            ptr[i] = i * n + n - 1;
            for(int j = 0; j < n; j++) {
                col[i * n + j] = j;
                data[i * n + j] = i * n + j;
            }
            rhs[i] = 1;
        }
        make_res();
    }
    void init_half(int n) {
        resize(n, n * n / 2);
        for(int i = 0; i < n; i++) {
            ptr[i] = i * (n / 2) + (n / 2) - 1;
            for(int j = 0; j < n / 2; j++) {
                col[i * (n / 2) + j] = j;
                data[i * (n / 2) + j] = i;
            }
            rhs[i] = 1;
        }
        make_res();
    }
    void init_eye(int n) {
        resize(n, n);
        for(int i = 0; i < n; i++) {
            ptr[i] = i;
            col[i] = i;
            data[i] = 1;
            rhs[i] = rand() % 10;
        }
        make_res();
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
        for(int i = 0; i < n; i++) {
            rhs[i] = 1;
        }
        make_res();
    }
    void init_empty(int n) {
        resize(n, 1);
        for(int i = 0; i < n; i++) {
            ptr[i] = 0;
            rhs[i] = i;
        }
        col[0] = 2;
        data[0] = 2;
        make_res();
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
            rhs[i] = 1;
            int buf[n];
            for(int j = 0; j < n; j++) {
                buf[j] = j;
                int p = rand() % (j+1);
                std::swap(buf[p], buf[j]);
            }
            for(int j = 0; j < cnt[i]; j++) {
                int p = psum[i] - cnt[i] + j;
                col[p] = buf[j];
                data[p] = rand() % 10;
            }
        }
        make_res();
    }
    template<typename ... Args>
    static Data new_with(void (Data::*func)(Args...), Args ... args) {
        Data res;
        (res.*func)(args...);
        return res;
    }
};

static bool test_it(const char * vcd_file, Data data) {
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->threads(1);
    contextp->traceEverOn(true);
    auto dut = std::make_unique<DUT>(&*contextp);
    dut->init();
    dut->open_vcd(vcd_file);
    auto delay = dut->delay;
    auto num_el = dut->num_el;
    for(int i = 0; i < data.input_cycles + delay; i++) {
        if(i == 0)
            dut->lhs_start = 1;
        else
            dut->lhs_start = 0;
        if(i < data.input_cycles) {
            data.feed_next(&*dut);
        }
        if(i >= delay) {
            data.compare_next(&*dut);
        }
        dut->step();
    }
    if(!data.ok) {
        std::cout << "Error in " << vcd_file << "\n";
        data.report_error();
    }
    return data.ok;
}

static void generate_gtkw_file(const char * out, int num_el) {
    std::ofstream fout(out);
    fout << "[timestart] 0\n";
    fout << "[color] 3\nTOP.clock\n";
    fout << "@24\n";
    int ptr_width = 2 * ceil(log2(num_el));
    int col_width = ceil(log2(num_el));
    for(int i = 0; i < num_el; i++) fout << "[color] 0\nTOP.lhs_ptr[" << i << "][" << ptr_width-1 << ":0]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 1\nTOP.lhs_col[" << i << "][" << col_width-1 << ":0]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 2\nTOP.lhs_data[" << i << "][7:0]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 3\nTOP.rhs[" << i << "][7:0]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 0\nTOP.out[" << i << "][7:0]\n";
    fout.close();
}

using  gen_data_func = std::function<Data()>;

static std::vector<gen_data_func> gen_data_no_halo(int num_el) {
    return {
        [=](){return Data::new_with(&Data::init_full, num_el);},
        [=](){return Data::new_with(&Data::init_half, num_el);},
        [=](){return Data::new_with(&Data::init_eye, num_el);},
        [=](){return Data::new_with(&Data::init_empty, num_el);},
        [=](){return Data::new_with(&Data::init_linesep, num_el);},
    };
}

static std::vector<gen_data_func> gen_data_halo(int num_el) {
    int Q0 = 0, Q1 = num_el / 4, Q2 = num_el / 2, Q3 = num_el * 3 / 4, Q4 = num_el;
    return {
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q1});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q1, Q2});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q2, Q3});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q3, Q4});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q2});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q1, Q3});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q2, Q4});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q4});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q4});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q4});},
        [=](){return Data::new_with(&Data::init_rand, num_el, Range{Q0, Q4});},
    };
}

#ifndef SCORE_PREFIX
#define SCORE_PREFIX "score/"
#endif

int main() {
    auto dut = std::make_unique<DUT>();
    dut->init();
    int delay = dut->delay;
    int num_el = dut->num_el;
    std::cout << "delay=" << delay << " num_el=" << num_el << std::endl;
    generate_gtkw_file("trace/PE2/wave.gtkw", num_el);
    auto no_halo = gen_data_no_halo(num_el);
    auto halo = gen_data_halo(num_el);
    int idx = 0;
    std::ofstream out(SCORE_PREFIX "PE2.tb.out");
    for(auto & t: no_halo) {
        idx++;
        std::stringstream ss;
        ss << "trace/PE2/";
        ss << std::setw(3) << std::setfill('0') << idx << "-test";
        std::cout << ss.str() << std::endl;
        bool success = test_it((ss.str()+".vcd").c_str(), t());
        out << 0 << " " << (int)success << std::endl;
    }
    for(auto & t: halo) {
        idx++;
        std::stringstream ss;
        ss << "trace/PE2/";
        ss << std::setw(3) << std::setfill('0') << idx << "-test+halo";
        std::cout << ss.str() << std::endl;
        bool success = test_it((ss.str()+".vcd").c_str(), t());
        out << 1 << " " << (int)success << std::endl;
    }
    out.close();
    return 0;
}