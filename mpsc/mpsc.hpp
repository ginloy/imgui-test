#ifndef MPSC_HPP
#define MPSC_HPP

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>

class mpsc {

  explicit mpsc() = delete;

  template <typename T> struct Data {
    std::atomic<bool> closed;
    std::queue<T> queue;
    std::mutex lock;
    std::counting_semaphore<> sem;

    explicit Data()
        : closed(false), queue(std::queue<T>{}), lock(std::mutex{}),
          sem(std::counting_semaphore<>{0}) {}
  };

public:
  template <typename T> class Send {
    friend class mpsc;

    std::shared_ptr<Data<T>> data;

    Send() = delete;
    Send(const std::shared_ptr<Data<T>> &data) : data(data) {}

  public:
    template <typename U>
    bool send(U &&res) {
      std::unique_lock temp{data->lock};
      if (data->closed) {
        return false;
      }
      data->queue.push(std::forward<U>(res));
      data->sem.release();
      return true;
    }
  };


  template <typename T> class Recv {
    friend class mpsc;
    std::shared_ptr<Data<T>> data;

    Recv() = delete;
    Recv(const Recv<T> &other) = delete;

    Recv(std::shared_ptr<Data<T>> &data) : data(data) {}

  public:
    Recv(Recv<T> &&other) : data(other.data) { other.data.reset(); }
    ~Recv() {
      if (data != nullptr) {
        std::unique_lock temp{data->lock};
        data->closed = true;
      }
    }

    void close() {
      std::unique_lock temp{data->lock};
      data->closed = true;
    }

    Send<T> get_new_send() { return Send<T>{data}; }

    std::optional<T> recv() {
      data->sem.acquire();
      std::unique_lock temp{data->lock};
      T res = std::move(data->queue.front());
      data->queue.pop();
      return {std::move(res)};
    }

    std::optional<T> try_recv() {
      if (data->sem.try_acquire()) {
        std::unique_lock temp{data->lock};
        T res = std::move(data->queue.front());
        data->queue.pop();
        return std::optional<T>(std::move(res));
      } else {
        return std::nullopt;
      }
    }

    std::vector<T> flush() {
      std::vector<T> res;
      std::unique_lock temp{data->lock};
      while (data->sem.try_acquire()) {
        res.push_back(std::move(data->queue.front()));
        data->queue.pop();
      }
      return res;
    }
  };

  template <typename T> static std::pair<Send<T>, Recv<T>> make() {
    std::shared_ptr<Data<T>> data = std::make_shared<Data<T>>();
    auto send = Send<T>{data};
    auto recv = Recv<T>{data};
    return {send, std::move(recv)};
  }
};

#endif
