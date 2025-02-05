#include "mpsc.hpp"

#include <chrono>
#include <future>
#include <gtest/gtest.h>

TEST(MpscTest, General) {
  auto [send, recv] = mpsc::make<int>();
  std::thread producer([send]() mutable {
    ASSERT_TRUE(send.send(42));
    ASSERT_TRUE(send.send(43));
  });

  std::thread consumer([send, recv = std::move(recv)]() mutable {
    EXPECT_EQ(recv.recv(), std::optional<int>{42});
    EXPECT_EQ(recv.recv(), std::optional<int>{43});
    auto fut =
        std::async(std::launch::async, [recv = std::move(recv)]() mutable {
          EXPECT_EQ(recv.recv(), std::optional<int>{69});
        });
    auto status = fut.wait_for(std::chrono::milliseconds(1000));
    EXPECT_EQ(status, std::future_status::timeout)
        << "Channel failed to block on empty queue";
    ASSERT_TRUE(send.send(69));
    status = fut.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready)
        << "Channel did not unblock after queue populated";
  });

  producer.join();
  consumer.join();
}

TEST(MpscTest, NoConsumer) {

  auto [send, recv] = mpsc::make<int>();
  ASSERT_TRUE(send.send(1));
  std::thread temp([recv = std::move(recv)]() mutable {

  });

  temp.join();
  ASSERT_FALSE(send.send(1));
}

TEST(MpscTest, TryRecv) {
  auto [send, recv] = mpsc::make<int>();

  ASSERT_EQ(std::nullopt, recv.try_recv());
  ASSERT_TRUE(send.send(32));
  ASSERT_EQ(recv.try_recv(), std::optional{32});
}

TEST(MpscTest, MultipleProducers) {
  auto [send, recv] = mpsc::make<int>();
  std::vector<std::thread> producers;
  for (int i = 0; i < 1000; ++i) {
    producers.push_back(std::thread{[send]() mutable { send.send(32); }});
  }

  int count = 0;
  for (int i = 0; i < 1000; ++i) {
    count += recv.recv().value();
  }
  ASSERT_EQ(count, 32 * 1000);
  for (auto &t: producers) {
    t.join();
  }
}

//------------------------------------------------------------------------------
// 1) Basic Single-Producer Single-Consumer
//------------------------------------------------------------------------------

TEST(MpscTest, SingleProducerSingleConsumer) {
    auto [sender, receiver] = mpsc::make<int>();

    // Check that try_recv() returns empty initially
    auto opt = receiver.try_recv();
    EXPECT_FALSE(opt.has_value());

    // Send one item
    bool success = sender.send(42);
    EXPECT_TRUE(success);

    // Now receive it (blocking or try_recv)
    opt = receiver.try_recv();
    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt.value(), 42);

    // Queue should now be empty again
    auto opt2 = receiver.try_recv();
    EXPECT_FALSE(opt2.has_value());
}


//------------------------------------------------------------------------------
// 2) Multiple Producers Single Consumer
//------------------------------------------------------------------------------

TEST(MpscTest, MultipleProducers2) {
    // Create an initial producer/consumer pair
    auto [sender1, receiver] = mpsc::make<int>();

    // Create additional producer from receiver
    auto sender2 = receiver.get_new_send();

    // We'll spawn two threads, each sending a range of integers
    std::thread t1([&]() {
        for (int i = 0; i < 50; ++i) {
            bool ok = sender1.send(i);
            EXPECT_TRUE(ok);
        }
    });

    std::thread t2([&]() {
        for (int i = 100; i < 150; ++i) {
            bool ok = sender2.send(i);
            EXPECT_TRUE(ok);
        }
    });

    // Collect results in the main thread
    std::vector<int> results;
    results.reserve(100);

    // We'll read exactly 100 messages (50 from each sender)
    for (int i = 0; i < 100; ++i) {
        auto valOpt = receiver.recv();  // blocking
        ASSERT_TRUE(valOpt.has_value());
        results.push_back(valOpt.value());
    }

    t1.join();
    t2.join();

    // The order of arrival is not guaranteed across threads,
    // but we expect all 100 unique integers from [0..49] and [100..149].
    // Let's do a simple check: ensure the size is 100, and that
    // it contains the expected sets of numbers.
    EXPECT_EQ(results.size(), 100u);

    // We'll do a simple presence check
    std::vector<bool> gotLow(50, false);
    std::vector<bool> gotHigh(50, false);

    for (auto n : results) {
        if (n >= 0 && n < 50) {
            gotLow[n] = true;
        } else if (n >= 100 && n < 150) {
            gotHigh[n - 100] = true;
        }
    }

    for (bool b : gotLow) {
        EXPECT_TRUE(b) << "Missing a number in range [0..49]";
    }
    for (bool b : gotHigh) {
        EXPECT_TRUE(b) << "Missing a number in range [100..149]";
    }
}


//------------------------------------------------------------------------------
// 3) Blocking vs. try_recv
//------------------------------------------------------------------------------

TEST(MpscTest, TryRecvVsBlocking) {
    auto [sender, receiver] = mpsc::make<int>();

    // Initially, queue is empty, so try_recv() should fail
    auto maybeVal = receiver.try_recv();
    EXPECT_FALSE(maybeVal.has_value());

    // Let's do a blocking recv in a separate thread
    std::atomic<bool> started{false};
    std::atomic<bool> finished{false};

    std::thread consumer([&]() {
        started.store(true, std::memory_order_release);
        auto valOpt = receiver.recv();  // should block until we send
        finished.store(true, std::memory_order_release);
        ASSERT_TRUE(valOpt.has_value());
        EXPECT_EQ(valOpt.value(), 123);
    });

    // Wait until the consumer is definitely blocking
    while (!started.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(finished.load(std::memory_order_acquire))
        << "recv() should still be blocked!";

    // Now send a value so the consumer can unblock
    bool ok = sender.send(123);
    EXPECT_TRUE(ok);

    consumer.join();
}


//------------------------------------------------------------------------------
// 4) Close Behavior
//------------------------------------------------------------------------------

TEST(MpscTest, CloseChannel) {
    auto [sender, receiver] = mpsc::make<int>();

    // Send a few items
    EXPECT_TRUE(sender.send(1));
    EXPECT_TRUE(sender.send(2));
    EXPECT_TRUE(sender.send(3));

    // Close from consumer side
    receiver.close();

    // Now, further sends should fail
    EXPECT_FALSE(sender.send(999));

    // But we can still receive the items that were already in the queue
    auto v1 = receiver.try_recv();
    auto v2 = receiver.try_recv();
    auto v3 = receiver.try_recv();
    auto v4 = receiver.try_recv();  // No more items
    EXPECT_TRUE(v1.has_value());
    EXPECT_TRUE(v2.has_value());
    EXPECT_TRUE(v3.has_value());
    EXPECT_FALSE(v4.has_value());
    EXPECT_EQ(v1.value(), 1);
    EXPECT_EQ(v2.value(), 2);
    EXPECT_EQ(v3.value(), 3);
}


//------------------------------------------------------------------------------
// 5) Flush Behavior
//------------------------------------------------------------------------------

TEST(MpscTest, FlushAllItems) {
    auto [sender, receiver] = mpsc::make<int>();

    // Send multiple items
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(sender.send(i));
    }

    // Flush them all at once
    auto items = receiver.flush();
    ASSERT_EQ(items.size(), 10u);

    // We expect the items in FIFO order: [0..9]
    // but let's just do a quick check
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(items[i], i);
    }

    // Queue should now be empty
    auto emptyCheck = receiver.try_recv();
    EXPECT_FALSE(emptyCheck.has_value());
}
