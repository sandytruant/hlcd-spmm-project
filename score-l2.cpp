#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
using namespace std;

int total[16];
int good[16];
double ratio[16];
double best_route[16];
double score[16];
int parent[16];

std::string get_name(int mask) {
    static const char * names[] = {
        "halo",
        "dbbuf",
        "ws",
        "os"
    };
    std::stringstream ss;
    for(int i = 0; i < 4; i++) {
        if(mask >> i & 1) {
            ss << "  " << names[i];
        } else {
            ss << "  ";
            for(int t = 0, e = strlen(names[i]); t < e; t++) {
                ss << " ";
            }
        }
    }
    return ss.str();
}

int main() {
    for(auto f: {"score/PE2.tb.out", "score/SpMM2.tb.out"}) {
        ifstream fpe(f);
        int mask, success;
        while(fpe >> mask >> success) {
            total[mask]++;
            good[mask] += success != 0;
        }
    }
    std::cerr << "SUCCESS RATE: " << std::endl;
    for(int i = 1; i < 16; i++) {
        if(total[i] == 0) {
            ratio[i] = 0.0;
        } else {
            ratio[i] = 1.0 * good[i] / total[i];
        }
        std::cerr  << get_name(i) << "   = " << std::fixed << std::setprecision(4) << ratio[i] << std::endl;
    }
    best_route[0] = 1.0;
    for(int s = 1; s < 16; s++) {
        int from = 0;
        for(int i = 0; i < 4; i++) {
            if(s >> i & 1) {
                auto cur_score = best_route[s ^ (1 << i)] * ratio[s] + score[s ^ (1 << i)];
                if(cur_score >= score[s]) {
                    score[s] = cur_score;
                    from = i;
                }
                best_route[s] = std::max(best_route[s], best_route[s ^ (1 << i)] * ratio[s]);
            }
        }
        parent[s] = from;
    }
    std::cerr << std::endl;
    int route[4] = {};
    for(int t = 15, i = 0; i < 4; t = t ^ (1<<parent[t]), i++) {
        route[3 - i] = parent[t];
    }
    std::cerr << "BEST ROUTE:" << std::endl;
    double propagate = 1, partsum = 0;
    for(int i = 0, s = 0; i < 4; i++) {
        s ^= 1 << route[i];
        propagate *= ratio[s];
        partsum += propagate;
        std::cerr << get_name(s) << "  ";
        std::cerr << " success-rate=" << std::fixed << std::setprecision(4) << ratio[s];
        std::cerr << " cum_prod=" << std::fixed << std::setprecision(4)  << propagate;
        std::cerr << " part_sum=" << std::fixed << std::setprecision(4)  << partsum << std::endl;
    }
    std::cerr << std::endl;
    std::cerr << "EXPECTED VAL: " << std::fixed << std::setprecision(4) << score[15]  << std::endl;
    std::cerr << "WEIGHT       *     5 " << std::endl;
    std::cerr << "FINAL SCORE : " << std::fixed << std::setprecision(4) << score[15] * 5 << std::endl;
    return 0;
}