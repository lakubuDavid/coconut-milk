#ifndef CORE_MESSAGE_QUEUE_H
#define CORE_MESSAGE_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace coconut::core {
template <class T> class MessageQueue {
private:
  std::queue<T> _Queue;
  std::mutex _Mtx;
  std::condition_variable _Cv;
  bool _Stopped;

public:
  void push(T msg) {
    {
      std::lock_guard<std::mutex> lock(this->_Mtx);
      if (_Stopped)
        return;

      _Queue.push(std::move(msg));
    }
    _Cv.notify_one();
  }
  std::optional<T> pop() {
    std::lock_guard<std::mutex> lock(this->_Mtx);

    if (_Queue.empty()) {
      return std::nullopt;
    }

    T msg = std::move(_Queue.front());
    _Queue.pop();

    return msg;
  }
  std::optional<T> waitAndPop() {
    std::unique_lock<std::mutex> lock(this->_Mtx);
    _Cv.wait(lock, [this] { return _Stopped || !_Queue.empty(); });

    if (_Queue.empty()) {
      return std::nullopt;
    }

    T msg = std::move(_Queue.front());
    _Queue.pop();

    return msg;
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lock(_Mtx);
      _Stopped = true;
    }

    // Wake every thread waiting in waitAndPop().
    _Cv.notify_all();
  }

  bool stopped()const{
    std::lock_guard<std::mutex> lock(_Mtx);
    return _Stopped;
  }
};
}; // namespace coconut::core

#endif
