#pragma once

class Logger {
 public:
  static void begin();
  static void hardwareReport(bool sdMounted, unsigned long long sdSize,
                             unsigned long sdMountMs);
};

