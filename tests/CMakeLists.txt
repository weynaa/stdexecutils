file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/" FILES ${TEST_SOURCES})
add_executable(${PROJECT_NAME}_test ${TEST_SOURCES})

set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)

target_link_libraries(${PROJECT_TEST_NAME} PRIVATE GTest::gtest GTest::gtest_main ${PROJECT_NAME})
gtest_discover_tests(${PROJECT_TEST_NAME})

if(BUILD_COVERAGE)
    target_link_libraries(${PROJECT_TEST_NAME} PRIVATE gcov)
    setup_target_for_coverage_lcov(
        NAME ${PROJECT_NAME}_coverage
        EXECUTABLE $<TARGET_FILE:${PROJECT_TEST_NAME}>
        SONARQUBE
    )
endif()
