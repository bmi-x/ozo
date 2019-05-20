#include <ozo/result.h>
#include <ozo/io/recv.h>
#include <ozo/ext/boost.h>
#include <ozo/ext/std.h>
#include <ozo/io/array.h>
#include <ozo/io/composite.h>
#include <ozo/shortcuts.h>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace {

using namespace ::testing;

auto execute_query(const char* query_text, int binary = 1) {
    using scoped_connection = std::unique_ptr<PGconn, void(*)(PGconn*)>;
    auto connection = scoped_connection(PQconnectdb(OZO_PG_TEST_CONNINFO), PQfinish);
    EXPECT_TRUE(connection != nullptr);

    ozo::native_result_handle result(PQexecParams(connection.get(),
                        query_text,
                        0,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        binary));

    EXPECT_EQ(PGRES_TUPLES_OK, PQresultStatus(result.get())) << PQresultErrorMessage(result.get());

    return ozo::result(std::move(result));
}

TEST(result, should_convert_into_tuple_integer_and_text) {
    auto result = execute_query("select 1::int4, '2';");
    auto oid_map = ozo::empty_oid_map();
    std::vector<std::tuple<int32_t, std::string>> r;
    ozo::recv_result(result, oid_map, std::back_inserter(r));

    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(std::get<0>(r[0]), 1);
    EXPECT_EQ(std::get<1>(r[0]), "2");
}

TEST(result, should_convert_into_tuple_float_and_text) {
    auto result = execute_query("select 42.13::float4, 'text';");
    auto oid_map = ozo::empty_oid_map();
    std::vector<std::tuple<float, std::string>> r;
    ozo::recv_result(result, oid_map, std::back_inserter(r));

    ASSERT_EQ(r.size(), 1u);
    EXPECT_EQ(std::get<0>(r[0]), 42.13f);
    EXPECT_EQ(std::get<1>(r[0]), "text");
}

TEST(result, should_convert_into_tuple_with_nulls_from_nullables) {
    // boost::scoped_ptr is missing here. It is neither movable nor copyable
    // by design, therefore ozo cannot pass the row instance it constructed
    // during deserialization into back_insert_iterator::operator=.
    // TODO: test that this can be circumvented by adding a custom
    // move-assign operator to the row type, which swaps the scoped_ptr.
    using row = std::tuple<
        boost::optional<int32_t>,
#ifdef __OZO_STD_OPTIONAL
        __OZO_STD_OPTIONAL<float>,
#else
        boost::optional<float>,
#endif
        std::unique_ptr<std::string>,
        boost::shared_ptr<std::vector<char>>,
        std::shared_ptr<std::string>
    >;
    auto result = execute_query("select 7::int4, 42.13::float4, 'text', null, null;");
    auto oid_map = ozo::empty_oid_map();
    std::vector<row> r;
    ozo::recv_result(result, oid_map, std::back_inserter(r));

    ASSERT_EQ(r.size(), 1u);
    EXPECT_TRUE(std::get<0>(r[0]));
    EXPECT_EQ(*std::get<0>(r[0]), 7);
    EXPECT_TRUE(std::get<1>(r[0]));
    EXPECT_EQ(*std::get<1>(r[0]), 42.13f);
    EXPECT_TRUE(std::get<2>(r[0]));
    EXPECT_EQ(*std::get<2>(r[0]), "text");
    EXPECT_FALSE(std::get<3>(r[0]));
    EXPECT_FALSE(std::get<4>(r[0]));
}

TEST(result, for_raw_result_should_move_in_to_out) {
    auto result = execute_query("SELECT 1");
    auto oid_map = ozo::empty_oid_map();
    ozo::result out;
    const auto handle = result.handle().get();

    ozo::recv_result(result, oid_map, out);

    EXPECT_EQ(result.handle(), nullptr);
    EXPECT_EQ(out.handle().get(), handle);
}

TEST(result, for_result_and_reference_wrapper_of_result_should_move_in_to_out) {
    auto result = execute_query("SELECT 1");
    auto oid_map = ozo::empty_oid_map();
    ozo::result out;
    const auto handle = result.handle().get();

    ozo::recv_result(result, oid_map, std::ref(out));

    EXPECT_EQ(result.handle(), nullptr);
    EXPECT_EQ(out.handle().get(), handle);
}

TEST(result, should_convert_in_rows_of_tuple_rows_of_records) {
    auto result = execute_query("SELECT * FROM (VALUES ((1, 'one'::text)), ((2, 'two'::text)), ((3, 'three'::text))) AS t (tuple);");
    auto oid_map = ozo::empty_oid_map();

    ozo::rows_of<std::tuple<int, std::string>> out;

    ozo::recv_result(result, oid_map, ozo::into(out));

    EXPECT_THAT(out, ElementsAre(
        std::make_tuple(std::make_tuple(int(1), "one")),
        std::make_tuple(std::make_tuple(int(2), "two")),
        std::make_tuple(std::make_tuple(int(3), "three"))
    ));
}

} // namespace
