#pragma once

#include <cstdint>

enum class PrefetchState : uint8_t {
  Idle, ReadingInput, Parsing, LayingOut, Ready, CancelRequested, Restoring, Failed
};

enum class PrefetchCancelResult : uint8_t {
  NotActive, Cancelled, PreservedAsCache, DeferredUntilSafePoint, Failed
};

class PrefetchStateMachine {
 public:
  bool start();
  bool transition(PrefetchState next);
  PrefetchCancelResult requestCancel();
  void restored(bool success);
  void reset() { state_ = PrefetchState::Idle; }
  PrefetchState state() const { return state_; }
  bool hasWork() const;
  bool ready() const { return state_ == PrefetchState::Ready; }

 private:
  PrefetchState state_ = PrefetchState::Idle;
};
