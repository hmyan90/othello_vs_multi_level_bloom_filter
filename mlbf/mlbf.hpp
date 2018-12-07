#pragma once

#include "bf/basic.h"
#include <cmath>
#include <deque>

using bf::basic_bloom_filter;
using namespace std;

class MLBFilter {
    private:
        int rCapacity;
        int sCapacity;
        vector<string> revoked;
        vector<string> stay;
        float firstFpRate;
        float baseFpRate;
        vector<basic_bloom_filter> mlbfilters;
    
        bool build();

    public:
        MLBFilter(int _rCapacity, int _sCapacity, vector<string> _revoked, vector<string> _stay, float firstFpRate, float _baseFpRate);

        bool contains(string data);

        size_t bytesize();

};
