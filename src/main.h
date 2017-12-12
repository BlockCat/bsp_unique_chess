#include <iostream>

#include <mcbsp.hpp>
#include <mcbsp-templates.hpp>
#include <thc.h>
#include <queue>
#include <btree_set.h>

using namespace thc;
using namespace std;


struct CompressedLess {
    CompressedLess() : n(2) {}
    CompressedLess(size_t length)
        : n(length) {
    }
    bool operator()(const CompressedPosition &a, const CompressedPosition &b) const {
        int as = memcmp(&a, &b, 24);
        return as < 0;
    }
    size_t n;
  };

typedef btree::btree_set<CompressedPosition, CompressedLess> PositionSet;
typedef vector<CompressedPosition> CompressedQueue;