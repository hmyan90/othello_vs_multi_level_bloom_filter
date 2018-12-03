#include <iostream>
#include <vector>
#include <cstdint>
#include "mlbf.hpp"

using bf::basic_bloom_filter;
using namespace std;

bool MLBFilter::build() {
    //bool done = false;
    //int r_remain = rCapacity;
    //int s_remain = sCapacity;
    vector<string>* to_insert;
    vector<string>* to_check;

    for (int level=1; ; level++) {
        
        if (level % 2 == 1) {
            to_insert = &revoked; 
            to_check = &stay;
        } else {
            to_insert = &stay;
            to_check = &revoked;
        }

        cout << "Level" << level << " revoked remain " << revoked.size() << " stay remain " << stay.size() << endl; 

        mlbfilters.emplace_back(baseFpRate, to_check->size() + to_insert->size()); // a desired false-positive probability and capacity

        for (vector<string>::iterator it = to_insert->begin(); it != to_insert->end(); ++it){
            mlbfilters.back().add(*it);
        }

        vector<string> fps;
        for (vector<string>::iterator it = to_check->begin(); it != to_check->end(); ++it){
            if (mlbfilters.back().lookup(*it)) {
                fps.push_back(*it);
            }
        }

        if (level % 2 == 1) {
            stay = fps;
        } else {
            revoked = fps;
        }

        if ( fps.size() == 0 ) {
            break;
        }
    }
    return true;
} 


MLBFilter::MLBFilter(int _rCapacity, int _sCapacity, vector<string> _revoked, vector<string> _stay, float _baseFpRate) {
    rCapacity = _rCapacity;
    sCapacity = _sCapacity;
    revoked = _revoked;
    stay = _stay;
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
