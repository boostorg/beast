#ifndef BOOST_BEAST_CORE_DETAIL_WORK_GUARD_HPP
#define BOOST_BEAST_CORE_DETAIL_WORK_GUARD_HPP

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/execution.hpp>

namespace boost {
namespace beast {
namespace detail {

template<class Executor, class Enable = void>
struct select_work_guard;

template<class Executor>
using select_work_guard_t = typename
    select_work_guard<Executor>::type;

#if !defined(BOOST_ASIO_NO_TS_EXECUTORS)
template<class Executor>
struct select_work_guard
<
    Executor,
    typename std::enable_if
    <
        boost::asio::is_executor<Executor>::value
    >::type
>
{
    using type = net::executor_work_guard<Executor>;
};
#endif

template<class Executor>
struct execution_work_guard
{
    using executor_type = decltype(
        net::prefer(std::declval<Executor const&>(),
            net::execution::outstanding_work.tracked));

    execution_work_guard(Executor const& exec)
    : exec_(net::prefer(exec, net::execution::outstanding_work.tracked))
    {

    }

    executor_type
    get_executor() const noexcept
    {
        return exec_;
    }

    void reset() noexcept
    {
        // no-op
    }

private:

    executor_type exec_;
};

template<class Executor>
struct select_work_guard
<
    Executor,
    typename std::enable_if
    <
        boost::asio::execution::is_executor<Executor>::value
#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
        || boost::asio::is_executor<Executor>::value
#else
        && !boost::asio::is_executor<Executor>::value
#endif
    >::type
>
{
    using type = execution_work_guard<Executor>;
};

template<class Executor>
select_work_guard_t<Executor>
make_work_guard(Executor const& exec) noexcept
{
    return select_work_guard_t<Executor>(exec);
}

}
}
}

#endif // BOOST_BEAST_CORE_DETAIL_WORK_GUARD_HPP
