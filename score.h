#pragma once
const double weight_halo  = 5.0 / 211;
const double weight_dbbuf = 5.0 / 240;
const double weight_ws    = 5.0 / 160;
const double weight_os    = 5.0 / 120;
inline double get_score(bool halo, bool dbbuf, bool ws, bool os) {
    double res = 0;
    if(halo) res += weight_halo;
    if(dbbuf) res += weight_dbbuf;
    if(ws) res += weight_ws;
    if(os) res += weight_os;
    return res;
}
