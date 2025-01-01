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

#ifndef SCORE_PREFIX
#define SCORE_PREFIX "score/"
#endif

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
    for(auto f: {SCORE_PREFIX "PE2.tb.out", SCORE_PREFIX "SpMM2.tb.out"}) {
        std::cerr << "reading " << f << std::endl;
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
    std::cerr << "COMPONENT SUCCESS RATE:" << std::endl;
    for(auto i: {1, 2, 4, 8}) {
        std::cerr << get_name(i) << "   = " << ratio[i] << std::endl;
    }
    std::cerr << "COMPLEXITY BEST ROUTE:" << std::endl;
    double propagate = 1, partsum = 0;
    for(int i = 0, s = 0; i < 4; i++) {
        s ^= 1 << route[i];
        propagate *= ratio[s];
        partsum += propagate;
        std::cerr << get_name(s) << "  ";
        std::cerr << " success-rate=" << std::fixed << std::setprecision(4) << ratio[s];
        std::cerr << " cum-prod=" << std::fixed << std::setprecision(4)  << propagate;
        std::cerr << " part-sum=" << std::fixed << std::setprecision(4)  << partsum << std::endl;
    }
    std::cerr << std::endl;
    double complexity_score = score[15];
    double component_score = ratio[1] + ratio[2] + ratio[4] + ratio[8];
    std::cerr << "COMPLEXITY SCORE:  " << std::fixed << std::setprecision(4) << complexity_score << "  /  4" << std::endl;
    std::cerr << "COMPONENT SCORE :  " << std::fixed << std::setprecision(4) << component_score  << "  /  4" << std::endl;
    std::cerr << "FINAL SCORE     : " << std::fixed << std::setprecision(4) << (component_score + complexity_score) * 2.5 << "  / 20" << std::endl;
    return 0;
}