#pragma once

#include <cstdint>

enum class PendingReaderAction : uint8_t {
  None, OpenMenu, PreviousPage, NextPage, IncreaseFont, DecreaseFont, BackToLibrary
};

struct PendingActionEntry {
  PendingActionEntry() = default;
  PendingActionEntry(PendingReaderAction pendingAction, uint32_t timestampUs)
      : action(pendingAction), queuedAtUs(timestampUs) {}
  PendingReaderAction action = PendingReaderAction::None;
  uint32_t queuedAtUs = 0;
};

class PendingReaderActions {
 public:
  static constexpr uint8_t kMaxNextActions = 3;
  bool enqueue(PendingReaderAction action, uint32_t queuedAtUs);
  PendingActionEntry pop();
  void clear();
  bool empty() const;
  uint8_t pendingNextCount() const { return nextCount_; }

 private:
  PendingActionEntry back_;
  PendingActionEntry menu_;
  PendingActionEntry previous_;
  PendingActionEntry font_;
  PendingActionEntry next_;
  uint8_t nextCount_ = 0;
};

const char* pendingReaderActionName(PendingReaderAction action);
