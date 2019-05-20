#include <ozo/type_traits.h>
#include <ozo/io/size_of.h>
#include <ozo/ext/boost.h>
#include <ozo/ext/std.h>
#include <ozo/core/recursive.h>

#include <boost/make_shared.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#include <boost/fusion/adapted/struct/define_struct.hpp>
#include <boost/fusion/include/define_struct.hpp>

#include <boost/hana/adapt_adt.hpp>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

namespace hana = boost::hana;

using namespace testing;

TEST(is_null, should_return_true_for_non_initialized_optional) {
    EXPECT_TRUE(ozo::is_null(boost::optional<int>{}));
}

TEST(is_null, should_return_false_for_initialized_optional) {
    EXPECT_TRUE(!ozo::is_null(boost::optional<int>{0}));
}

TEST(is_null, should_return_false_for_valid_std_weak_ptr) {
    auto ptr = std::make_shared<int>();
    EXPECT_TRUE(!ozo::is_null(std::weak_ptr<int>{ptr}));
}

TEST(is_null, should_return_true_for_expired_std_weak_ptr) {
    std::weak_ptr<int> ptr;
    {
        ptr = std::make_shared<int>();
    }
    EXPECT_TRUE(ozo::is_null(ptr));
}

TEST(is_null, should_return_true_for_non_initialized_std_weak_ptr) {
    std::weak_ptr<int> ptr;
    EXPECT_TRUE(ozo::is_null(ptr));
}


TEST(is_null, should_return_false_for_valid_boost_weak_ptr)  {
    auto ptr = boost::make_shared<int>();
    EXPECT_TRUE(!ozo::is_null(boost::weak_ptr<int>(ptr)));
}

TEST(is_null, should_return_true_for_expired_boost_weak_ptr)  {
    boost::weak_ptr<int> ptr;
    {
        ptr = boost::make_shared<int>();
    }
    EXPECT_TRUE(ozo::is_null(ptr));
}

TEST(is_null, should_return_true_for_non_initialized_boost_weak_ptr)  {
    boost::weak_ptr<int> ptr;
    EXPECT_TRUE(ozo::is_null(ptr));
}

TEST(is_null, should_return_false_for_non_nullable_type) {
    EXPECT_TRUE(!ozo::is_null(int()));
}

TEST(is_null_recursive, should_return_true_if_nested_nullable_type_is_null) {
    boost::optional<boost::optional<std::shared_ptr<int>>> obj{{{nullptr}}};
    EXPECT_FALSE(ozo::is_null(obj));
    EXPECT_TRUE(ozo::is_null_recursive(obj));
}

TEST(is_null_recursive, should_return_false_if_nested_nullable_types_contains_no_null) {
    boost::optional<boost::optional<std::shared_ptr<int>>> obj{{{std::make_shared<int>(0)}}};
    EXPECT_FALSE(ozo::is_null(obj));
    EXPECT_FALSE(ozo::is_null_recursive(obj));
}

TEST(is_null_recursive, should_return_false_for_non_nullable_type) {
    EXPECT_FALSE(ozo::is_null_recursive(int()));
}

namespace test_unwrap_local {
struct test_type {};

void unwrap(test_type){}
}

TEST(unwrap_type, should_unwrap_type_with_no_adl) {
    using type = test_unwrap_local::test_type;
    unwrap(type{});
    const bool is_same = std::is_same_v<ozo::unwrap_type<type>, type>;
    EXPECT_TRUE(is_same);
}

TEST(unwrap, should_unwrap_type) {
    boost::optional<int> n(7);
    EXPECT_EQ(ozo::unwrap(n), 7);
}

TEST(unwrap, should_unwrap_notnullable_type) {
    int n(7);
    EXPECT_EQ(ozo::unwrap(n), 7);
}

TEST(unwrap, should_unwrap_std_reference_wrapper) {
    int n(7);
    EXPECT_EQ(ozo::unwrap(std::ref(n)), 7);
}

TEST(unwrap_recursive, should_unwrap_type) {
    boost::optional<int> n(7);
    EXPECT_EQ(ozo::unwrap_recursive(n), 7);
}

TEST(unwrap_recursive, should_unwrap_type_recursively) {
    boost::optional<boost::optional<int>> n{{7}};
    EXPECT_EQ(ozo::unwrap_recursive(n), 7);
}

TEST(unwrap_recursive, should_unwrap_type_recursively_different_types) {
    boost::optional<std::shared_ptr<int>> n{std::make_shared<int>(7)};
    EXPECT_EQ(ozo::unwrap_recursive(n), 7);
}

TEST(unwrap_recursive, should_unwrap_notnullable_type) {
    int n(7);
    EXPECT_EQ(ozo::unwrap_recursive(n), 7);
}


}

