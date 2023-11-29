#include <iostream>
#include <set>

#include "gflags/gflags.h"
#include "pam/pam.h"
#include "pam/treap.h"
#include "parlay/parallel.h"
#include "treap2.h"

using namespace std;

struct Info {
  using key_t = size_t;
  key_t key;
  size_t val, sum;
  Info(size_t key_ = 0) : key(key_), val(0), sum(0) {}
  static bool cmp(const key_t& a, const key_t& b) { return a < b; }
  void Update(Info* left, Info* right) {
    sum = val;
    if (left) sum += left->sum;
    if (right) sum += right->sum;
  }
};

struct entry {
  using key_t = size_t;
  using val_t = size_t;
  using aug_t = size_t;
  static inline bool comp(key_t a, key_t b) { return a < b; }
  static aug_t get_empty() { return 0; }
  static aug_t from_entry(key_t k, val_t v) { return v; }
  static aug_t combine(aug_t a, aug_t b) { return a + b; }
  static inline size_t hash(const auto& e) {
    return parlay::hash64(e.first * e.second);
  }
};

auto Generate(size_t n, size_t kk, size_t vv, size_t seed) {
  assert(seed > 0);
  parlay::sequence<pair<size_t, size_t>> a(n);
  parlay::parallel_for(0, n, [&](size_t i) {
    a[i].first = parlay::hash64(i ^ parlay::hash64(seed)) % kk;
    a[i].second =
        parlay::hash64((a[i].first ^ parlay::hash64(i - seed)) + seed) % vv;
  });
  return a;
}

void TestUnion(auto a, auto b) {
  cout << "\nTestUnion start, a.size: " << a.size() << ", b.size: " << b.size()
       << endl;
  using map_t = aug_map<entry, treap<entry>>;
  map_t map1, map2;
  auto replace = [](const auto& a, const auto& b) { return a; };
  map1 = map_t::Tree::multi_insert_sorted(map1.get_root(), a.data(), a.size(),
                                          replace);
  map2 = map_t::Tree::multi_insert_sorted(map2.get_root(), b.data(), b.size(),
                                          replace);

  Treap<Info> treap1, treap2;
  treap1.root = treap1.BuildTree(0, a.size() - 1, [&](size_t i, Info* info) {
    info->key = a[i].first;
    info->val = a[i].second;
  });
  treap2.root = treap2.BuildTree(0, b.size() - 1, [&](size_t i, Info* info) {
    info->key = b[i].first;
    info->val = b[i].second;
  });

  parlay::internal::timer t1;
  map1 = map_t::map_union(map1, map2, replace);
  cout << "pam union time: " << t1.stop() << endl;

  parlay::internal::timer t2;
  treap1.root = treap1.Union(treap1.root, treap2.root);
  cout << "treap union time: " << t2.stop() << endl;

  cout << "union size: " << map1.size() << endl;

  bool ok = true;
  ok &= map1.size() == treap1.root->size;
  ok &= map1.aug_val() == treap1.root->sum;
  cout << "TestUnion result: " << (ok ? "true" : "false") << endl;
}

void TestIntersect(auto a, auto b) {
  cout << "\nTestIntersect start, a.size: " << a.size()
       << ", b.size: " << b.size() << endl;
  using map_t = aug_map<entry, treap<entry>>;
  map_t map1, map2;
  auto replace = [](const auto& a, const auto& b) { return a; };
  map1 = map_t::Tree::multi_insert_sorted(map1.get_root(), a.data(), a.size(),
                                          replace);
  map2 = map_t::Tree::multi_insert_sorted(map2.get_root(), b.data(), b.size(),
                                          replace);

  Treap<Info> treap1, treap2;
  treap1.root = treap1.BuildTree(0, a.size() - 1, [&](size_t i, Info* info) {
    info->key = a[i].first;
    info->val = a[i].second;
  });
  treap2.root = treap2.BuildTree(0, b.size() - 1, [&](size_t i, Info* info) {
    info->key = b[i].first;
    info->val = b[i].second;
  });

  parlay::internal::timer t1;
  map1 = map_t::map_intersect(map1, map2, replace);
  cout << "pam intersect time: " << t1.stop() << endl;

  parlay::internal::timer t2;
  treap1.root = treap1.Intersect(treap1.root, treap2.root);
  cout << "treap intersect time: " << t2.stop() << endl;

  cout << "intersect size: " << map1.size() << endl;

  bool ok = true;
  ok &= map1.size() == treap1.root->size;
  ok &= map1.aug_val() == treap1.root->sum;
  cout << "TestIntersect result: " << (ok ? "true" : "false") << endl;
}

void TestFilter(auto a) {
  cout << "\nTestFilter start, a.size: " << a.size() << endl;
  using map_t = aug_map<entry, treap<entry>>;
  map_t map;
  auto replace = [](const auto& a, const auto& b) { return a; };
  map = map_t::Tree::multi_insert_sorted(map.get_root(), a.data(), a.size(),
                                         replace);

  Treap<Info> treap;
  treap.root = treap.BuildTree(0, a.size() - 1, [&](size_t i, Info* info) {
    info->key = a[i].first;
    info->val = a[i].second;
  });

  parlay::internal::timer t1;
  map = map_t::filter(map, [](const auto& p) { return p.second % 2 == 0; });
  cout << "pam filter time: " << t1.stop() << endl;

  parlay::internal::timer t2;
  treap.root =
      treap.Filter(treap.root, [](Info* info) { return info->val % 2 == 0; });
  cout << "treap filter time: " << t2.stop() << endl;

  cout << "filter size: " << map.size() << endl;

  bool ok = true;
  ok &= map.size() == treap.root->size;
  ok &= map.aug_val() == treap.root->sum;
  cout << "TestFilter result: " << (ok ? "true" : "false") << endl;
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // using map_t = aug_map<entry, treap<entry>>;
  // map_t a;
  // vector<pair<int, long long>> b = {{1, 2}, {3, 4}, {5, 6}};
  // auto replace = [](const auto& a, const auto& b) { return b; };
  // a = map_t::Tree::multi_insert_sorted(a.get_root(), b.data(), b.size(),
  //                                      replace);
  // cout << a.aug_val() << endl;

  auto a = Generate(1000000, 3000000, 1000000000, 1);
  auto b = Generate(1000000, 3000000, 1000000000, 2);

  parlay::sort_inplace(a);
  parlay::sort_inplace(b);

  a = parlay::unique(
      a, [](const auto& p, const auto& q) { return p.first == q.first; });
  b = parlay::unique(
      b, [](const auto& p, const auto& q) { return p.first == q.first; });

  TestUnion(a, b);

  TestIntersect(a, b);

  TestFilter(a);

  return 0;
}