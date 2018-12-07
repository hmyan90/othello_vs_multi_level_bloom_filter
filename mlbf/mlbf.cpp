#include <iostream>
#include <vector>
#include <cstdint>
#include "mlbf.hpp"

using bf::basic_bloom_filter;
using namespace std;

bool MLBFilter::build() {
    int r_remain = rCapacity;
    int s_remain = sCapacity;
    vector<string>* to_insert;
    vector<string>* to_check;
    int n;
    float curFpRate;

    for (int level=1; ; level++) {
        // cout << "Level" << level << " revoked remain " << r_remain << " stay remain " << s_remain << endl;  
        if (level == 1) {
            curFpRate = firstFpRate;
        } else {
            curFpRate = baseFpRate;
        }
        if (level % 2 == 1) {
            n = r_remain;
            to_insert = &revoked; 
            to_check = &stay;
            s_remain = round(curFpRate * s_remain);
        } else {
            n = s_remain;
            to_insert = &stay;
            to_check = &revoked;
            r_remain = round(curFpRate * r_remain);
        }

        cout << "Level" << level << " to_check " << to_check->size() << " to_insert " << to_insert->size() << endl;

        if (n < 100) {
            n = to_insert->size() + to_check->size(); // speed up for last several level
        }

        //mlbfilters.emplace_back(curFpRate, n); // a desired false-positive probability and capacity
        mlbfilters.emplace_back(baseFpRate, to_check->size() + to_insert->size());      

        for (vector<string>::iterator it = to_insert->begin(); it != to_insert->end(); ++it){
            mlbfilters.back().add(*it);
        }

        vector<string> fps;
        for (vector<string>::iterator it = to_check->begin(); it != to_check->end(); ++it){
            if (mlbfilters.back().lookup(*it)) {
                fps.push_back(*it);
            }
        }

        if ( fps.size() <= 1 || to_check->size() <= 1 || to_insert->size() <= 1) {
            break;
        }

        if (level % 2 == 1) {
            stay = fps;
        } else {
            revoked = fps;
        }
    }
    return true;
} 


MLBFilter::MLBFilter(int _rCapacity, int _sCapacity, vector<string> _revoked, vector<string> _stay, float _firstFpRate, float _baseFpRate) {
    rCapacity = _rCapacity;
    sCapacity = _sCapacity;
    revoked = _revoked;
    stay = _stay;
    firstFpRate = _firstFpRate;
    baseFpRate = _baseFpRate;

    build();
};


bool MLBFilter::contains(string data) {
    // contains: false means in S, true means in R
    bool included = false;
    for (vector<basic_bloom_filter>::iterator filter = mlbfilters.begin(); filter != mlbfilters.end(); ++filter){
        if (filter->lookup(data)) {
            included = !included;
        } else {
            return included;
        }
    }

    if (mlbfilters.size() % 2 == 1)  {
        // odd level, is definitively in R
        return true;
    } else {
        // even level, is definitively in S
        return false;
    }
}

size_t MLBFilter::bytesize() {
    size_t size = 0;
    for (vector<basic_bloom_filter>::iterator filter = mlbfilters.begin(); filter != mlbfilters.end(); ++filter){
        size += filter->size();
    }
    return size;
}
