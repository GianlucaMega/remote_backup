#pragma once

#include <iostream>
#include <vector>

std::string computeHash(const std::vector<char>& data);
std::string computeFileHash(const std::string& path);
std::string getActionString(int opcode);