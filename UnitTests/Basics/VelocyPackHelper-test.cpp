////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for VelocyPackHelper.cpp
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#define BOOST_TEST_INCLUDED
#include <boost/test/unit_test.hpp>

#include <velocypack/Builder.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

#include "Basics/VelocyPackHelper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                    private macros
// -----------------------------------------------------------------------------
  
#define VPACK_CHECK(expected, func, lValue, rValue)  \
  l = VPackParser::fromJson(lValue);  \
  r = VPackParser::fromJson(rValue);  \
  BOOST_CHECK_EQUAL(expected, func(l->slice(), r->slice(), true)); \

#define INIT_BUFFER  TRI_string_buffer_t* sb = TRI_CreateStringBuffer(TRI_UNKNOWN_MEM_ZONE);
#define FREE_BUFFER  TRI_FreeStringBuffer(TRI_UNKNOWN_MEM_ZONE, sb);
#define STRINGIFY    TRI_StringifyJson(sb, json);
#define STRING_VALUE sb->_buffer
#define FREE_JSON    TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, json);

// -----------------------------------------------------------------------------
// --SECTION--                                                 setup / tear-down
// -----------------------------------------------------------------------------

struct VPackHelperSetup {
  VPackHelperSetup () {
    BOOST_TEST_MESSAGE("setup VelocyPackHelper test");
  }

  ~VPackHelperSetup () {
    BOOST_TEST_MESSAGE("tear-down VelocyPackHelper test");
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief setup
////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE(VPackHelperTest, VPackHelperSetup)

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with equal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_equal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;

  // With Utf8-mode:
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "null", "null");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "false", "false");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "true", "true");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "0", "0");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "1", "1");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.5");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "-43.2", "-43.2");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\"\"", "\"\"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\" \"", "\" \"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "\"the quick brown fox\"", "\"the quick brown fox\"");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[]", "[]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[-1]", "[-1]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[0]", "[0]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[1]", "[1]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "[true]", "[true]");
  VPACK_CHECK(0, arangodb::basics::VelocyPackHelper::compare, "{}", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test compare values with unequal values
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE (tst_compare_values_unequal) {
  std::shared_ptr<VPackBuilder> l;
  std::shared_ptr<VPackBuilder> r;
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "false");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "true");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "null", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "true");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "false", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "0");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "-10");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "{}");
  
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-2", "-1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-10", "-9");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-20", "-5");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "-5", "-2");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "true", "1");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1.5", "1.6");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "10.5", "10.51");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"0\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "\"-1\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\"-1\"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "\" \"");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[-1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "0", "{}");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[-1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[0]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[1]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[null]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[false]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "[true]");
  VPACK_CHECK(-1, arangodb::basics::VelocyPackHelper::compare, "1", "{}");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test duplicate keys
