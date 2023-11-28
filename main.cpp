#include <iostream>

#include "pam/pam.h"
#include "pam/treap.h"
#include "parlay/parallel.h"
#include "treap.h"

using namespace std;

struct Info {
  int val, sum;
  Info(int val_ = 0) : val(val_), sum(0) {}
  bool operator<(const Info& info) const { return val < info.val; }
  void Update(Info* left, Info* right) {
    sum = val;
    if (left) sum += left->sum;
    if (right) sum += right->sum;
  }
};

struct entry {
  using key_t = int;
  using val_t = long long;
  using aug_t = long long;
  static inline bool comp(key_t a, key_t b) { return a < b; }
  static aug_t get_empty() { return 0; }
  static aug_t from_entry(key_t k, val_t v) { return v; }
  static aug_t combine(aug_t a, aug_t b) { return a + b; }
  static inline size_t hash(const auto& e) {
    return parlay::hash64(e.first * e.second);
  }
};

int main() {
  using map_t = aug_map<entry, treap<entry>>;
  map_t a;
  vector<pair<int, long long>> b = {{1, 2}, {3, 4}, {5, 6}};
  auto replace = [](const auto& a, const auto& b) { return b; };
  a = map_t::Tree::multi_insert_sorted(a.get_root(), b.data(), b.size(),
                                       replace);
  cout << a.aug_val() << endl;

  Treap<Info, true> treap;
  treap.Insert(Info(2));
  treap.Insert(Info(3));
  treap.Insert(Info(5));
  treap.Insert(Info(2));
  treap.Traverse(treap.root, [&](Info* info) { cout << info->val << '\n'; });
  cout << treap.root->sum << endl;

  return 0;
}