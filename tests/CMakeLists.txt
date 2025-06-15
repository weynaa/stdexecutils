add_executable(${PROJECT_NAME}_test 
    spawn_stdfuture_test.cpp 
    channel_test.cpp
)

set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)

target_link_libraries(${PROJECT_TEST_NAME} PRIVATE GTest::gtest GTest::gtest_main ${PROJECT_NAME})
gtest_add_tests(TARGET ${PROJECT_TEST_NAME})

if(BUILD_COVERAGE)
    target_link_libraries(${PROJECT_TEST_NAME} PRIVATE gcov)
    setup_target_for_coverage_lcov(
        NAME ${PROJECT_NAME}_coverage
        EXECUTABLE $<TARGET_FILE:${PROJECT_TEST_NAME}>
        SONARQUBE
    )
endif()
