#include <Arduino.h>
#include "app/AppController.h"

namespace { AppController app; }

void setup() { app.begin(); }
void loop() { app.tick(); }

