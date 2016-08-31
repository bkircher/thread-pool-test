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
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "apply.hpp"

namespace foo
{
    // Exception indicating that the current thread has been interrupted
    class thread_interrupted final : public std::exception
    {
    };

    namespace internal
    {
        class interrupt_flag
        {
        public:
            interrupt_flag() : flag_(false), thread_cond_(nullptr), mutex_() {}

            void set()
            {
                flag_.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> guard(mutex_);
                if (thread_cond_)
                {
                    thread_cond_->notify_all();
                }
            }

            bool is_set() const
            {
                return flag_.load(std::memory_order_relaxed);
            }

            void set_condition_variable(std::condition_variable& cond)
            {
                std::lock_guard<std::mutex> guard(mutex_);
                thread_cond_ = std::addressof(cond);
            }

            void clear_condition_variable()
            {
                std::lock_guard<std::mutex> guard(mutex_);
                thread_cond_ = nullptr;
            }

        private:
            std::atomic<bool> flag_;
            std::condition_variable* thread_cond_;
            std::mutex mutex_;
        };

        thread_local interrupt_flag this_thread_interrupt_flag;

        struct clear_condition_variable_on_destruct
        {
            ~clear_condition_variable_on_destruct()
            {
                this_thread_interrupt_flag.clear_condition_variable();
            }
        };
    }

    class interruptible_thread
    {
    public:
        // Construct with no thread
        interruptible_thread() : internal_thread_(), flag_(nullptr) {}

        // Construct with func(args...)
        template <typename Function, typename... Args>
        explicit interruptible_thread(Function&& func, Args&&... args)
            : interruptible_thread()
        {
            struct wrapper final
            {
                Function wrapped_function;
                std::tuple<Args...> arguments;

                explicit wrapper(Function&& f, Args&&... as)
                    : wrapped_function(std::forward<Function>(f)),
                      arguments(std::forward<Args>(as)...)
                {
                }

                void operator()(internal::interrupt_flag** flag_ptr,
                                std::mutex* m, std::condition_variable* c)
                {
                    {
                        std::lock_guard<std::mutex> guard(*m);
                        *flag_ptr = std::addressof(
                            internal::this_thread_interrupt_flag);
                    }
                    c->notify_one();
                    try
                    {
                        apply(wrapped_function, arguments);
                    }
                    catch (thread_interrupted&)
                    {
                        // Thread was interrupted
                    }
                }
            };

            std::mutex flag_mutex;
            std::condition_variable flag_condition;
            internal_thread_ =
                std::thread(wrapper(std::forward<Function>(func),
                                    std::forward<Args>(args)...),
                            &flag_, &flag_mutex, &flag_condition);
            std::unique_lock<std::mutex> lock(flag_mutex);
            while (!flag_)
            {
                flag_condition.wait(lock);
            }
        }

        interruptible_thread(const interruptible_thread&) = delete;
        interruptible_thread& operator=(const interruptible_thread&) = delete;

        // Swap with other
        void swap(interruptible_thread& other) noexcept
        {
            using std::swap;
            swap(flag_, other.flag_);
            swap(internal_thread_, other.internal_thread_);
        }

        // Move-construct from other
        interruptible_thread(interruptible_thread&& other) noexcept
            : internal_thread_(std::move(other.internal_thread_)),
              flag_(other.flag_)
        {
            other.flag_ = nullptr;
        }

        // Move-assign from rhs
        interruptible_thread& operator=(interruptible_thread&& rhs) noexcept
        {
            using std::swap;
            swap(flag_, rhs.flag_);
            swap(internal_thread_, rhs.internal_thread_);
            return *this;
        }

        // Return true if this thread can be joined
        bool joinable() const noexcept { return internal_thread_.joinable(); }

        // Terminate thread
        void join() { internal_thread_.join(); }

        // Detach thread
        void detach() { internal_thread_.detach(); }

        std::thread::id get_id() const noexcept
        {
            return internal_thread_.get_id();
        }

        // On Windows: Win32 HANDLE as void *; on Linux: some representation
        // of pthread_t
        std::thread::native_handle_type native_handle()
        {
            return internal_thread_.native_handle();
        }

        // Interrupt this thread at next possible moment
        void interrupt()
        {
            if (flag_)
            {
                flag_->set();
            }
        }

    private:
        std::thread internal_thread_;
        internal::interrupt_flag* flag_;
    };

