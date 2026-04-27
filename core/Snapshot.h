#pragma once
#include <vector>
#include <utility>

struct Snapshot
{
    long long timestamp;

    std::vector<std::pair<int, int>> bidLevels; // price, total qty
    std::vector<std::pair<int, int>> askLevels;
};