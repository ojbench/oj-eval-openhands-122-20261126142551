#ifndef MEMO_HPP
#define MEMO_HPP

#include "event.h"
#include <cstdio>

// Event type enum for fast dispatch
enum EventType {
    TYPE_NORMAL,
    TYPE_NOTIFY_BEFORE,
    TYPE_NOTIFY_LATE
};

// Node in the linked list representing a scheduled notification
struct Node {
    const Event* event;
    int next_time;
    EventType type;
    Node* next;
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
    head_ = nullptr;
  }

  // 析构函数，需保证没有内存泄漏
  ~Memo() {
    Node* cur = head_;
    while (cur != nullptr) {
        Node* tmp = cur;
        cur = cur->next;
        delete tmp;
    }
    head_ = nullptr;
  }

  // Helper: remove a specific node from the list
  void RemoveNode(Node* target) {
    Node* p = head_;
    Node* pp = nullptr;
    while (p != nullptr && p != target) {
        pp = p;
        p = p->next;
    }
    if (p == target) {
        if (pp == nullptr) {
            head_ = target->next;
        } else {
            pp->next = target->next;
        }
    }
  }

  // Helper: insert a node into sorted position by next_time
  void InsertNode(Node* node) {
    if (head_ == nullptr || head_->next_time >= node->next_time) {
        node->next = head_;
        head_ = node;
    } else {
        Node* ip = head_;
        while (ip->next != nullptr && ip->next->next_time < node->next_time) {
            ip = ip->next;
        }
        node->next = ip->next;
        ip->next = node;
    }
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
    newNode->next_time = add_time;
    newNode->type = etype;
    newNode->next = nullptr;
    InsertNode(newNode);
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

  // 模拟时间流逝，进行下一个小时的事件提醒
  void Tick() {
    current_hour_++;
    if (current_hour_ > duration_) return;
    
    int hour = current_hour_;
    
    // Pass 1: count nodes at current hour and remove outdated nodes
    int count_at_hour = 0;
    Node* prev = nullptr;
    Node* cur = head_;
    while (cur != nullptr) {
        if (cur->next_time < hour) {
            Node* tmp = cur;
            if (prev == nullptr) {
                head_ = cur->next;
            } else {
                prev->next = cur->next;
            }
            cur = cur->next;
            delete tmp;
            continue;
        }
        
        if (cur->next_time > hour) {
            break;
        }
        
        count_at_hour++;
        prev = cur;
        cur = cur->next;
    }
    
    // Pass 2: process each node at the current hour
    for (int i = 0; i < count_at_hour; i++) {
        Node* node = nullptr;
        Node* p = head_;
        while (p != nullptr && p->next_time != hour) {
            p = p->next;
        }
        if (p == nullptr) break;
        node = p;
        
        const Event* event = node->event;
        
        if (event->IsComplete()) {
            RemoveNode(node);
            delete node;
            i--;
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
            RemoveNode(node);
            delete node;
            i--;
        } else if (move_to_time > 0) {
            RemoveNode(node);
            node->next_time = move_to_time;
            node->next = nullptr;
            InsertNode(node);
            i--;
        }
    }
  }

 private:
    Node* head_;
    int duration_;
    int current_hour_;
};

#endif
