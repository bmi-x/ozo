#include <ozo/detail/deadline.h>
#include "../test_asio.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

using namespace testing;
using namespace std::literals;

using time_point = ozo::time_traits::time_point;
using duration = ozo::time_traits::duration;

struct deadline_handler : Test {
    ozo::tests::executor_gmock executor;
    ozo::tests::executor_gmock strand;
    ozo::tests::strand_executor_service_gmock strand_service;
    ozo::tests::steady_timer_gmock timer;
    ozo::tests::steady_timer_service_mock timer_service;
    ozo::tests::execution_context io{executor, strand_service, timer_service};
    StrictMock<ozo::tests::callback_gmock<>> on_deadline;
    StrictMock<ozo::tests::callback_gmock<>> continuation;

    deadline_handler() {
        EXPECT_CALL(timer_service, timer(An<time_point>())).WillRepeatedly(ReturnRef(timer));
        EXPECT_CALL(strand_service, get_executor()).WillOnce(ReturnRef(strand));
    }
};

TEST_F(deadline_handler, should_call_timeout_handler_on_timeout) {
    ozo::tests::executor_gmock on_deadline_executor;
    EXPECT_CALL(timer, async_wait(_)).WillOnce(InvokeArgument<0>(ozo::error_code{}));
    EXPECT_CALL(strand, post(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(on_deadline, get_executor())
        .WillOnce(Return(ozo::tests::executor{on_deadline_executor, io}));
    EXPECT_CALL(on_deadline_executor, dispatch(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(on_deadline, call(_));
    using ozo::tests::wrap;
    ozo::detail::deadline_handler(io.get_executor(), time_point{}, wrap(continuation), wrap(on_deadline));
}

TEST_F(deadline_handler, should_not_call_timeout_handler_on_timer_cancel) {
    EXPECT_CALL(timer, async_wait(_)).WillOnce(InvokeArgument<0>(boost::asio::error::operation_aborted));
    EXPECT_CALL(strand, post(_)).WillOnce(InvokeArgument<0>());
    using ozo::tests::wrap;
    ozo::detail::deadline_handler(io.get_executor(), time_point{}, wrap(continuation), wrap(on_deadline));
}

TEST_F(deadline_handler, should_cancel_timer_and_call_continuation) {
    ozo::tests::executor_gmock continuation_executor;
    EXPECT_CALL(timer, async_wait(_));
    EXPECT_CALL(timer, cancel());
    EXPECT_CALL(continuation, get_executor())
        .WillOnce(Return(ozo::tests::executor{continuation_executor, io}));
    EXPECT_CALL(continuation_executor, dispatch(_)).WillOnce(InvokeArgument<0>());
    EXPECT_CALL(continuation, call(_));
    using ozo::tests::wrap;
    auto h = ozo::detail::deadline_handler(io.get_executor(), time_point{}, wrap(continuation), wrap(on_deadline));
    h(ozo::error_code{});
}

} // namespace
