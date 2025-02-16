#ifndef MPSC_HPP
#define MPSC_HPP

#include <concepts>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

class mpsc {

  explicit mpsc() = delete;

  template <typename T> struct Data {
    std::queue<T> queue;
    std::mutex lock;
    std::condition_variable cond;

    explicit Data() {}
  };

public:
  template <typename T> class Send {
    friend class mpsc;

    std::weak_ptr<Data<T>> data;

    Send(const std::shared_ptr<Data<T>> &data) : data(data) {}

  public:
    bool send(std::convertible_to<T> auto &&res) {
      using U = decltype(res);
      if (data.expired()) {
        return false;
      }
      Data<T> &data = *(this->data.lock());
      std::unique_lock temp{data.lock};
      data.queue.push(std::forward<U>(res));
      data.cond.notify_one();
      return true;
    }
  };

  template <typename T> class Recv {
    friend class mpsc;
    std::shared_ptr<Data<T>> data;

    Recv() = delete;

    Recv(std::convertible_to<std::shared_ptr<Data<T>>> auto &&data)
        : data(std::forward<decltype(data)>(data)) {}

  public:
    Recv(const Recv<T> &other) = delete;
    Recv(Recv<T> &&other) : data(std::move(other.data)) {}
    Recv<T> &operator=(Recv<T> &&other) {
      data = std::move(other.data);
      return *this;
    }

    void close() { data.reset(); }

    Send<T> get_new_send() { return Send<T>{data}; }

    std::optional<T> recv() {
      if (!data) {
        return std::nullopt;
      }
      std::unique_lock lock{data->lock};
      data->cond.wait(lock, [this]() { return !data->queue.empty(); });
      T res = std::move(data->queue.front());
      data->queue.pop();
      return {std::move(res)};
    }

    std::optional<T> try_recv() {
      if (!data) {
        return std::nullopt;
      }
      std::unique_lock lock{data->lock};
      if (data->queue.empty()) {
        return std::nullopt;
      }
      T res = std::move(data->queue.front());
      data->queue.pop();
      return std::optional<T>(std::move(res));
    }

    std::vector<T> flush() {
      std::vector<T> res;
      if (!data) {
        return res;
      }
      std::unique_lock lock{data->lock};
      data->cond.wait(lock, [this]() { return !data->queue.empty(); });
      while (!data->queue.empty()) {
        res.push_back(std::move(data->queue.front()));
        data->queue.pop();
      }
      return res;
    }

    std::vector<T> flush_no_block() {
      std::vector<T> res;
      if (!data) {
        return res;
      }
      std::unique_lock lock{data->lock};
      while (!data->queue.empty()) {
        res.push_back(std::move(data->queue.front()));
        data->queue.pop();
      }
      return res;
    }
  };

  template <typename T> static std::pair<Send<T>, Recv<T>> make() {
    std::shared_ptr<Data<T>> data = std::make_shared<Data<T>>();
    Send<T> send{data};
    Recv<T> recv{std::move(data)};
    return {std::move(send), std::move(recv)};
  }
};

#endif
