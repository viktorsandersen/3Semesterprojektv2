#pragma once
#include <iostream>
#include <string>
#include <vector>

int getbit(std::vector<int> msg,int line, int bit);

int signal_to_int(std::vector<char> msg);

bool errorcheck(std::vector<int> msg);

int getdir(std::vector<int> msg);

int getstop(std::vector<int> msg);