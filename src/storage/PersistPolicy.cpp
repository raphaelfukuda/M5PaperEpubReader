#include "PersistPolicy.h"

void PersistPolicy::markDirty(uint32_t nowMs, bool pageChanged) {
  if (!state_.dirty) state_.dirtySinceMs = nowMs;
  state_.dirty = true;
  if (pageChanged && state_.pagesSinceSave != UINT16_MAX)
    ++state_.pagesSinceSave;
}

bool PersistPolicy::shouldSave(uint32_t nowMs, bool displayBusy,
                               bool interactiveWorkPending,
                               PersistReason& reason) const {
  if (!state_.dirty || displayBusy || interactiveWorkPending) return false;
  if (state_.pagesSinceSave >= pageThreshold_) {
    reason = PersistReason::PageThreshold;
    return true;
  }
  if (state_.lastSaveMs != 0 &&
      nowMs - state_.lastSaveMs >= maximumIntervalMs_) {
    reason = PersistReason::MaximumInterval;
    return true;
  }
  if (nowMs - state_.dirtySinceMs >= idleDelayMs_) {
    reason = PersistReason::IdleTimeout;
    return true;
  }
  return false;
}

void PersistPolicy::recordSaved(uint32_t nowMs) {
  state_.dirty = false;
  state_.lastSaveMs = nowMs;
  state_.pagesSinceSave = 0;
}

const char* persistReasonName(PersistReason reason) {
  switch (reason) {
    case PersistReason::IdleTimeout: return "idle_timeout";
    case PersistReason::PageThreshold: return "page_threshold";
    case PersistReason::MaximumInterval: return "maximum_interval";
    case PersistReason::BackToLibrary: return "back_to_library";
    case PersistReason::EnterSleep: return "enter_sleep";
    case PersistReason::FontChanged: return "font_changed";
    case PersistReason::RestartBook: return "restart_book";
    case PersistReason::CloseBook: return "close_book";
    case PersistReason::ExplicitRequest: return "explicit_request";
  }
  return "unknown";
}
