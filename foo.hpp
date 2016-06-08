#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <stack>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace foo
{
    // A thread-safe queue using locks and condition variables (from C++
    // Concurrency in Action, chapter 6.2)
    template <typename T> class locked_queue
    {
    public:
        typedef T value_type;

        locked_queue() = default;

        void push(value_type val)
        {
            std::lock_guard<std::mutex> guard(mutex_);
            data_.push(val);
            cond_.notify_one();
        }

        bool try_pop(value_type& val)
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (data_.empty())
            {
                return false;
            }
            val = data_.front();
            data_.pop();
            return true;
        }

        locked_queue(const locked_queue&) = delete;
        locked_queue& operator=(const locked_queue&) = delete;

    private:
        mutable std::mutex mutex_;
        std::queue<value_type> data_;
        std::condition_variable cond_;
    };

    // A very simple thread pool. Slightly adapted from C++ Concurrency in
    // Action, chapter 9.
    class thread_pool
    {
    public:
        thread_pool() : done_(false), tasks_(), workers_(), joiner_(workers_)
        {
            // const unsigned int
            // hwthreads{std::thread::hardware_concurrency()};
            const unsigned int threads = 8U;
            std::cout << "threads: " << threads << std::endl;

            try
            {
                for (unsigned int i = 0; i < threads; ++i)
                {
                    workers_.emplace_back([this] {
                        while (!done_)
                        {
                            std::function<void()> task;
                            if (tasks_.try_pop(task))
                            {
                                task();
                            }
                            else
                            {
                                std::this_thread::yield(); // Wait a little
                                                           // bit
                            }
                        }
                    });
                }
            }
            catch (std::exception&)
            {
                done_ = true;
                throw;
            }
        }

        ~thread_pool() { done_ = true; }

        template <typename Function, typename... Args>
        auto submit(Function&& function, Args&&... args) // URef
            -> std::future<typename std::result_of<Function(Args...)>::type>
        {
            typedef
                typename std::result_of<Function(Args...)>::type result_type;

            if (done_)
            {
                throw std::runtime_error("submit on stopped thread_pool");
            }

            auto taskptr = std::make_shared<std::packaged_task<result_type()>>(
                std::bind(std::forward<Function>(function),
                          std::forward<Args>(args)...));

            std::future<result_type> result = taskptr->get_future();
            tasks_.push([taskptr]() { (*taskptr)(); });
            return result;
        }

    private:
        // Ensures we join our worker threads at scope exit.
        class join_threads
        {
        public:
            explicit join_threads(std::vector<std::thread>& threads)
                : threads_(threads)
            {
            }

            ~join_threads()
            {
                for (auto& t : threads_)
                {
                    if (t.joinable())
                    {
                        t.join();
                    }
                }
            }

        private:
            std::vector<std::thread>& threads_;
        };

        std::atomic<bool> done_;
        locked_queue<std::function<void()>> tasks_;
        std::vector<std::thread> workers_;
        join_threads joiner_;
    };

    // Returns whether f has a result; doesn't block
    template <typename T> bool is_ready(const std::future<T>& f)
    {
        return f.valid() &&
               f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
}

// vim:et ts=4 sw=4 noic cc=80
