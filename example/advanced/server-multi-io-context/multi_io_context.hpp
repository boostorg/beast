//
// Copyright (c) 2018 Brett Robinson (octo dot banana93 at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: Advanced server, one io_context per thread
//
//------------------------------------------------------------------------------

#ifndef MULTI_IO_CONTEXT_HPP
#define MULTI_IO_CONTEXT_HPP

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <deque>
#include <vector>

class multi_io_context
{
    class io_context
    {
        boost::asio::io_context ioc_;
        boost::asio::executor_work_guard<
            boost::asio::io_context::executor_type> work_;

    public:

        explicit
        io_context(int concurrency_hint = -1)
            : ioc_{concurrency_hint}
            , work_{boost::asio::make_work_guard(ioc_)}
        {
        }

        io_context(io_context const&) = delete;
        io_context& operator=(io_context const&) = delete;
        io_context(io_context&&) = default;
        io_context& operator=(io_context&&) = default;
        ~io_context() = default;

        boost::asio::io_context&
        get()
        {
            return ioc_;
        }
    };

    int const concurrency_hint_{1};
    std::size_t index_{0};
    std::deque<io_context> io_contexts_;
    std::vector<boost::asio::io_context*> ptr_io_contexts_;

public:

    explicit
    multi_io_context(std::size_t num_contexts = 1)
    {
        num_contexts = std::max<std::size_t>(1, num_contexts);
        for(std::size_t i = 0; i < num_contexts; ++i)
        {
            io_contexts_.emplace_back(concurrency_hint_);
            ptr_io_contexts_.emplace_back(&io_contexts_.back().get());
        }
    }

    multi_io_context(multi_io_context const&) = delete;
    multi_io_context& operator=(multi_io_context const&) = delete;
    multi_io_context(multi_io_context&&) = default;
    multi_io_context& operator=(multi_io_context&&) = default;
    ~multi_io_context() = default;

    std::size_t
    size() const
    {
        return io_contexts_.size();
    }

    // Returns a vector of non-owning pointers to the io_contexts
    std::vector<boost::asio::io_context*> const
    contexts()
    {
        return ptr_io_contexts_;
    }

    // Returns the current io_context, without advancing the index
    boost::asio::io_context&
    get_context()
    {
        return io_contexts_.at(index_).get();
    }

    // Advances the index, then returns an io_context
    boost::asio::io_context&
    next_context()
    {
        if(++index_ == io_contexts_.size())
            index_ = 0;
        return io_contexts_.at(index_).get();
    }

    // Returns the current io_context, and advances the index
    boost::asio::io_context&
    bump_context()
    {
        auto& ioc = io_contexts_.at(index_).get();
        if(++index_ == io_contexts_.size())
            index_ = 0;
        return ioc;
    }
};

#endif
