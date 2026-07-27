#pragma once

#include <cstdint>

enum class PersistReason : uint8_t {
  IdleTimeout,
  PageThreshold,
  MaximumInterval,
  BackToLibrary,
  EnterSleep,
  FontChanged,
  RestartBook,
  CloseBook,
  ExplicitRequest
};

struct PersistState {
  bool dirty = false;
  uint32_t dirtySinceMs = 0;
  uint32_t lastSaveMs = 0;
  uint16_t pagesSinceSave = 0;
};

class PersistPolicy {
 public:
  PersistPolicy(uint32_t idleDelayMs = 15000, uint32_t maximumIntervalMs = 60000,
                uint16_t pageThreshold = 5)
      : idleDelayMs_(idleDelayMs), maximumIntervalMs_(maximumIntervalMs),
        pageThreshold_(pageThreshold) {}
  void markDirty(uint32_t nowMs, bool pageChanged = true);
  bool shouldSave(uint32_t nowMs, bool displayBusy, bool interactiveWorkPending,
                  PersistReason& reason) const;
  void recordSaved(uint32_t nowMs);
  const PersistState& state() const { return state_; }

 private:
  PersistState state_;
  uint32_t idleDelayMs_;
  uint32_t maximumIntervalMs_;
  uint16_t pageThreshold_;
};

const char* persistReasonName(PersistReason reason);
