#include "SpiBusGuard.h"

bool SpiBusGuard::tryAcquire(SpiBusOwner owner) {
  if (owner == SpiBusOwner::None || owner_ != SpiBusOwner::None) return false;
  owner_ = owner;
  return true;
}

void SpiBusGuard::release(SpiBusOwner owner) {
  if (owner_ == owner) owner_ = SpiBusOwner::None;
}

