#ifndef TREAP_H_
#define TREAP_H_

#include <cassert>
#include <functional>
#include <tuple>
#include <vector>

#include "parlay/utilities.h"

/*
struct Info {
  Info() {}
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

template <bool>
struct TreapNodeBase {};
template <>
struct TreapNodeBase<false> {};
template <>
struct TreapNodeBase<true> {
  size_t ts;
};

template <typename Info, bool persist = false>
struct Treap {
  using info_t = Info;

  struct Node : public Info, public TreapNodeBase<persist> {
    size_t priority;
    Node *lch, *rch;
    Node(size_t ts_ = 0) : lch(nullptr), rch(nullptr) {
      priority = parlay::hash64((size_t)this);
      if constexpr (persist) this->ts = ts_;
    }
    // when copying a node, must be in persist mode
    Node(Node* x, size_t ts) {
      *this = *x;
      if constexpr (persist) this->ts = ts;
    }
    void Set(const Info& info) { (Info&)(*this) = info; }
  };

  Node* Create() { return new Node(ts); }
  Node* Copy(Node* x) { return new Node(x, ts); }

  Node* root;
  size_t ts;  // plus version if persistence is wanted
  Treap() : root(nullptr), ts(0) {}

  Node* Update(Node* x) {
    x->Update(x->lch, x->rch);
    return x;
  }

  Node* Join(Node* x, Node* y) {
    if (!x) return y ? Update(y) : nullptr;
    if (!y) return Update(x);
    if constexpr (persist) {
      if (x->ts != ts) x = new Node(x, ts);
      if (y->ts != ts) y = new Node(y, ts);
    }
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
    if constexpr (persist) {
      if (x->ts != ts) x = new Node(x, ts);
      if (y->ts != ts) y = new Node(y, ts);
      if (z->ts != ts) z = new Node(z, ts);
    }
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
    if constexpr (persist) {
      if (x->ts != ts) x = new Node(x, ts);
    }
    auto d = cmp(x);
    if (d == 0) {
      auto l = x->lch, r = x->rch;
      if constexpr (persist) {
        if (l && l->ts != ts) l = new Node(l, ts);
        if (r && r->ts != ts) r = new Node(r, ts);
      }
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

  auto Split(Node* x, const Info& info) {
    return Split(root, [&](Info* t) {
      if (info < (*t)) return -1;
      if ((*t) < info) return 1;
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

  void Insert(const Info& info) {
    Node* x = Create();
    x->Set(info);
    auto [t1, t2, t3] = Split(root, info);
    if (!t2) t2 = x;
    root = Join(t1, t2, t3);
  }

  bool Delete(const Info& info) {
    auto [t1, t2, t3] = Split(root, info);
    root = Join(t1, t3);
    bool res = t2;
    if (t2) delete t2;
    return res;
  }
};

#endif  // TREAP_H_