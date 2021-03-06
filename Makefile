# makefile
GCC=clang++
FLAG= -Wall -std=c++11 -lpthread -O0 -ggdb

maketest: othello/common.cpp mlbf/bf/bitvector.cc mlbf/bf/hash.cc mlbf/bf/basic.cc mlbf/mlbf.cpp test.cpp
	${GCC} ${FLAG} othello/common.cpp mlbf/bf/bitvector.cc mlbf/bf/hash.cc mlbf/bf/basic.cc mlbf/mlbf.cpp test.cpp -o test

clean:
	rm -fr *.o