    static_assert(std::is_default_constructible<interruptible_thread>::value,
                  "");
    static_assert(!std::is_copy_constructible<interruptible_thread>::value, "");
    static_assert(!std::is_copy_assignable<interruptible_thread>::value, "");
    static_assert(std::is_move_constructible<interruptible_thread>::value, "");
    static_assert(std::is_move_assignable<interruptible_thread>::value, "");

    // Checks whether this thread has been interrupted
    //
    // Call this function at a point in your code where it is safe to be
    // interrupted; throws a thread_interrupted exception if the flag is set,
    // does nothing otherwise
    //
    // Example usage:
    //
    //    void foo()
    //    {
    //        while (!done)
    //        {
    //            interruption_point();
    //            do_some_more_work();
    //        }
    //    }
    //
    void interruption_point()
    {
        if (internal::this_thread_interrupt_flag.is_set())
        {
            throw thread_interrupted();
        }
    }

    // Lets you wait on a condition variable in an interruptible way
    template <typename Predicate>
    inline void interruptible_wait(std::condition_variable& cond,
                                   std::unique_lock<std::mutex>& lock,
                                   Predicate pred)
    {
        interruption_point();
        internal::this_thread_interrupt_flag.set_condition_variable(cond);
        internal::clear_condition_variable_on_destruct guard;
        while (!internal::this_thread_interrupt_flag.is_set() && !pred())
        {
            cond.wait_for(lock, std::chrono::milliseconds(1));
        }
        interruption_point();
    }

    inline void interruptible_wait(std::condition_variable& cond,
                                   std::unique_lock<std::mutex>& lock)
    {
        interruption_point();
        internal::this_thread_interrupt_flag.set_condition_variable(cond);
        internal::clear_condition_variable_on_destruct guard;
        interruption_point();
        cond.wait_for(lock, std::chrono::milliseconds(1));
        interruption_point();
    }

    // A thread-safe queue using locks and condition variables (from C++
    // Concurrency in Action, chapter 4.1 and 6.2)
    template <typename T> class locked_queue final
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

        // Try to get a value from the queue; returns immediately, indicating
        // whether there was a value retrieved or not
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

        // Wait until there is a value to get from this queue
        //
        // Blocks until there is a value or the current thread was interrupted
        void wait_and_pop(value_type& val)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            interruptible_wait(cond_, lock, [this] { return !data_.empty(); });
            val = data_.front();
            data_.pop();
        }

        locked_queue(const locked_queue&) = delete;
        locked_queue& operator=(const locked_queue&) = delete;

    private:
        mutable std::mutex mutex_;
        std::queue<value_type> data_;
        std::condition_variable cond_;
    };

    static_assert(std::is_default_constructible<locked_queue<int>>::value, "");
    static_assert(!std::is_copy_constructible<locked_queue<int>>::value, "");
    static_assert(!std::is_copy_assignable<locked_queue<int>>::value, "");
    static_assert(!std::is_move_constructible<locked_queue<int>>::value, "");
    static_assert(!std::is_move_assignable<locked_queue<int>>::value, "");

    // A very simple thread pool. Slightly adapted from C++ Concurrency in
    // Action, chapter 9.
    class thread_pool final
    {
    public:
        typedef interruptible_thread thread_type;

        thread_pool() : done_(false), tasks_(), workers_(), joiner_(workers_)
        {
            // const unsigned int
            // hwthreads = std::thread::hardware_concurrency();
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
                            tasks_.wait_and_pop(task);
                            task();
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

        // Join all worker threads and clean-up
        ~thread_pool()
        {
            done_ = true;

            for (auto& thread : workers_)
            {
                thread.interrupt();
            }

            // joiner_ will take care of joining
        }

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

        thread_pool(const thread_pool&) = delete;
        thread_pool& operator=(const thread_pool&) = delete;

    private:
        // Ensures we join our worker threads at scope exit.
        class join_threads
        {
        public:
            explicit join_threads(std::vector<thread_type>& threads)
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
            std::vector<thread_type>& threads_;
        };

        std::atomic<bool> done_;
        locked_queue<std::function<void()>> tasks_;
        std::vector<thread_type> workers_;
        join_threads joiner_;
    };

    static_assert(std::is_default_constructible<thread_pool>::value, "");
    static_assert(!std::is_copy_constructible<thread_pool>::value, "");
    static_assert(!std::is_copy_assignable<thread_pool>::value, "");
    static_assert(!std::is_move_constructible<thread_pool>::value, "");
    static_assert(!std::is_move_assignable<thread_pool>::value, "");

    // Returns whether f has a result; doesn't block
    template <typename T> bool is_ready(const std::future<T>& f)
    {
        return f.valid() &&
               f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
}

// vim:et ts=4 sw=4 noic cc=80
