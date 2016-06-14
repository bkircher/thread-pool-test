#pragma once

#include <tuple>
#include <utility>

#include <stddef.h>

namespace foo
{
    namespace internal
    {
        template <size_t N> struct apply_helper
        {
            template <typename Function, typename Tuple, typename... Args>
            static auto apply(Function&& func, Tuple&& tuple, Args&&... args)
                -> decltype(auto)
            {
                return apply_helper<N - 1>::apply(
                    std::forward<Function>(func), std::forward<Tuple>(tuple),
                    std::get<N - 1>(std::forward<Tuple>(tuple)),
                    std::forward<Args>(args)...);
            }
        };

        template <> struct apply_helper<0>
        {
            template <typename Function, typename Tuple, typename... Args>
            static auto apply(Function&& func, Tuple&&, Args&&... args)
                -> decltype(auto)
            {
                return std::forward<Function>(func)(
                    std::forward<Args>(args)...);
            }
        };
    }

    // Apply a tuple to a function.
    //
    // Example usage:
    //
    //    void f(int i, bool b)
    //    {
    //        std::cout << "f(" << i << ", " << std::boolalpha << b << ")\n";
    //    }
    //
    //    int main()
    //    {
    //        auto tuple = std::make_tuple(20, false);
    //        apply(&f, tuple);
    //        return 0;
    //    }
    //
    template <typename Function, typename Tuple>
    inline auto apply(Function&& func, Tuple&& tuple) -> decltype(auto)
    {
        return internal::apply_helper<
            std::tuple_size<typename std::decay<Tuple>::type>::value>::
            apply(std::forward<Function>(func), std::forward<Tuple>(tuple));
    }
}

// vim:et ts=4 sw=4 noic cc=80
