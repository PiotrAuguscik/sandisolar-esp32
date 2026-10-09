#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>
#include <vector>
#include <cstring>
struct TestQueue {
  std::mutex mutex;
  std::condition_variable available;
  std::deque<std::vector<unsigned char>> items;
  unsigned capacity, bytes;
  TestQueue(unsigned c,unsigned b):capacity(c),bytes(b) {}
};
using QueueHandle_t=TestQueue*;
inline QueueHandle_t xQueueCreate(unsigned c,unsigned b) { return new TestQueue(c,b); }
inline int xQueueSend(QueueHandle_t q,const void* value,unsigned) {
  std::lock_guard<std::mutex> lock(q->mutex);
  if (q->items.size()>=q->capacity) return 0;
  const auto* data=static_cast<const unsigned char*>(value);
  q->items.emplace_back(data,data+q->bytes); q->available.notify_one(); return 1;
}
inline int xQueueReceive(QueueHandle_t q,void* value,unsigned timeout) {
  std::unique_lock<std::mutex> lock(q->mutex);
  if (!q->available.wait_for(lock,std::chrono::milliseconds(timeout),[q]{return !q->items.empty();})) return 0;
  std::memcpy(value,q->items.front().data(),q->bytes); q->items.pop_front(); return 1;
}
inline void xQueueReset(QueueHandle_t q) {
  std::lock_guard<std::mutex> lock(q->mutex); q->items.clear();
}
