//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_EXAMPLE_PROCESSOR_BASE_H
#define BOOST_BEAST_EXAMPLE_PROCESSOR_BASE_H

class processor_base {

public:

	enum class input_type {
		LIVE,
		HISTORIC
	};

	virtual void cancel() = 0;

	virtual ~processor_base() {}
};

#endif

