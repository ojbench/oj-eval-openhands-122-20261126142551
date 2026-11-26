#ifndef MEMO_HPP
#define MEMO_HPP

#include "event.h"
#include <vector>
#include <memory>
#include <iostream>
#include <functional>

inline std::string CustomNotifyLateEvent::GetNotification(int n) const {
    // Step 1: Call base class GetNotification(n)
    std::string base = NotifyLateEvent::GetNotification(n);
    // Step 2: Call the generator function
    std::string custom = generator_(n);
    // Step 3: Concatenate both strings
    return base + custom;
}

class Memo {
 public:
  // 显示删除默认构造函数
  Memo() = delete;

  // 构造函数，参数duration表示需要模拟第1~duration小时的备忘录
  Memo(int duration) : duration_(duration), current_hour_(0) {
    // Allocate event storage
    events_.reserve(1024);
  }

  // 析构函数，需保证没有内存泄漏
  ~Memo() {
    // Events are externally owned, do not delete them
    events_.clear();
  }

  // 向备忘录中加入一项事件。传入一个Event指针，需根据实际派生类类型进行具体操作
  void AddEvent(const Event *event) {
    if (event == nullptr) return;
    
    EventEntry entry;
    entry.event = event;
    entry.notify_count_ = 0;
    
    // Determine max notification count based on event type
    if (dynamic_cast<const NormalEvent*>(event) != nullptr) {
      entry.max_notify = 1; // Only n=0
    } else if (dynamic_cast<const NotifyBeforeEvent*>(event) != nullptr) {
      entry.max_notify = 2; // n=0 and n=1
    } else if (dynamic_cast<const NotifyLateEvent*>(event) != nullptr) {
      entry.max_notify = -1; // Unlimited (negative means unlimited)
    } else {
      entry.max_notify = 1;
    }
    
    events_.push_back(entry);
  }

  // 模拟时间流逝，进行下一个小时的事件提醒
  void Tick() {
    current_hour_++;
    if (current_hour_ > duration_) return;
    
    int hour = current_hour_;
    
    for (auto& entry : events_) {
      const Event* event = entry.event;
      
      // Skip completed events
      if (event->IsComplete()) continue;
      
      // Try to notify based on event type
      bool notified = false;
      
      if (const auto* ne = dynamic_cast<const NormalEvent*>(event)) {
        // NormalEvent: notify at deadline with n=0
        if (hour == ne->GetDeadline() && entry.notify_count_ < 1) {
          std::cout << ne->GetNotification(0) << std::endl;
          entry.notify_count_++;
          notified = true;
        }
      } else if (const auto* nbe = dynamic_cast<const NotifyBeforeEvent*>(event)) {
        // NotifyBeforeEvent: n=0 at deadline-notify_time+1, n=1 at deadline
        int notify_hour = nbe->GetDeadline() - nbe->GetNotifyTime() + 1;
        
        if (hour == notify_hour && entry.notify_count_ < 1) {
          std::cout << nbe->GetNotification(0) << std::endl;
          entry.notify_count_++;
          notified = true;
        }
        if (hour == nbe->GetDeadline() && entry.notify_count_ < 2) {
          std::cout << nbe->GetNotification(1) << std::endl;
          entry.notify_count_++;
          notified = true;
        }
      } else if (const auto* nle = dynamic_cast<const NotifyLateEvent*>(event)) {
        // NotifyLateEvent: n=0 at deadline, n=k at deadline+k*frequency
        if (hour >= nle->GetDeadline()) {
          int elapsed = hour - nle->GetDeadline();
          int freq = nle->GetFrequency();
          
          if (freq > 0 && elapsed % freq == 0) {
            int n = elapsed / freq;
            if (n >= entry.notify_count_) {
              std::cout << nle->GetNotification(n) << std::endl;
              entry.notify_count_ = n + 1;
              notified = true;
            }
          }
        }
      }
      
      (void)notified; // suppress unused warning
    }
  }

 private:
  struct EventEntry {
    const Event* event = nullptr;
    int notify_count_ = 0;
    int max_notify = -1; // -1 means unlimited
  };
  
  std::vector<EventEntry> events_;
  int duration_;
  int current_hour_;
};

#endif
