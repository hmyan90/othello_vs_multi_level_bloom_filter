#include <iostream>
#include <fstream>
#include <ctime>
#include <cassert>
#include <sys/time.h>
#include <algorithm>
#include "mlbf/mlbf.hpp"
#include "othello/control_plane_othello.h"

using namespace std;

typedef string Key;
typedef bool Val;

#define REVOKED_FLAG true
#define STAY_FLAG false

int diffs_ms(timeval t1, timeval t2) {
  // 1ms = 10^(-3)s; 
  return (((t1.tv_sec - t2.tv_sec) * 1000000) + (t1.tv_usec - t2.tv_usec)) / 1000;
}

int diffs_us(timeval t1, timeval t2) {
  // 1us = 10^(-6)s;
  return (((t1.tv_sec - t2.tv_sec) * 1000000) + (t1.tv_usec - t2.tv_usec));
}

class TestBase {
public:
  struct timeval sStart, sEnd;
  virtual void build(vector<Key>& _revoked, vector<Key>& _stay) = 0;
  virtual Val query(Key& k) = 0;
  virtual size_t getMemSize() = 0;

  virtual ~TestBase() {
  }
};

class OthelloStorage: public TestBase {
public:
  ControlPlaneOthello<Key, Val>* oth;


  virtual void build(vector<string>& _revoked, vector<string>& _stay ) {
    vector<Key> all_keys;
    for (int i = 0; i < _revoked.size(); ++i) {
      all_keys.push_back(_revoked[i]);
    }
    for (int i = 0; i < _stay.size(); ++i) {
      all_keys.push_back(_stay[i]);
    }

    vector<Val> all_values;
    for (int i = 0; i < _revoked.size(); ++i) {
      all_values.push_back(REVOKED_FLAG);
    }
    for (int i = 0; i < _stay.size(); ++i) {
      all_values.push_back(STAY_FLAG);
    }

    gettimeofday(&sStart, NULL);
    oth = new ControlPlaneOthello<Key, Val>(all_keys, all_keys.size(), all_values);
    gettimeofday(&sEnd, NULL);
    cout << "Othello build time: " << diffs_ms(sEnd, sStart) << "ms\n";
  }

  inline virtual Val query(Key& k) {
    return oth->query(k);
  }
  
  inline virtual size_t getMemSize() {
    // return sizeof(pair<Key, Val>) * o.size() + o.getMemSize() * sizeof(uint32_t); 
    return oth->getMemSize() * sizeof(Val);
  }
};

class MLBFStorage: public TestBase {
public:
  // cuckoohash_map<string, string, Hasher32<string>> cuckoo_table;
  MLBFilter* mlbf;

  virtual void build(vector<string>& _revoked, vector<string>& _stay) {
    gettimeofday(&sStart, NULL);
    mlbf = new MLBFilter(_revoked.size(), _stay.size(), _revoked, _stay, 0.5);
    gettimeofday(&sEnd, NULL);
    cout << "MLBF build time: " << diffs_ms(sEnd, sStart) << "ms\n";
  }

  inline virtual Val query(Key& k) {
    return mlbf->contains(k);
  }
  
  inline virtual size_t getMemSize() {
    return mlbf->bytesize();
  }
};

OthelloStorage o;
MLBFStorage m;

vector<Key> revoked;
vector<Key> stay;

int loadData(char* revoked_filename, char* stay_filename) {
  string value;
  
  // load revoked data
  ifstream revoked_data(revoked_filename);
  if (!revoked_data.is_open()) {
    cout << "ERROR: file Open failed\n";
    return -1;
  }
          
  cout << "Start load revoked data...\n";
  
  while (getline(revoked_data, value, '\n')) {
    std::string value1 = value.substr(0, value.length()/2);
    revoked.push_back(value1);
  }
  revoked_data.close();

  // load non-revoked data
  ifstream stay_data(stay_filename);
  if (!stay_data.is_open()) {
    cout << "ERROR: file Open failed\n";
    return -1;
  }
        
  cout << "Start load non-revoked data...\n";
  
  while (getline(stay_data, value, '\n')) {
    std::string value1 = value.substr(0, value.length()/2);
    stay.push_back(value1);
  }
  stay_data.close();
 
  o.build(revoked, stay);
  m.build(revoked, stay);
  
  // memory usage
  cout << "Othello size: " << o.getMemSize() / 1024.0 / 1024.0 << "MB\n";
  cout << "MLBF size: " << m.getMemSize() / 1024.0 / 1024.0 << "MB\n";
  return 0;
}

void queryAll() {
  struct timeval qStart, qEnd;
  int queryTimes = 10000000;
  vector<int> randomIndex;
  int totalNum = revoked.size() + stay.size();
  
  cout << "query " << queryTimes << " times\n";
  
  // generate key(index) to be queried
  for (int i = 0; i < queryTimes; i++) {
    randomIndex.push_back(rand() % totalNum);
  }
  
  // start query
  cout << "---Othello---" << endl;
  gettimeofday(&qStart, NULL);
  int error = 0;  
  for (int i = 0; i < queryTimes; i++) {
    int idx = randomIndex[i];
    if (idx < revoked.size()) {
      if (o.query(revoked[idx]) != REVOKED_FLAG) {
        error += 1;
      }
    } else {
      idx = idx - revoked.size();
      if (o.query(stay[idx]) != STAY_FLAG) {
        error += 1;
      }
    }
  }
  cout << "Error count " << error << endl;
  gettimeofday(&qEnd, NULL);
  cout << "Average query time: " << diffs_us(qEnd, qStart) / (queryTimes * 1.0) << "us\n";
  cout << "Query Throughout is: " << 1000000.0 * queryTimes / diffs_us(qEnd, qStart) << '\n';
  
  cout << "---MLBF---" << endl;
  gettimeofday(&qStart, NULL);
  error = 0;  
  for (int i = 0; i < queryTimes; i++) {
    int idx = randomIndex[i];
    if (idx < revoked.size()) {
      if (m.query(revoked[idx]) != REVOKED_FLAG) {
        error += 1;
      }
    } else {
      idx = idx - revoked.size();
      if (m.query(stay[idx]) != STAY_FLAG) {
        error += 1;
      }
    }
  }
  cout << "Error count " << error << endl;
  gettimeofday(&qEnd, NULL);
  cout << "Average query time: " << diffs_us(qEnd, qStart) / (queryTimes * 1.0) << "us\n";
  cout << "Query Throughout is: " << 1000000.0 * queryTimes / diffs_us(qEnd, qStart) << '\n';
}

int main(int argc, char **argv) {
  // check input validity
  if (argc != 3) {
    cout << "Please input revoked file name and un-revoked file name\n";
    return -1;
  }
  
  // load data and calculate memory
  if (loadData(argv[1], argv[2]) < 0) return -1;
  
  //query
  queryAll();
  return 0;
}
