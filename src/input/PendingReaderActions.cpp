#include "PendingReaderActions.h"

namespace {
void setIfEmpty(PendingActionEntry& entry, PendingReaderAction action, uint32_t at) {
  if (entry.action == PendingReaderAction::None) entry = PendingActionEntry(action, at);
}
}

bool PendingReaderActions::enqueue(PendingReaderAction action, uint32_t queuedAtUs) {
  switch (action) {
    case PendingReaderAction::None: return false;
    case PendingReaderAction::BackToLibrary:
      clear(); back_ = PendingActionEntry(action, queuedAtUs); return true;
    case PendingReaderAction::OpenMenu:
      menu_ = PendingActionEntry(action, queuedAtUs);
      next_ = {}; nextCount_ = 0;
      return true;
    case PendingReaderAction::PreviousPage:
      previous_ = PendingActionEntry(action, queuedAtUs);
      next_ = {}; nextCount_ = 0;
      return true;
    case PendingReaderAction::IncreaseFont:
    case PendingReaderAction::DecreaseFont:
      font_ = PendingActionEntry(action, queuedAtUs);
      next_ = {}; nextCount_ = 0;
      return true;
    case PendingReaderAction::NextPage:
      if (menu_.action != PendingReaderAction::None ||
          previous_.action != PendingReaderAction::None ||
          font_.action != PendingReaderAction::None || nextCount_ >= kMaxNextActions)
        return false;
      setIfEmpty(next_, action, queuedAtUs);
      ++nextCount_;
      return true;
  }
  return false;
}

PendingActionEntry PendingReaderActions::pop() {
  PendingActionEntry result;
  if (back_.action != PendingReaderAction::None) { result = back_; back_ = {}; }
  else if (menu_.action != PendingReaderAction::None) { result = menu_; menu_ = {}; }
  else if (previous_.action != PendingReaderAction::None) { result = previous_; previous_ = {}; }
  else if (font_.action != PendingReaderAction::None) { result = font_; font_ = {}; }
  else if (nextCount_ != 0) {
    result = next_;
    if (--nextCount_ == 0) next_ = {};
  }
  return result;
}

void PendingReaderActions::clear() {
  back_ = menu_ = previous_ = font_ = next_ = {};
  nextCount_ = 0;
}

bool PendingReaderActions::empty() const {
  return back_.action == PendingReaderAction::None &&
         menu_.action == PendingReaderAction::None &&
         previous_.action == PendingReaderAction::None &&
         font_.action == PendingReaderAction::None && nextCount_ == 0;
}

const char* pendingReaderActionName(PendingReaderAction action) {
  switch (action) {
    case PendingReaderAction::None: return "none";
    case PendingReaderAction::OpenMenu: return "open_menu";
    case PendingReaderAction::PreviousPage: return "previous";
    case PendingReaderAction::NextPage: return "next";
    case PendingReaderAction::IncreaseFont: return "increase_font";
    case PendingReaderAction::DecreaseFont: return "decrease_font";
    case PendingReaderAction::BackToLibrary: return "back_to_library";
  }
  return "unknown";
}
