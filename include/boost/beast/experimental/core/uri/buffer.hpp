// Copyright (c) 2018 oneiric (oneiric at i2pmail dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_URI_BUFFER_HPP
#define BOOST_BEAST_CORE_URI_BUFFER_HPP

#include <boost/beast/core/string.hpp>
#include <boost/beast/experimental/core/uri/error.hpp>
#include <boost/beast/experimental/core/uri/parts.hpp>
#include <vector>

namespace boost {
namespace beast {
namespace uri   {

class buffer : public parts {
  std::vector<char> data_;

public:
  using const_iterator = std::vector<char>::const_iterator;
  using value_type = std::vector<char>::value_type;
  using size_type = std::vector<char>::size_type;

  std::vector<char> &data() { return data_; }

  value_type back() const noexcept { return data_.back(); }

  const_iterator begin() const noexcept { return data_.begin(); }

  const_iterator end() const noexcept { return data_.end(); }

  void push_back(char c) { data_.push_back(c); }

  size_type size() const noexcept { return data_.size(); }

  std::string part_from(const_iterator first, const_iterator last) {
    BOOST_ASSERT(first >= data_.begin() && first < data_.end());
    BOOST_ASSERT(last > data_.begin() && last <= data_.end());
    BOOST_ASSERT(first < last);

    return {first, last};
  }

  string_view uri() const noexcept { return {data_.data(), data_.size()}; }

  boost::string_view update_uri()
  {
    const std::string auth_start = scheme_ == "file" ? ":///" : "://";
    const boost::string_view new_uri =
        scheme_ + auth_start
        + (username_.empty() ? "" : user_info().to_string() + "@") + host_
        + (port_.empty() ? "" : ":" + port_) + path_
        + (query_.empty() ? "" : "?" + query_)
        + (fragment().empty() ? "" : "#" + fragment_);
    data_.clear();
    data_.insert(data_.begin(), new_uri.begin(), new_uri.end());
    return new_uri;
  }

  void clear()
  {
    data_.clear();
    reset();
  }
};

} // namespace uri
} // namespace beast
} // namespace boost

#endif  // BOOST_BEAST_CORE_URI_BUFFER_HPP
