# Part of Beast

GroupSources(extras/beast extras)
GroupSources(include/beast beast)

GroupSources(example/http-crawl "/")

add_executable (http-crawl
    ${BEAST_INCLUDES}
    ${EXTRAS_INCLUDES}
    urls_large_data.hpp
    urls_large_data.cpp
    http_crawl.cpp
)

if (NOT WIN32)
    target_link_libraries(http-crawl ${Boost_LIBRARIES} Threads::Threads)
else()
    target_link_libraries(http-crawl ${Boost_LIBRARIES})
endif()
