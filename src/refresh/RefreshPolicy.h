#pragma once

#include <cstdint>

enum class RefreshProfile : uint8_t { Quality, Text, Fast, Fastest, Adaptive };

enum class RefreshReason : uint8_t {
  Boot, OpenBook, NormalPageTurn, RapidPageTurn, PreviousPage, MenuOpen,
  MenuClose, BrowserNavigation, TouchFeedback, FontReflow, TocNavigation,
  WakeFromSleep, GhostingCleanup, ManualCleanup, ErrorScreen
};

struct RefreshRequest {
  RefreshReason reason = RefreshReason::NormalPageTurn;
  bool fullScreen = true;
  float changedAreaRatio = 1.0f;
  uint32_t millisecondsSincePreviousTurn = 0;
  uint8_t consecutiveRapidTurns = 0;
  uint32_t nowMs = 0;
};

struct GhostingBudget {
  uint16_t fastRefreshes = 0;
  uint16_t fastestRefreshes = 0;
  uint16_t partialRefreshes = 0;
  uint16_t readingRefreshes = 0;
  uint32_t lastQualityRefreshMs = 0;
  uint32_t historicalCleanups = 0;

  void record(RefreshProfile profile, bool partial);
  void resetAfterQuality(uint32_t nowMs);
};

struct RefreshPolicyConfig {
  uint32_t rapidPageTurnWindowMs = 900;
  uint8_t rapidTurnsBeforeFastest = 2;
  uint8_t maxConsecutiveFastestRefreshes = 4;
  uint8_t maxFastRefreshesBeforeCleanup = 8;
  uint8_t maxReadingRefreshesBeforeQuality = 10;
  uint32_t maxMillisecondsWithoutQualityRefresh = 10UL * 60UL * 1000UL;
  float largeContentChangeRatio = 0.85f;
};

struct RefreshDecision {
  RefreshProfile requested = RefreshProfile::Adaptive;
  RefreshProfile effective = RefreshProfile::Text;
  RefreshReason reason = RefreshReason::NormalPageTurn;
  bool cleanupForced = false;
  const char* cleanupCause = "none";
  bool partial = false;
  uint32_t submittedAtMs = 0;
};

const char* refreshProfileName(RefreshProfile profile);
const char* refreshReasonName(RefreshReason reason);

class RefreshPolicy {
 public:
  explicit RefreshPolicy(RefreshPolicyConfig config = {}) : config_(config) {}
  RefreshDecision decide(const RefreshRequest& request) const;
  void recordSubmitted(const RefreshDecision& decision);
  void recordQualityCompleted(uint32_t nowMs);
  void reset();
  const GhostingBudget& budget() const { return budget_; }

 private:
  static uint16_t saturatedIncrement(uint16_t value);
  RefreshPolicyConfig config_;
  GhostingBudget budget_;
};
