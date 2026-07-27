#include "PrefetchStateMachine.h"

bool PrefetchStateMachine::start() {
  if (state_ != PrefetchState::Idle && state_ != PrefetchState::Ready) return false;
  state_ = PrefetchState::ReadingInput;
  return true;
}

bool PrefetchStateMachine::transition(PrefetchState next) {
  bool valid = false;
  switch (state_) {
    case PrefetchState::ReadingInput:
      valid = next == PrefetchState::Parsing || next == PrefetchState::Ready ||
              next == PrefetchState::CancelRequested || next == PrefetchState::Failed;
      break;
    case PrefetchState::Parsing:
      valid = next == PrefetchState::LayingOut || next == PrefetchState::CancelRequested ||
              next == PrefetchState::Failed;
      break;
    case PrefetchState::LayingOut:
      valid = next == PrefetchState::ReadingInput || next == PrefetchState::Ready ||
              next == PrefetchState::CancelRequested || next == PrefetchState::Failed;
      break;
    case PrefetchState::CancelRequested:
      valid = next == PrefetchState::Restoring || next == PrefetchState::Failed;
      break;
    case PrefetchState::Restoring:
      valid = next == PrefetchState::Idle || next == PrefetchState::Failed;
      break;
    default: break;
  }
  if (valid) state_ = next;
  return valid;
}

PrefetchCancelResult PrefetchStateMachine::requestCancel() {
  if (state_ == PrefetchState::Idle) return PrefetchCancelResult::NotActive;
  if (state_ == PrefetchState::Ready) return PrefetchCancelResult::PreservedAsCache;
  if (state_ == PrefetchState::Failed) return PrefetchCancelResult::Failed;
  if (state_ == PrefetchState::CancelRequested || state_ == PrefetchState::Restoring)
    return PrefetchCancelResult::DeferredUntilSafePoint;
  state_ = PrefetchState::CancelRequested;
  return PrefetchCancelResult::DeferredUntilSafePoint;
}

void PrefetchStateMachine::restored(bool success) {
  state_ = success ? PrefetchState::Idle : PrefetchState::Failed;
}

bool PrefetchStateMachine::hasWork() const {
  return state_ != PrefetchState::Idle && state_ != PrefetchState::Ready &&
         state_ != PrefetchState::Failed;
}
