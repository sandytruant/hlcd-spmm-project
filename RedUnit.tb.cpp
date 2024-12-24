#include "VRedUnit.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

namespace {

struct DUT: VRedUnit {
protected:
    VerilatedVcdC* tfp = nullptr;
    uint64_t sim_clock = 0;
public:
    using VRedUnit::VRedUnit;
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
    // uint8_t * data = (uint8_t*)&this->data_0;
    // uint8_t * split = (uint8_t*)&this->split_0;
    // uint8_t * out_idx = (uint8_t*)&this->out_idx_0;
    // uint8_t * out_data = (uint8_t*)&this->out_data_0;
};

struct Range {
    int start, stop;
    int gen() {
        return rand() % (stop - start) + start;
    }
};

struct Data {
    int n;
    std::vector<uint8_t> data;
    std::vector<uint8_t> split;
    std::vector<uint8_t> out_idx;
    std::vector<uint8_t> out_data;
    std::vector<bool> out_data_valid;
    void report_error(uint8_t * out_data) const {
        std::cout << "  Err:      ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4) << i << "   ";
        }
        std::cout << "\n" << "    Data:   ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4) << int(data[i]);
            if(split[i]) std::cout << "  |";
            else std::cout << "   ";
        }
        std::cout << "\n" << "    PSum:   ";
        uint8_t acc = 0;
        for(int i = 0; i < n; i++) {
            acc += data[i];
            std::cout << std::setw(4);
            if(split[i]) {
                std::cout << int(acc) << "  |";
                acc = 0;
            }
            else std::cout << " " << "   ";
        }
        std::cout << "\n" << "    OutIdx: ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4)  << int(out_idx[i]) << "   ";
        }
        std::cout << "\n" << "    Expect: ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4);
            if(out_data_valid[i]) std::cout << int(this->out_data[i]) << "   ";
            else std::cout << " " << "   ";
        }
        std::cout << "\n" << "    Get:    ";
        for(int i = 0; i < n; i++) {
            std::cout << std::setw(4) << int(out_data[i]) << "   ";
        }
        std::cout << "\n" << "\n";
    }
    bool compare(uint8_t * out_data) const {
        bool ok = true;
        for(int i = 0; i < n; i++) {
            if(out_data_valid[i] && this->out_data[i] != out_data[i]) {
                ok = false;
            }
        }
        if(!ok) {
            report_error(out_data);
        }
        return ok;
    }
    void init(int n) {
        this->n = n;
        data.resize(n);
        split.resize(n);
        out_idx.resize(n);
        out_data.resize(n);
        out_data_valid.resize(n);
    }
    void make_out_data() {
        int acc = 0;
        std::vector<uint8_t> out(n);
        std::vector<bool> out_valid(n, false);
        for(int i = 0; i < n; i++) {
            acc += data[i];
            if(split[i]) {
                out[i] = acc;
                acc = 0;
                out_valid[i] = true;
            }
        }
        for(int i = 0; i < n; i++) {
            out_data[i] = out[out_idx[i]];
            out_data_valid[i] = out_valid[out_idx[i]];
        }
    }
    static Data init_full(Range rg, int n) {
        Data res;
        res.init(n);
        for(int i = 0; i < n; i++) {
            res.data[i] = rg.gen();
            res.split[i] = 1;
            res.out_idx[i] = i;
        }
        res.make_out_data();
        return res;
    }
    static Data init_single(Range rg, int n) {
        Data res;
        res.init(n);
        for(int i = 0; i < n; i++) {
            res.data[i] = rg.gen();
            res.split[i] = i == n-1;
            res.out_idx[i] = i;
        }
        res.make_out_data();
        return res;
    }
    static Data init_random(Range rg, int n) {
        Data res;
        res.init(n);
        for(int i = 0; i < n; i++) {
            res.data[i] = rg.gen();
            res.split[i] = rand() % 3 == 0;
            res.out_idx[i] = i;
        }
        res.make_out_data();
        return res;
    }
    static Data init_shuffle(Range rg, int n) {
        Data res;
        res.init(n);
        for(int i = 0; i < n; i++) {
            res.data[i] = rg.gen();
            res.split[i] = rand() % 3 == 0;
            res.out_idx[i] = rand() % n;
        }
        res.make_out_data();
        return res;
    }
};

} // namespace

static void generate_gtkw_file(const char * out, int num_el) {
    std::ofstream fout(out);
    fout << "[timestart] 0\n";
    fout << "[color] 0\nTOP.clock\n";
    fout << "@24\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 0\nTOP.data[" << i << "][7:0]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 1\nTOP.split[" << i << "]\n";
    for(int i = 0; i < num_el; i++) fout << "[color] 2\nTOP.out_data[" << i << "][7:0]\n";
    fout.close();
}

static bool test_it(const char * vcd_file, const std::vector<Data> & data) {
    std::cout << "Generate " << vcd_file << std::endl;
    auto contextp = std::make_unique<VerilatedContext>();
    contextp->traceEverOn(true);
    auto dut = std::make_unique<DUT>(&*contextp);
    dut->init();
    dut->open_vcd(vcd_file);
    auto delay = dut->delay;
    auto num_el = dut->num_el;
    int total_cycles = data.size() + delay;
    bool ok = true;
    for(int i = 0; i < total_cycles; i++) {
        if(i < data.size()) {
            for(int j = 0; j < num_el; j++) {
                dut->data[j] = data[i].data[j];
                dut->split[j] = data[i].split[j];
                dut->out_idx[j] = data[i].out_idx[j];
            }
        }
        if(i >= delay) {
            int j = i - delay;
            ok |= data[j].compare(dut->out_data);
        }
        dut->step();
    }
    return ok;
}

int main(int argc, char ** argv) {
    auto dut = std::make_unique<DUT>();
    dut->init();
    auto delay = dut->delay;
    auto num_el = dut->num_el;
    std::cout << "delay=" << delay << " " << "num_el=" << num_el << std::endl;
    generate_gtkw_file("trace/RedUnit/wave.gtkw", num_el);
    auto make_data = [=](std::function<Data(Range, int)> gen){
        std::vector res{gen({1, 2}, num_el)};
        for(int i = 2; i <= 256; i*=2) {
            for(int k = 0; k < 10; k++) {
                res.push_back(gen({0, i}, num_el));
            }
        }
        return res;
    };
    test_it("trace/RedUnit/01-single.vcd", make_data(Data::init_single));
    test_it("trace/RedUnit/02-full.vcd", make_data(Data::init_full));
    test_it("trace/RedUnit/03-random.vcd", make_data(Data::init_random));
    test_it("trace/RedUnit/04-shuffle.vcd", make_data(Data::init_shuffle));
    return 0;
}
