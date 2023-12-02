#ifndef TREAP_H_
#define TREAP_H_

#include <cassert>
#include <functional>
#include <tuple>
#include <vector>

#include "parlay/alloc.h"
#include "parlay/parallel.h"
#include "parlay/primitives.h"
#include "parlay/utilities.h"
#include "utils.h"

/*
struct Info {
  using key_t = size_t;
  key_t key;
  // other values you want to maintain
  Info() {}
  static bool cmp(const key_t& a, const key_t& b) {}
  void Update(Info* left, Info* right) {}
};
*/

template <typename Info>
struct KthCmp {
  KthCmp(size_t k_) : k(k_) {}
  size_t k;
  auto operator()() {
    return [&](Info* info) {
      auto x = info->Node();
      size_t left = x->lch ? x->lch->size : 0;
      if (k <= left) return -1;
      if (k == left + 1) return 0;
      k -= left + 1;
      return 1;
    };
  }
};

template <typename Info>
struct Treap {
  using info_t = Info;
  using key_t = typename Info::key_t;

  struct Node : public Info {
    size_t priority;
    size_t size;
    Node *lch, *rch;
    Node() : size(1), lch(nullptr), rch(nullptr) {
      priority = parlay::hash64((size_t)this);
    }
    void Set(const Info& info) { (Info&)(*this) = info; }
  };

  using allocator = parlay::type_allocator<Node>;

  Node* Create() { return allocator::create(); }

  Node* root;
  Treap() : root(nullptr) {}

  Node* Update(Node* x) {
    x->Update(x->lch, x->rch);
    x->size = 1 + (x->lch ? x->lch->size : 0) + (x->rch ? x->rch->size : 0);
    return x;
  }

  Node* Join(Node* x, Node* y) {
    if (!x) return y ? Update(y) : nullptr;
    if (!y) return Update(x);
    if (x->priority >= y->priority) {
      x->rch = Join(x->rch, y);
      return Update(x);
    } else {
      y->lch = Join(x, y->lch);
      return Update(y);
    }
  }

  Node* Join(Node* x, Node* y, Node* z) {
    if (!x) return Join(y, z);
    if (!y) return Join(x, z);
    if (!z) return Join(x, y);
    assert(!y->lch && !y->rch);  // y must be single node
    if (x->priority >= y->priority && x->priority >= z->priority) {
      x->rch = Join(x->rch, y, z);
      return Update(x);
    } else if (y->priority >= z->priority) {
      y->lch = x;
      y->rch = z;
      return Update(y);
    } else {
      z->lch = Join(x, y, z->lch);
      return Update(z);
    }
  }

  // cmp(info) < 0: split lch
  // cmp(info) = 0: return this
  // cmp(info) > 0: split rch
  template <typename Cmp>
  std::tuple<Node*, Node*, Node*> Split(Node* x, Cmp cmp) {
    if (!x) return {nullptr, nullptr, nullptr};
    auto d = cmp(x);
    if (d == 0) {
      auto l = x->lch, r = x->rch;
      x->lch = x->rch = nullptr;
      return {l, Update(x), r};
    } else if (d < 0) {
      auto [w, y, z] = Split(x->lch, cmp);
      x->lch = nullptr;
      return {w, y, Join(z, Update(x))};
    } else {
      auto [w, y, z] = Split(x->rch, cmp);
      x->rch = nullptr;
      return {Join(Update(x), w), y, z};
    }
  }

  auto Split(Node* x, const key_t& key) {
    return Split(x, [&](Info* t) {
      if (Info::cmp(key, t->key)) return -1;
      if (Info::cmp(t->key, key)) return 1;
      return 0;
    });
  }

  auto SplitKth(Node* x, size_t k) { return Split(x, KthCmp<Info>(k)()); }

  template <typename F>
  void Traverse(Node* x, F f) {
    if (!x) return;
    Traverse(x->lch, f);
    f(x);
    Traverse(x->rch, f);
  }

  template <typename F>
  Node* BuildTree(size_t l, size_t r, F f) {
    if (l > r) return nullptr;
    size_t mid = (l + r) / 2;
    Node* x = Create();
    f(mid, x);
    bool parallel = r - l + 1 > 100;
    conditional_par_do(parallel,
      [&]() { if (l < mid) x->lch = BuildTree(l, mid - 1, f); },
      [&]() { if (mid < r) x->rch = BuildTree(mid + 1, r, f); }
    );
    return Update(x);
  }

  void Insert(const Info& info) {
    Node* x = Create();
    x->Set(info);
    auto [t1, t2, t3] = Split(root, info.key);
    if (!t2) t2 = x;
    root = Join(t1, t2, t3);
  }

  bool Delete(const key_t& key) {
    auto [t1, t2, t3] = Split(root, key);
    root = Join(t1, t3);
    bool res = t2;
    if (t2) allocator::free(t2);
    return res;
  }

  Node* Find(const key_t& key) {
    Node* x = root;
    while (x) {
      if (Info::cmp(key, x->key)) x = x->lch;
      else if (Info::cmp(x->key, key)) x = x->rch;
      else break;
    }
    return x;
  }

  size_t Rank(const key_t& key) {
    Node* x = root;
    size_t less = 0;
    while (x) {
      if (Info::cmp(key, x->key)) x = x->lch;
      else if (Info::cmp(x->key, key)) {
        less += x->lch ? x->lch->size : 0;
        less++;
        x = x->rch;
      } else {
        less += x->lch ? x->lch->size : 0;
        break;
      }
    }
    return less + 1;
  }

  size_t dbg(Node* x) { return x ? x->size : 0; }

  Node* Union(Node* x, Node* y) {
    if (!x) return y;
    if (!y) return x;
    bool parallel = x->size > 100 && y->size > 100;
    auto [t1, t2, t3] = Split(y, x->key);
    if (t2) allocator::free(t2);
    Node* left;
    Node* right;
    conditional_par_do(
        parallel, [&]() { left = Union(x->lch, t1); },
        [&]() { right = Union(x->rch, t3); });
    x->lch = x->rch = nullptr;
    return Join(left, Update(x), right);
  }

  Node* Intersect(Node* x, Node* y) {
    if (!x) {
      GC(y);
      return nullptr;
    }
    if (!y) {
      GC(x);
      return nullptr;
    }
    bool parallel = x->size > 100 && y->size > 100;
    auto [t1, t2, t3] = Split(y, x->key);
    Node* left;
    Node* right;
    conditional_par_do(
        parallel, [&]() { left = Intersect(x->lch, t1); },
        [&]() { right = Intersect(x->rch, t3); });
    x->lch = x->rch = nullptr;
    if (t2) {
      allocator::free(t2);
      return Join(left, Update(x), right);
    }
    else {
      allocator::free(x);
      return Join(left, right);
    }
  }

  template <typename F>
  Node* Filter(Node* x, F f) {
    if (!x) return nullptr;
    bool parallel = x->size > 100;
    Node* left;
    Node* right;
    conditional_par_do(
        parallel, [&]() { left = Filter(x->lch, f); },
        [&]() { right = Filter(x->rch, f); });
    x->lch = x->rch = nullptr;
    if (f(x)) return Join(left, Update(x), right);
    else return Join(left, right);
  }

  void GC(Node* x) {
    if (!x) return;
    bool parallel = x->size > 100;
    conditional_par_do(parallel,
    [&]() {GC(x->lch);},
      [&]() {GC(x->rch);}
    );
    allocator::free(x);
  }
};

#endif  // TREAP_H_
