#pragma once

#include <stdint.h>

enum class SpiBusOwner : uint8_t { None, Display, SdCard };

class SpiBusGuard {
 public:
  bool tryAcquire(SpiBusOwner owner);
  void release(SpiBusOwner owner);
  SpiBusOwner owner() const { return owner_; }

 private:
  SpiBusOwner owner_ = SpiBusOwner::None;
};

class ScopedSpiBus {
 public:
  ScopedSpiBus(SpiBusGuard& guard, SpiBusOwner owner)
      : guard_(guard), owner_(owner), acquired_(guard_.tryAcquire(owner)) {}
  ~ScopedSpiBus() { if (acquired_) guard_.release(owner_); }
  explicit operator bool() const { return acquired_; }

 private:
  SpiBusGuard& guard_;
  SpiBusOwner owner_;
  bool acquired_;
};

