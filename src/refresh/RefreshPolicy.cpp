#include "RefreshPolicy.h"

#include <limits>

namespace {
uint16_t increment(uint16_t value) {
  return value == std::numeric_limits<uint16_t>::max() ? value : value + 1;
}
}

void GhostingBudget::record(RefreshProfile profile, bool partial) {
  if (profile == RefreshProfile::Fast) fastRefreshes = increment(fastRefreshes);
  if (profile == RefreshProfile::Fastest) fastestRefreshes = increment(fastestRefreshes);
  if (profile == RefreshProfile::Fast || profile == RefreshProfile::Fastest ||
      profile == RefreshProfile::Text)
    readingRefreshes = increment(readingRefreshes);
  if (partial) partialRefreshes = increment(partialRefreshes);
}

void GhostingBudget::resetAfterQuality(uint32_t nowMs) {
  fastRefreshes = fastestRefreshes = partialRefreshes = readingRefreshes = 0;
  lastQualityRefreshMs = nowMs;
  if (historicalCleanups != std::numeric_limits<uint32_t>::max()) ++historicalCleanups;
}

const char* refreshProfileName(RefreshProfile profile) {
  switch (profile) {
    case RefreshProfile::Quality: return "quality";
    case RefreshProfile::Text: return "text";
    case RefreshProfile::Fast: return "fast";
    case RefreshProfile::Fastest: return "fastest";
    case RefreshProfile::Adaptive: return "adaptive";
  }
  return "unknown";
}

const char* refreshReasonName(RefreshReason reason) {
  switch (reason) {
    case RefreshReason::Boot: return "boot";
    case RefreshReason::OpenBook: return "open_book";
    case RefreshReason::NormalPageTurn: return "normal_page_turn";
    case RefreshReason::RapidPageTurn: return "rapid_page_turn";
    case RefreshReason::PreviousPage: return "previous_page";
    case RefreshReason::MenuOpen: return "menu_open";
    case RefreshReason::MenuClose: return "menu_close";
    case RefreshReason::BrowserNavigation: return "browser_navigation";
    case RefreshReason::TouchFeedback: return "touch_feedback";
    case RefreshReason::FontReflow: return "font_reflow";
    case RefreshReason::TocNavigation: return "toc_navigation";
    case RefreshReason::WakeFromSleep: return "wake_from_sleep";
    case RefreshReason::GhostingCleanup: return "ghosting_cleanup";
    case RefreshReason::ManualCleanup: return "manual_cleanup";
    case RefreshReason::ErrorScreen: return "error_screen";
  }
  return "unknown";
}

RefreshDecision RefreshPolicy::decide(const RefreshRequest& request) const {
  RefreshDecision decision;
  decision.reason = request.reason;
  decision.partial = !request.fullScreen;
  decision.submittedAtMs = request.nowMs;
  switch (request.reason) {
    case RefreshReason::Boot:
    case RefreshReason::WakeFromSleep:
    case RefreshReason::ManualCleanup:
    case RefreshReason::GhostingCleanup:
    case RefreshReason::ErrorScreen:
      decision.requested = RefreshProfile::Quality; break;
    case RefreshReason::OpenBook:
    case RefreshReason::FontReflow:
    case RefreshReason::TocNavigation:
      decision.requested = RefreshProfile::Text; break;
    case RefreshReason::RapidPageTurn:
      decision.requested = request.consecutiveRapidTurns >= config_.rapidTurnsBeforeFastest
                               ? RefreshProfile::Fastest : RefreshProfile::Fast;
      break;
    default: decision.requested = RefreshProfile::Fast; break;
  }
  decision.effective = decision.requested;

  const bool readingMode = decision.effective == RefreshProfile::Fast ||
                           decision.effective == RefreshProfile::Fastest;
  if (!readingMode || request.reason == RefreshReason::TouchFeedback ||
      request.reason == RefreshReason::MenuOpen || request.reason == RefreshReason::MenuClose)
    return decision;

  const char* cause = nullptr;
  if (decision.effective == RefreshProfile::Fastest &&
      (budget_.fastestRefreshes + 1 >= config_.maxConsecutiveFastestRefreshes))
    cause = "fastest_limit";
  else if (decision.effective == RefreshProfile::Fast &&
           (budget_.fastRefreshes + 1 >= config_.maxFastRefreshesBeforeCleanup))
    cause = "fast_limit";
  else if (budget_.readingRefreshes + 1 >= config_.maxReadingRefreshesBeforeQuality)
    cause = "reading_limit";
  else if (budget_.lastQualityRefreshMs != 0 &&
           request.nowMs - budget_.lastQualityRefreshMs >=
               config_.maxMillisecondsWithoutQualityRefresh)
    cause = "quality_timeout";
  else if (request.changedAreaRatio >= config_.largeContentChangeRatio)
    cause = nullptr;  // Full reading pages are expected; counters govern cleanup.

  if (cause) {
    decision.effective = RefreshProfile::Quality;
    decision.reason = RefreshReason::GhostingCleanup;
    decision.cleanupForced = true;
    decision.cleanupCause = cause;
  }
  return decision;
}

void RefreshPolicy::recordSubmitted(const RefreshDecision& decision) {
  if (decision.effective == RefreshProfile::Quality)
    budget_.resetAfterQuality(decision.submittedAtMs);
  else
    budget_.record(decision.effective, decision.partial);
}

void RefreshPolicy::recordQualityCompleted(uint32_t nowMs) {
  budget_.resetAfterQuality(nowMs);
}

void RefreshPolicy::reset() { budget_ = {}; }

uint16_t RefreshPolicy::saturatedIncrement(uint16_t value) { return increment(value); }
