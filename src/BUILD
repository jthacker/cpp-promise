package(default_visibility = ["//visibility:public"])

cc_library(
    name = "timer",
    srcs = [
        "timer.cc",
    ],
    hdrs = [
        "timer.h",
    ],
)

cc_test(
    name = "timer_test",
    srcs = ["timer_test.cc"],
    deps = [
        "//src/cpp_common/cpppromise:timer",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "cpppromise",
    srcs = [
        "empty.cc",
        "event_queue.cc",
        "lifecycle_listener_manager.cc",
        "process.cc",
        "schedule.cc",
        "schedule_cancel_trigger.cc",
        "schedule_control_block.cc",
        "timer.cc",
    ],
    hdrs = [
        "cpppromise.h",
        "cpppromise_stream.h",
        "empty.h",
        "event_listener.h",
        "event_queue.h",
        "event_queue_impl.h",
        "event_queue_listener.h",
        "lifecycle_listener.h",
        "lifecycle_listener_manager.h",
        "non_csp_utils.h",
        "process.h",
        "process_impl.h",
        "promise.h",
        "promise_control_block.h",
        "promise_control_block_impl.h",
        "promise_impl.h",
        "promise_listener.h",
        "publication.h",
        "publication_impl.h",
        "resolver.h",
        "resolver_impl.h",
        "schedule.h",
        "schedule_cancel_trigger.h",
        "schedule_control_block.h",
        "subscription.h",
        "subscription_control_block.h",
        "subscription_impl.h",
        "subscription_unsubscribe_trigger.h",
        "subscription_unsubscribe_trigger_impl.h",
        "timer.h",
        "topic.h",
        "topic_impl.h",
    ],
    deps = [
        "//src/cpp_common/cpppromise:timer",
    ],
)

cc_binary(
    name = "cpppromise_stream_demo",
    srcs = ["cpppromise_stream_demo_main.cpp"],
    linkopts = ["-lpthread"],
    deps = ["cpppromise"],
)

cc_binary(
    name = "cpppromise_demo",
    srcs = ["cpppromise_demo_main.cpp"],
    linkopts = ["-lpthread"],
    deps = ["cpppromise"],
)

cc_library(
    name = "cpppromise_test_listeners",
    testonly = 1,
    hdrs = ["customized_test_listeners.h"],
    deps = [
        "//src/cpp_common/cpppromise",
    ],
)

cc_test(
    name = "cpppromise_test",
    srcs = ["cpppromise_test.cc"],
    deps = [
        ":cpppromise_test_listeners",
        "//src/cpp_common/cpppromise",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "cpppromise_stream_test",
    srcs = ["cpppromise_stream_test.cc"],
    deps = [
        ":cpppromise_test_listeners",
        "//src/cpp_common/cpppromise",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "non_csp_utils_test",
    srcs = ["non_csp_utils_test.cc"],
    deps = [
        "//src/cpp_common/cpppromise",
        "@com_google_googletest//:gtest_main",
    ],
)
