#include <iostream>

#include "pam/pam.h"
#include "pam/treap.h"
#include "parlay/parallel.h"
#include "treap2.h"

using namespace std;

struct Info {
  using key_t = int;
  key_t key;
  long long val, sum;
  Info(int key_ = 0) : key(key_), val(0), sum(0) {}
  static bool cmp(const key_t& a, const key_t& b) { return a < b; }
  void Update(Info* left, Info* right) {
    sum = key;
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

std::mt19937 rng(0);

int main() {
  using map_t = aug_map<entry, treap<entry>>;
  map_t a;
  vector<pair<int, long long>> b = {{1, 2}, {3, 4}, {5, 6}};
  auto replace = [](const auto& a, const auto& b) { return b; };
  a = map_t::Tree::multi_insert_sorted(a.get_root(), b.data(), b.size(),
                                       replace);
  cout << a.aug_val() << endl;

  vector<pair<int, long long>> tt(10);
  for (int i = 0; i < 10; i++) {
    tt[i].first = i;
    tt[i].second = rng() % 100;
  }
  Treap2<Info> treap;
  cout << "before build" << endl;
  treap.root = treap.BuildTree(0, 9, [&](int i, Info* info) {
    info->key = tt[i].first;
    info->val = tt[i].second;
  });
  cout << "after build" << endl;
  int cnt = 0;
  treap.Traverse(treap.root, [&](Info* info) {
    cnt++;
    cout << info->key << ' ' << info->val << '\n';
  });
  cout << "cnt: " << cnt << endl;

  return 0;
}