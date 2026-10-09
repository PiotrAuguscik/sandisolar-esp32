#pragma once
#include <thread>
constexpr int pdTRUE=1, pdPASS=1;
inline unsigned pdMS_TO_TICKS(unsigned ms) { return ms; }
inline int xTaskCreate(void (*fn)(void*), const char*, unsigned, void* p, unsigned, void*) {
  std::thread(fn,p).detach(); return pdPASS;
}