namespace ozo {
namespace tests {

struct nullable_mock {
    MOCK_METHOD0(emplace, void());
    MOCK_CONST_METHOD0(negate, bool());
    MOCK_METHOD0(reset, void());
    bool operator!() const { return negate(); }
};

struct nullable {
    nullable_mock* mock_ = nullptr;
    void emplace() { mock_->emplace();}
    bool negate() const { return mock_->negate(); }
    void reset() { mock_->reset(); }
    bool operator!() const { return negate(); }

    nullable& operator = (const nullable& other) {
        if (!other.mock_ && mock_) {
            reset();
        }
        mock_ = other.mock_;
        return *this;
    }
};

}// namespace tests

template <>
struct is_nullable<StrictMock<tests::nullable_mock>> : std::true_type {};

template <>
struct is_nullable<tests::nullable_mock> : std::true_type {};

template <>
struct is_nullable<tests::nullable> : std::true_type {};

} // namespace ozo

namespace {

using namespace ozo::tests;

TEST(init_nullable, should_initialize_uninitialized_nullable) {
    StrictMock<nullable_mock> mock{};
    EXPECT_CALL(mock, negate()).WillOnce(Return(true));
    EXPECT_CALL(mock, emplace()).WillOnce(Return());
    ozo::init_nullable(mock);
}

TEST(init_nullable, should_pass_initialized_nullable) {
    StrictMock<nullable_mock> mock{};
    EXPECT_CALL(mock, negate()).WillOnce(Return(false));
    ozo::init_nullable(mock);
}

TEST(init_nullable, should_allocate_std_unique_ptr) {
    std::unique_ptr<int> ptr{};
    ozo::init_nullable(ptr);
    EXPECT_TRUE(ptr);
}

TEST(init_nullable, should_allocate_std_shared_ptr) {
    std::shared_ptr<int> ptr{};
    ozo::init_nullable(ptr);
    EXPECT_TRUE(ptr);
}

TEST(init_nullable, should_allocate_boost_scoped_ptr) {
    boost::scoped_ptr<int> ptr{};
    ozo::init_nullable(ptr);
    EXPECT_TRUE(ptr);
}

TEST(init_nullable, should_allocate_boost_shared_ptr) {
    boost::shared_ptr<int> ptr{};
    ozo::init_nullable(ptr);
    EXPECT_TRUE(ptr);
}

TEST(reset_nullable, should_reset_nullable) {
    StrictMock<nullable_mock> mock{};
    EXPECT_CALL(mock, reset()).WillOnce(Return());
    auto v = nullable{&mock};
    ozo::reset_nullable(v);
}

}// namespace

namespace ozo::tests {

struct some_type {
    std::size_t size() const {
        return 1000;
    }
    bool empty() const { return size() == 0; }
    decltype(auto) begin() const { return std::addressof(v_);}
    char v_;
};

struct builtin_type { std::int64_t v = 0; };

} // namespace ozo::tests

OZO_PG_DEFINE_CUSTOM_TYPE(some_type, "some_type")
OZO_PG_DEFINE_TYPE(builtin_type, "builtin_type", 5, bytes<8>)

BOOST_FUSION_DEFINE_STRUCT(
    (ozo)(tests), fusion_adapted,
    (std::string, name)
    (int, age))

namespace {

using namespace ozo::tests;

TEST(type_name, should_return_type_name_object) {
    using namespace std::string_literals;
    EXPECT_EQ(ozo::type_name(some_type{}), "some_type"s);
}

TEST(size_of, should_return_size_from_traits_for_static_size_type) {
    EXPECT_EQ(ozo::size_of(builtin_type{}), 8u);
}

TEST(size_of, should_return_size_from_method_size_for_dynamic_size_objects) {
    EXPECT_EQ(ozo::size_of(some_type{}), 1000u);
}

TEST(type_oid, should_return_oid_from_traits_for_buildin_type) {
    const auto oid_map = ozo::register_types<>();
    EXPECT_EQ(ozo::type_oid(oid_map, builtin_type{}), 5u);
}

TEST(type_oid, should_return_oid_from_oid_map_for_custom_type) {
    auto oid_map = ozo::register_types<some_type>();
    const auto val = some_type{};
    ozo::set_type_oid<some_type>(oid_map, 333);
    EXPECT_EQ(ozo::type_oid(oid_map, val), 333u);
}

struct accepts_oid : Test {
    decltype(ozo::register_types<some_type>()) oid_map = ozo::register_types<some_type>();
};

TEST_F(accepts_oid, should_return_true_for_type_with_oid_in_map_and_same_oid_argument) {
    const auto val = some_type{};
    ozo::set_type_oid<some_type>(oid_map, 222);
    EXPECT_TRUE(ozo::accepts_oid(oid_map, val, 222));
}

TEST_F(accepts_oid, should_return_false_for_type_with_oid_in_map_and_different_oid_argument) {
    const auto val = some_type{};
    ozo::set_type_oid<some_type>(oid_map, 222);
    EXPECT_TRUE(!ozo::accepts_oid(oid_map, val, 0));
}

} // namespace
