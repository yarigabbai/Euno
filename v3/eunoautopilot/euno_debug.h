#pragma once
#include <Arduino.h>

// Versione "safe" all'avvio: stampa solo su Serial, nessuna coda/WS
inline void debugLog(const String& s){ Serial.println(s); }
inline void debugLog(const char* s){ Serial.println(s); }