////////////////////////////////////////////////////////////////////////////////
/*
BOOST_AUTO_TEST_CASE (tst_duplicate_keys) {
  INIT_BUFFER
  
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[\"a\",\"a\"]");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"A\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"a\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":1,\"b\":1,\"c\":1,\"d\":{},\"c\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1},\"b\":1}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1,\"a\":3},\"b\":1}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":1,\"b\":1,\"a\":3}}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":{\"a\":{}}}}");
  BOOST_CHECK_EQUAL(false, TRI_HasDuplicateKeyJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{\"a\":{\"a\":{\"a\":{},\"a\":2}}}");
  BOOST_CHECK_EQUAL(true, TRI_HasDuplicateKeyJson(json));
  FREE_JSON

  FREE_BUFFER
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing
////////////////////////////////////////////////////////////////////////////////
/*
BOOST_AUTO_TEST_CASE (tst_json_hash_utf8) {
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"äöüßÄÖÜ€µ\"");
  BOOST_CHECK_EQUAL(17926322495289827824ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"코리아닷컴 메일알리미 서비스 중단안내 [안내] 개인정보취급방침 변경 안내 회사소개 | 광고안내 | 제휴안내 | 개인정보취급방침 | 청소년보호정책 | 스팸방지정책 | 사이버고객센터 | 약관안내 | 이메일 무단수집거부 | 서비스 전체보기\"");
  BOOST_CHECK_EQUAL(11647939066062684691ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"بان يأسف لمقتل لاجئين سوريين بتركيا المرزوقي يندد بعنف الأمن التونسي تنديد بقتل الجيش السوري مصورا تلفزيونيا 14 قتيلا وعشرات الجرحى بانفجار بالصومال\"");
  BOOST_CHECK_EQUAL(9773937585298648628ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"中华网以中国的市场为核心，致力为当地用户提供流动增值服务、网上娱乐及互联网服务。本公司亦推出网上游戏，及透过其门户网站提供包罗万有的网上产品及服务。\"");
  BOOST_CHECK_EQUAL(5348732066920102360ULL, TRI_HashJson(json));
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing
////////////////////////////////////////////////////////////////////////////////

/*
BOOST_AUTO_TEST_CASE (tst_json_hash) {
  TRI_json_t* json;

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "null");
  BOOST_CHECK_EQUAL(6601085983368743140ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "false");
  BOOST_CHECK_EQUAL(13113042584710199672ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "true");
  BOOST_CHECK_EQUAL(6583304908937478053ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "0");
  BOOST_CHECK_EQUAL(12161962213042174405ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "123");
  BOOST_CHECK_EQUAL(3423744850239007323ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"\"");
  BOOST_CHECK_EQUAL(12638153115695167455ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\" \"");
  BOOST_CHECK_EQUAL(560073664097094349ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"foobar\"");
  BOOST_CHECK_EQUAL(3770388817002598200ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"Foobar\"");
  BOOST_CHECK_EQUAL(6228943802847363544ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "\"FOOBAR\"");
  BOOST_CHECK_EQUAL(7710850877466186488ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[]");
  BOOST_CHECK_EQUAL(13796666053062066497ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ null ]");
  BOOST_CHECK_EQUAL(12579909069687325360ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ 0 ]");
  BOOST_CHECK_EQUAL(10101894954932532065ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ false ]");
  BOOST_CHECK_EQUAL(4554324570636443940ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ \"false\" ]");
  BOOST_CHECK_EQUAL(295270779373686828ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ [ ] ]");
  BOOST_CHECK_EQUAL(3935687115999630221ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ { } ]");
  BOOST_CHECK_EQUAL(13595004369025342186ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "[ [ false, 0 ] ]");
  BOOST_CHECK_EQUAL(8026218647638185280ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{}");
  BOOST_CHECK_EQUAL(5737045748118630438ULL, TRI_HashJson(json));
  FREE_JSON
 
  // the following hashes should be identical 
  const uint64_t a = 5721494255658103046ULL;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"1\", \"b\": \"2\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"2\", \"a\": \"1\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"2\", \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(a, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(2549570315580563109ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"1\" }");
  BOOST_CHECK_EQUAL(5635413490308263533ULL, TRI_HashJson(json));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": 123, \"b\": [ ] }");
  BOOST_CHECK_EQUAL(9398364376493393319ULL, TRI_HashJson(json));
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

/*
BOOST_AUTO_TEST_CASE (tst_json_hashattributes_single) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "_key" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  const uint64_t h1 = TRI_HashJsonByAttributes(json, v1, 1, true, error);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"_key\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"keys\": { \"_key\": \"foobar\" } }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_KEY\": \"foobar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"_key\": \"i-am-a-foo\" }");
  const uint64_t h2 = TRI_HashJsonByAttributes(json, v1, 1, true, error);
  BOOST_CHECK(h1 != h2);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foobar\", \"KEY\": 1234, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": [ \"foobar\" ], \"KEY\": { }, \"_key\": \"i-am-a-foo\" }");
  BOOST_CHECK_EQUAL(h2, TRI_HashJsonByAttributes(json, v1, 1, true, error));
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

/*
BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult1) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  const uint64_t h1 = TRI_HashJsonByAttributes(json, v1, 2, true, error);
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON

  // test if non-relevant attributes influence our hash
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"B\": 123 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"B\": 1234, \"a\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": null, \"A\": 123, \"B\": \"hihi\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": null, \"d\": null }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"A\": 1, \"B\": 2, \" a\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"ab\": 1, \"ba\": 2 }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names
////////////////////////////////////////////////////////////////////////////////

/*
BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult2) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  const uint64_t h1 = 6369173190757857502ULL;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(h1, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"food\", \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(720060016857102700ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"baz\" }");
  BOOST_CHECK_EQUAL(6361520589827022742ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"FOO\", \"b\": \"BAR\" }");
  BOOST_CHECK_EQUAL(3595137217367956894ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  BOOST_CHECK_EQUAL(12739237936894360852ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"meow\" }");
  BOOST_CHECK_EQUAL(13378327204915572311ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  BOOST_CHECK_EQUAL(10085884912118216755ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
  
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\", \"a\": \"meow\" }");
  BOOST_CHECK_EQUAL(15753579192430387496ULL, TRI_HashJsonByAttributes(json, v1, 2, true, error));
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hashing by attribute names with incomplete docs
////////////////////////////////////////////////////////////////////////////////

/*
BOOST_AUTO_TEST_CASE (tst_json_hashattributes_mult3) {
  TRI_json_t* json;
  int error;
  
  const char* v1[] = { "a", "b" };
  
  int error;
  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\", \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_NO_ERROR, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": \"foo\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"b\": \"bar\" }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"c\": 12 }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, error);
  FREE_JSON

  json = TRI_JsonString(TRI_UNKNOWN_MEM_ZONE, "{ \"a\": 1, \"b\": null }");
  TRI_HashJsonByAttributes(json, v1, 2, false, &error);
  BOOST_CHECK_EQUAL(TRI_ERROR_NO_ERROR, error);
  FREE_JSON
}
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief generate tests
////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_SUITE_END ()

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
