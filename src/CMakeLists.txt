add_library(cpppromise
  cpppromise.cc
  timer.cc)

enable_testing()

add_executable(cpppromise_test
  cpppromise_test.cc)
target_link_libraries(cpppromise_test
  cpppromise
  GTest::gtest_main)

include(GoogleTest)

gtest_discover_tests(cpppromise_test)
