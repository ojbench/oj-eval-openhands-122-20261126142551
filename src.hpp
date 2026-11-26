#ifndef MEMO_HPP
#define MEMO_HPP

#include "event.h"
#include <iostream>
#include <cstdio>

// Buffer stdout for faster output
static int memo_buf_size = 0;
static char memo_stdout_buf[1 << 20];
struct MemoIOInit {
    MemoIOInit() {
        setvbuf(stdout, memo_stdout_buf, _IOFBF, sizeof(memo_stdout_buf));
    }
} memo_io_init;

// Event type enum for fast dispatch
enum EventType {
    TYPE_NORMAL,
    TYPE_NOTIFY_BEFORE,
    TYPE_NOTIFY_LATE
};

// Node in the linked list representing a scheduled notification
struct Node {
    const Event* event;
    EventType type;
    Node* next;
};

// Time slot: holds events due at a specific time
struct TimeSlot {
    int time;
    Node* head;
    TimeSlot* next;
};

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
    slots_ = nullptr;
    current_slot_ = nullptr;
  }

  // 析构函数，需保证没有内存泄漏
  ~Memo() {
    TimeSlot* ts = slots_;
    while (ts != nullptr) {
        Node* n = ts->head;
        while (n != nullptr) {
            Node* tmp = n;
            n = n->next;
            delete tmp;
        }
        TimeSlot* tmp = ts;
        ts = ts->next;
        delete tmp;
    }
    slots_ = nullptr;
    current_slot_ = nullptr;
  }

  // Helper: find or create a time slot for the given time
  TimeSlot* GetOrCreateSlot(int time) {
    TimeSlot* prev = nullptr;
    TimeSlot* cur = slots_;
    while (cur != nullptr && cur->time < time) {
        prev = cur;
        cur = cur->next;
    }
    
    if (cur == nullptr || cur->time > time) {
        // Create new slot
        TimeSlot* newSlot = new TimeSlot;
        newSlot->time = time;
        newSlot->head = nullptr;
        newSlot->next = cur;
        if (prev == nullptr) {
            slots_ = newSlot;
        } else {
            prev->next = newSlot;
        }
        cur = newSlot;
    }
    return cur;
  }

  // Helper: remove a node from its time slot's list
  void RemoveNodeFromSlot(Node* target) {
    // Find which slot this node belongs to
    TimeSlot* ts = slots_;
    while (ts != nullptr) {
        Node* prev = nullptr;
        Node* cur = ts->head;
        while (cur != nullptr && cur != target) {
            prev = cur;
            cur = cur->next;
        }
        if (cur == target) {
            if (prev == nullptr) {
                ts->head = target->next;
            } else {
                prev->next = target->next;
            }
            return;
        }
        ts = ts->next;
    }
  }

  // Helper: add a node to a specific time slot's list (at tail)
  void AddNodeToSlot(TimeSlot* slot, Node* node) {
    node->next = nullptr;
    if (slot->head == nullptr) {
        slot->head = node;
    } else {
        Node* cur = slot->head;
        while (cur->next != nullptr) {
            cur = cur->next;
        }
        cur->next = node;
    }
  }

  // Helper: print string using printf for speed
  void PrintString(const std::string& s) {
    const char* cstr = s.c_str();
    while (*cstr) {
        fputc(*cstr, stdout);
        cstr++;
    }
    fputc('\n', stdout);
  }

  // 向备忘录中加入一项事件。传入一个Event指针，需根据实际派生类类型进行具体操作
  void AddEvent(const Event *event) {
    if (event == nullptr) return;
    
    int add_time = 0;
    EventType etype = TYPE_NORMAL;
    
    if (dynamic_cast<const NormalEvent*>(event) != nullptr) {
        add_time = event->GetDeadline();
        etype = TYPE_NORMAL;
    } else if (dynamic_cast<const NotifyBeforeEvent*>(event) != nullptr) {
        const NotifyBeforeEvent* nbe = static_cast<const NotifyBeforeEvent*>(event);
        add_time = nbe->GetDeadline() - nbe->GetNotifyTime() + 1;
        etype = TYPE_NOTIFY_BEFORE;
    } else if (dynamic_cast<const NotifyLateEvent*>(event) != nullptr) {
        add_time = event->GetDeadline();
        etype = TYPE_NOTIFY_LATE;
    }
    
    Node* newNode = new Node;
    newNode->event = event;
    newNode->type = etype;
    
    TimeSlot* slot = GetOrCreateSlot(add_time);
    AddNodeToSlot(slot, newNode);
  }

  // 模拟时间流逝，进行下一个小时的事件提醒
  void Tick() {
    current_hour_++;
    if (current_hour_ > duration_) return;
    
    int hour = current_hour_;
    
    // Advance current_slot_ to the right time slot
    if (current_slot_ == nullptr || current_slot_->time < hour) {
        current_slot_ = GetOrCreateSlot(hour);
    } else if (current_slot_->time > hour) {
        current_slot_ = GetOrCreateSlot(hour);
    }
    
    // Process all nodes in the current time slot
    Node* node = current_slot_->head;
    current_slot_->head = nullptr;
    
    while (node != nullptr) {
        Node* next_node = node->next;
        const Event* event = node->event;
        
        if (event->IsComplete()) {
            delete node;
            node = next_node;
            continue;
        }
        
        bool should_remove = false;
        int move_to_time = -1;
        
        switch (node->type) {
            case TYPE_NORMAL: {
                PrintString(event->GetNotification(0));
                should_remove = true;
                break;
            }
            case TYPE_NOTIFY_BEFORE: {
                const NotifyBeforeEvent* nbe = static_cast<const NotifyBeforeEvent*>(event);
                int notify_hour = nbe->GetDeadline() - nbe->GetNotifyTime() + 1;
                
                if (hour == notify_hour && hour < nbe->GetDeadline()) {
                    PrintString(event->GetNotification(0));
                    move_to_time = nbe->GetDeadline();
                } else if (hour == nbe->GetDeadline()) {
                    if (hour == notify_hour) {
                        PrintString(event->GetNotification(0));
                        PrintString(event->GetNotification(1));
                    } else {
                        PrintString(event->GetNotification(1));
                    }
                    should_remove = true;
                }
                break;
            }
            case TYPE_NOTIFY_LATE: {
                const NotifyLateEvent* nle = static_cast<const NotifyLateEvent*>(event);
                int elapsed = hour - nle->GetDeadline();
                int freq = nle->GetFrequency();
                int n = elapsed / freq;
                
                PrintString(event->GetNotification(n));
                move_to_time = hour + freq;
                break;
            }
        }
        
        if (should_remove) {
            delete node;
        } else if (move_to_time > 0) {
            TimeSlot* newSlot = GetOrCreateSlot(move_to_time);
            AddNodeToSlot(newSlot, node);
        }
        
        node = next_node;
    }
  }

 private:
    TimeSlot* slots_;
    TimeSlot* current_slot_;
    int duration_;
    int current_hour_;
};

#endif
