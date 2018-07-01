/*
 * Copyright 2017-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <future>
#include <map>
#include "gtest/gtest.h"
#include "../mock_tx_alloc.h"
#include "../../src/engines/mvtree.h"

using namespace pmemkv::mvtree;

const string PATH = "/dev/shm/pmemkv";
const string PATH_CACHED = "/tmp/pmemkv";
const string LAYOUT = "pmemkv";
const size_t SIZE = ((size_t) (1024 * 1024 * 1104));

class MVOidEmptyTest : public testing::Test {
public:

    PMEMobjpool *pop;
    MVOidEmptyTest() {
        std::remove(PATH.c_str());
        pop = pmemobj_create(PATH.c_str(), LAYOUT.c_str(), SIZE, S_IRWXU);
    }
    ~MVOidEmptyTest() {
        pmemobj_close(pop);
    }
};

class MVOidTest : public testing::Test {
public:
    MVTreeAnalysis analysis;
    MVTree *kv;
    PMEMobjpool *pop;
    PMEMoid rootoid;

    MVOidTest() {
        std::remove(PATH.c_str());
        pop = pmemobj_create(PATH.c_str(), LAYOUT.c_str(), SIZE, S_IRWXU);
        rootoid = OID_NULL;
        Open();
        rootoid = kv->GetRootOid();
    }

    ~MVOidTest() {
      delete kv;
      pmemobj_close(pop);
    }

    void Analyze() {
        analysis = {};
        kv->Analyze(analysis);
        // TODO ASSERT_TRUE(analysis.path == PATH);
    }

    void Reopen() {
        delete kv;
        Open();
    }

private:
    void Open() {
        kv = new MVTree(pop, rootoid);
    }
};

// =============================================================================================
// TEST EMPTY TREE with MVRoot on one newly created pmem object
// =============================================================================================


TEST_F(MVOidEmptyTest, CreateInstanceTestWithOid) {
    MVTree *kv = new MVTree(pop, OID_NULL);
    MVTreeAnalysis analysis = {};
    kv->Analyze(analysis);
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 0);
    delete kv;
}

TEST_F(MVOidEmptyTest, FailsToCreateInstanceWithInvalidPathWithOid) {
    try {
        new MVTree(nullptr, OID_NULL);
        FAIL();
    } catch (...) {
        // do nothing, expected to happen
    }
}

// (MVOidEmptyTest, FailsToCreateInstanceWithHugeSizeWithOid) {
//     try {
//         new MVTree(pop, OID_NULL, 9223372036854775807);   // 9.22 exabytes
//         FAIL();
//     } catch (...) {
//         // do nothing, expected to happen
//     }
// }

// (MVOidEmptyTest, FailsToCreateInstanceWithTinySizeWithOid) {
//     try {
//         new MVTree(pop, OID_NULL, PMEMOBJ_MIN_POOL - 1);  // too small
//         FAIL();
//     } catch (...) {
//         // do nothing, expected to happen
//     }
// }

// =============================================================================================
// TEST SINGLE-LEAF TREE 
// =============================================================================================

TEST_F(MVOidTest, BinaryKeyTest) {
    ASSERT_TRUE(kv->Put("a", "should_not_change") == OK) << pmemobj_errormsg();
    string key1 = string("a\0b", 3);
    ASSERT_TRUE(kv->Put(key1, "stuff") == OK) << pmemobj_errormsg();
    string value;
    ASSERT_TRUE(kv->Get(key1, &value) == OK && value == "stuff");
    string value2;
    ASSERT_TRUE(kv->Get("a", &value2) == OK && value2 == "should_not_change");
    ASSERT_TRUE(kv->Remove(key1) == OK);
    string value3;
    ASSERT_TRUE(kv->Get(key1, &value3) == NOT_FOUND);
    ASSERT_TRUE(kv->Get("a", &value3) == OK && value3 == "should_not_change");
}

TEST_F(MVOidTest, BinaryValueTest) {
    string value("A\0B\0\0C", 6);
    ASSERT_TRUE(kv->Put("key1", value) == OK) << pmemobj_errormsg();
    string value_out;
    ASSERT_TRUE(kv->Get("key1", &value_out) == OK && (value_out.length() == 6) && (value_out == value));
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, EmptyKeyTest) {
    ASSERT_TRUE(kv->Put("", "empty") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put(" ", "single-space") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("\t\t", "two-tab") == OK) << pmemobj_errormsg();
    string value1;
    string value2;
    string value3;
    ASSERT_TRUE(kv->Get("", &value1) == OK && value1 == "empty");
    ASSERT_TRUE(kv->Get(" ", &value2) == OK && value2 == "single-space");
    ASSERT_TRUE(kv->Get("\t\t", &value3) == OK && value3 == "two-tab");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, EmptyValueTest) {
    ASSERT_TRUE(kv->Put("empty", "") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("single-space", " ") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("two-tab", "\t\t") == OK) << pmemobj_errormsg();
    string value1;
    string value2;
    string value3;
    ASSERT_TRUE(kv->Get("empty", &value1) == OK && value1 == "");
    ASSERT_TRUE(kv->Get("single-space", &value2) == OK && value2 == " ");
    ASSERT_TRUE(kv->Get("two-tab", &value3) == OK && value3 == "\t\t");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetAppendToExternalValueTest) {
    ASSERT_TRUE(kv->Put("key1", "cool") == OK) << pmemobj_errormsg();
    string value = "super";
    ASSERT_TRUE(kv->Get("key1", &value) == OK && value == "supercool");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetHeadlessTest) {
    string value;
    ASSERT_TRUE(kv->Get("waldo", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 0);
}

TEST_F(MVOidTest, GetMultipleTest) {
    ASSERT_TRUE(kv->Put("abc", "A1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("def", "B2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("hij", "C3") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("jkl", "D4") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("mno", "E5") == OK) << pmemobj_errormsg();
    string value1;
    ASSERT_TRUE(kv->Get("abc", &value1) == OK && value1 == "A1");
    string value2;
    ASSERT_TRUE(kv->Get("def", &value2) == OK && value2 == "B2");
    string value3;
    ASSERT_TRUE(kv->Get("hij", &value3) == OK && value3 == "C3");
    string value4;
    ASSERT_TRUE(kv->Get("jkl", &value4) == OK && value4 == "D4");
    string value5;
    ASSERT_TRUE(kv->Get("mno", &value5) == OK && value5 == "E5");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetMultiple2Test) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("key2", "value2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("key3", "value3") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("key2") == OK);
    ASSERT_TRUE(kv->Put("key3", "VALUE3") == OK) << pmemobj_errormsg();
    string value1;
    ASSERT_TRUE(kv->Get("key1", &value1) == OK && value1 == "value1");
    string value2;
    ASSERT_TRUE(kv->Get("key2", &value2) == NOT_FOUND);
    string value3;
    ASSERT_TRUE(kv->Get("key3", &value3) == OK && value3 == "VALUE3");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetNonexistentTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    string value;
    ASSERT_TRUE(kv->Get("waldo", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, PutTest) {
    string value;
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("key1", &value) == OK && value == "value1");

    string new_value;
    ASSERT_TRUE(kv->Put("key1", "VALUE1") == OK) << pmemobj_errormsg();           // same size
    ASSERT_TRUE(kv->Get("key1", &new_value) == OK && new_value == "VALUE1");

    string new_value2;
    ASSERT_TRUE(kv->Put("key1", "new_value") == OK) << pmemobj_errormsg();        // longer size
    ASSERT_TRUE(kv->Get("key1", &new_value2) == OK && new_value2 == "new_value");

    string new_value3;
    ASSERT_TRUE(kv->Put("key1", "?") == OK) << pmemobj_errormsg();                // shorter size
    ASSERT_TRUE(kv->Get("key1", &new_value3) == OK && new_value3 == "?");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, PutKeysOfDifferentSizesTest) {
    string value;
    ASSERT_TRUE(kv->Put("123456789ABCDE", "A") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("123456789ABCDE", &value) == OK && value == "A");

    string value2;
    ASSERT_TRUE(kv->Put("123456789ABCDEF", "B") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("123456789ABCDEF", &value2) == OK && value2 == "B");

    string value3;
    ASSERT_TRUE(kv->Put("12345678ABCDEFG", "C") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("12345678ABCDEFG", &value3) == OK && value3 == "C");

    string value4;
    ASSERT_TRUE(kv->Put("123456789", "D") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("123456789", &value4) == OK && value4 == "D");

    string value5;
    ASSERT_TRUE(kv->Put("123456789ABCDEFGHI", "E") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("123456789ABCDEFGHI", &value5) == OK && value5 == "E");
}

TEST_F(MVOidTest, PutValuesOfDifferentSizesTest) {
    string value;
    ASSERT_TRUE(kv->Put("A", "123456789ABCDE") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("A", &value) == OK && value == "123456789ABCDE");

    string value2;
    ASSERT_TRUE(kv->Put("B", "123456789ABCDEF") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("B", &value2) == OK && value2 == "123456789ABCDEF");

    string value3;
    ASSERT_TRUE(kv->Put("C", "12345678ABCDEFG") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("C", &value3) == OK && value3 == "12345678ABCDEFG");

    string value4;
    ASSERT_TRUE(kv->Put("D", "123456789") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("D", &value4) == OK && value4 == "123456789");

    string value5;
    ASSERT_TRUE(kv->Put("E", "123456789ABCDEFGHI") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("E", &value5) == OK && value5 == "123456789ABCDEFGHI");
}

TEST_F(MVOidTest, PutValuesOfMaximumSizeTest) {
    // todo finish this when max is decided (#61)
}

TEST_F(MVOidTest, RemoveAllTest) {
    ASSERT_TRUE(kv->Put("tmpkey", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("tmpkey") == OK);
    string value;
    ASSERT_TRUE(kv->Get("tmpkey", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveAndInsertTest) {
    ASSERT_TRUE(kv->Put("tmpkey", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("tmpkey") == OK);
    string value;
    ASSERT_TRUE(kv->Get("tmpkey", &value) == NOT_FOUND);
    ASSERT_TRUE(kv->Put("tmpkey1", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == OK && value == "tmpvalue1");
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK);
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveExistingTest) {
    ASSERT_TRUE(kv->Put("tmpkey1", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("tmpkey2", "tmpvalue2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK);
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK); // ok to remove twice
    string value;
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == NOT_FOUND);
    ASSERT_TRUE(kv->Get("tmpkey2", &value) == OK && value == "tmpvalue2");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveHeadlessTest) {
    ASSERT_TRUE(kv->Remove("nada") == OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 0);
}

TEST_F(MVOidTest, RemoveNonexistentTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("nada") == OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

// =============================================================================================
// TEST RECOVERY OF SINGLE-LEAF TREE
// =============================================================================================

TEST_F(MVOidTest, GetHeadlessAfterRecoveryTest) {
    Reopen();
    string value;
    ASSERT_TRUE(kv->Get("waldo", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 0);
}

TEST_F(MVOidTest, GetMultipleAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("abc", "A1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("def", "B2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("hij", "C3") == OK) << pmemobj_errormsg();
    Reopen();
    ASSERT_TRUE(kv->Put("jkl", "D4") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("mno", "E5") == OK) << pmemobj_errormsg();
    string value1;
    ASSERT_TRUE(kv->Get("abc", &value1) == OK && value1 == "A1");
    string value2;
    ASSERT_TRUE(kv->Get("def", &value2) == OK && value2 == "B2");
    string value3;
    ASSERT_TRUE(kv->Get("hij", &value3) == OK && value3 == "C3");
    string value4;
    ASSERT_TRUE(kv->Get("jkl", &value4) == OK && value4 == "D4");
    string value5;
    ASSERT_TRUE(kv->Get("mno", &value5) == OK && value5 == "E5");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetMultiple2AfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("key2", "value2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("key3", "value3") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("key2") == OK);
    ASSERT_TRUE(kv->Put("key3", "VALUE3") == OK) << pmemobj_errormsg();
    Reopen();
    string value1;
    ASSERT_TRUE(kv->Get("key1", &value1) == OK && value1 == "value1");
    string value2;
    ASSERT_TRUE(kv->Get("key2", &value2) == NOT_FOUND);
    string value3;
    ASSERT_TRUE(kv->Get("key3", &value3) == OK && value3 == "VALUE3");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, GetNonexistentAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    Reopen();
    string value;
    ASSERT_TRUE(kv->Get("waldo", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, PutAfterRecoveryTest) {
    string value;
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("key1", &value) == OK && value == "value1");

    string new_value;
    ASSERT_TRUE(kv->Put("key1", "VALUE1") == OK) << pmemobj_errormsg();           // same size
    ASSERT_TRUE(kv->Get("key1", &new_value) == OK && new_value == "VALUE1");
    Reopen();

    string new_value2;
    ASSERT_TRUE(kv->Put("key1", "new_value") == OK) << pmemobj_errormsg();        // longer size
    ASSERT_TRUE(kv->Get("key1", &new_value2) == OK && new_value2 == "new_value");

    string new_value3;
    ASSERT_TRUE(kv->Put("key1", "?") == OK) << pmemobj_errormsg();                // shorter size
    ASSERT_TRUE(kv->Get("key1", &new_value3) == OK && new_value3 == "?");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveAllAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("tmpkey", "tmpvalue1") == OK) << pmemobj_errormsg();
    Reopen();
    ASSERT_TRUE(kv->Remove("tmpkey") == OK);
    string value;
    ASSERT_TRUE(kv->Get("tmpkey", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveAndInsertAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("tmpkey", "tmpvalue1") == OK) << pmemobj_errormsg();
    Reopen();
    ASSERT_TRUE(kv->Remove("tmpkey") == OK);
    string value;
    ASSERT_TRUE(kv->Get("tmpkey", &value) == NOT_FOUND);
    ASSERT_TRUE(kv->Put("tmpkey1", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == OK && value == "tmpvalue1");
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK);
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == NOT_FOUND);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveExistingAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("tmpkey1", "tmpvalue1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Put("tmpkey2", "tmpvalue2") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK);
    Reopen();
    ASSERT_TRUE(kv->Remove("tmpkey1") == OK); // ok to remove twice
    string value;
    ASSERT_TRUE(kv->Get("tmpkey1", &value) == NOT_FOUND);
    ASSERT_TRUE(kv->Get("tmpkey2", &value) == OK && value == "tmpvalue2");
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, RemoveHeadlessAfterRecoveryTest) {
    Reopen();
    ASSERT_TRUE(kv->Remove("nada") == OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 0);
}

TEST_F(MVOidTest, RemoveNonexistentAfterRecoveryTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    Reopen();
    ASSERT_TRUE(kv->Remove("nada") == OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

TEST_F(MVOidTest, UsePreallocAfterSingleLeafRecoveryTest) {
    ASSERT_TRUE(kv->Put("key1", "value1") == OK) << pmemobj_errormsg();
    ASSERT_TRUE(kv->Remove("key1") == OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);

    Reopen();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 1);
    ASSERT_EQ(analysis.leaf_total, 1);

    ASSERT_TRUE(kv->Put("key2", "value2") == OK) << pmemobj_errormsg();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 1);
}

// =============================================================================================
// TEST TREE WITH SINGLE INNER NODE
// =============================================================================================

const int SINGLE_INNER_LIMIT = LEAF_KEYS * (INNER_KEYS - 1);

TEST_F(MVOidTest, SingleInnerNodeAscendingTest) {
    for (int i = 10000; i <= (10000 + SINGLE_INNER_LIMIT); i++) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    for (int i = 10000; i <= (10000 + SINGLE_INNER_LIMIT); i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

TEST_F(MVOidTest, SingleInnerNodeAscendingTest2) {
    for (int i = 1; i <= SINGLE_INNER_LIMIT; i++) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    for (int i = 1; i <= SINGLE_INNER_LIMIT; i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

TEST_F(MVOidTest, SingleInnerNodeDescendingTest) {
    for (int i = (10000 + SINGLE_INNER_LIMIT); i >= 10000; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    for (int i = (10000 + SINGLE_INNER_LIMIT); i >= 10000; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 6);
}

TEST_F(MVOidTest, SingleInnerNodeDescendingTest2) {
    for (int i = SINGLE_INNER_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    for (int i = SINGLE_INNER_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

// =============================================================================================
// TEST RECOVERY OF TREE WITH SINGLE INNER NODE
// =============================================================================================

TEST_F(MVOidTest, SingleInnerNodeAscendingAfterRecoveryTest) {
    for (int i = 10000; i <= (10000 + SINGLE_INNER_LIMIT); i++) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = 10000; i <= (10000 + SINGLE_INNER_LIMIT); i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

TEST_F(MVOidTest, SingleInnerNodeAscendingAfterRecoveryTest2) {
    for (int i = 1; i <= SINGLE_INNER_LIMIT; i++) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = 1; i <= SINGLE_INNER_LIMIT; i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

TEST_F(MVOidTest, SingleInnerNodeDescendingAfterRecoveryTest) {
    for (int i = (10000 + SINGLE_INNER_LIMIT); i >= 10000; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = (10000 + SINGLE_INNER_LIMIT); i >= 10000; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 6);
}

TEST_F(MVOidTest, SingleInnerNodeDescendingAfterRecoveryTest2) {
    for (int i = SINGLE_INNER_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, istr) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = SINGLE_INNER_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == istr);
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 5);
}

TEST_F(MVOidTest, UsePreallocAfterMultipleLeafRecoveryTest) {
    for (int i = 1; i <= LEAF_KEYS + 1; i++)
        ASSERT_EQ(kv->Put(to_string(i), "!"), OK) << pmemobj_errormsg();
    Reopen();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 2);

    for (int i = 1; i <= LEAF_KEYS; i++) ASSERT_EQ(kv->Remove(to_string(i)), OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 2);
    Reopen();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 1);
    ASSERT_EQ(analysis.leaf_total, 2);

    ASSERT_EQ(kv->Remove(to_string(LEAF_KEYS + 1)), OK);
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 2);
    ASSERT_EQ(analysis.leaf_prealloc, 1);
    ASSERT_EQ(analysis.leaf_total, 2);
    Reopen();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 2);
    ASSERT_EQ(analysis.leaf_prealloc, 2);
    ASSERT_EQ(analysis.leaf_total, 2);

    for (int i = 1; i <= LEAF_KEYS; i++)
        ASSERT_EQ(kv->Put(to_string(i), "!"), OK) << pmemobj_errormsg();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 1);
    ASSERT_EQ(analysis.leaf_prealloc, 1);
    ASSERT_EQ(analysis.leaf_total, 2);
    ASSERT_EQ(kv->Put(to_string(LEAF_KEYS + 1), "!"), OK) << pmemobj_errormsg();
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 2);
}

// =============================================================================================
// TEST LARGE TREE
// =============================================================================================

const int LARGE_LIMIT = 2000; // 4000000;

TEST_F(MVOidTest, LargeAscendingTest) {
  std::map<string, string> = {
    { "n01950731_11432.JPEG", "n01950731_11432.JPEG" }
{ "n01950731_3712.JPEG ", "n01950731_3712.JPEG " }
{ "n01950731_344.JPEG", "n01950731_344.JPEG" }
{ "n01950731_12614.JPEG", "n01950731_12614.JPEG" }
{ "n01950731_14730.JPEG", "n01950731_14730.JPEG" }
{ "n01950731_2425.JPEG", "n01950731_2425.JPEG" }
{ "n01950731_13765.JPEG", "n01950731_13765.JPEG" }
{ "n01950731_8413.JPEG", "n01950731_8413.JPEG" }
{ "n01950731_10425.JPEG", "n01950731_10425.JPEG" }
{ "n01950731_12448.JPEG", "n01950731_12448.JPEG" }
{ "n01950731_8179.JPEG", "n01950731_8179.JPEG" }
{ "n01950731_4601.JPEG", "n01950731_4601.JPEG" }
{ "n01950731_8969.JPEG", "n01950731_8969.JPEG" }
{ "n01950731_12933.JPEG", "n01950731_12933.JPEG" }
{ "n01950731_11736.JPEG", "n01950731_11736.JPEG" }
{ "n01950731_2016.JPEG", "n01950731_2016.JPEG" }
{ "n01950731_13225.JPEG", "n01950731_13225.JPEG" }
{ "n01950731_2268.JPEG", "n01950731_2268.JPEG" }
{ "n01950731_2157.JPEG", "n01950731_2157.JPEG" }
{ "n01950731_728.JPEG", "n01950731_728.JPEG" }
{ "n01950731_12167.JPEG", "n01950731_12167.JPEG" }
{ "n01950731_12610.JPEG", "n01950731_12610.JPEG" }
{ "n01950731_5002.JPEG", "n01950731_5002.JPEG" }
{ "n01950731_16869.JPEG", "n01950731_16869.JPEG" }
{ "n01950731_7906.JPEG", "n01950731_7906.JPEG" }
{ "n01950731_22717.JPEG", "n01950731_22717.JPEG" }
{ "n01950731_15680.JPEG", "n01950731_15680.JPEG" }
{ "n01950731_2727.JPEG", "n01950731_2727.JPEG" }
{ "n01950731_16418.JPEG", "n01950731_16418.JPEG" }
{ "n01950731_13447.JPEG", "n01950731_13447.JPEG" }
{ "n01950731_12661.JPEG", "n01950731_12661.JPEG" }
{ "n01950731_13992.JPEG", "n01950731_13992.JPEG" }
{ "n01950731_14347.JPEG", "n01950731_14347.JPEG" }
{ "n01950731_14203.JPEG", "n01950731_14203.JPEG" }
{ "n01950731_22795.JPEG", "n01950731_22795.JPEG" }
{ "n01950731_15462.JPEG", "n01950731_15462.JPEG" }
{ "n01950731_12996.JPEG", "n01950731_12996.JPEG" }
{ "n01950731_9661.JPEG", "n01950731_9661.JPEG" }
{ "n01950731_4133.JPEG", "n01950731_4133.JPEG" }
{ "n01950731_4376.JPEG", "n01950731_4376.JPEG" }
{ "n01950731_10431.JPEG", "n01950731_10431.JPEG" }
{ "n01950731_13433.JPEG", "n01950731_13433.JPEG" }
{ "n01950731_13188.JPEG", "n01950731_13188.JPEG" }
{ "n01950731_5054.JPEG", "n01950731_5054.JPEG" }
{ "n01950731_2848.JPEG", "n01950731_2848.JPEG" }
{ "n01950731_12859.JPEG", "n01950731_12859.JPEG" }
{ "n01950731_19865.JPEG", "n01950731_19865.JPEG" }
{ "n01950731_12513.JPEG", "n01950731_12513.JPEG" }
{ "n01950731_16906.JPEG", "n01950731_16906.JPEG" }
{ "n01950731_13723.JPEG", "n01950731_13723.JPEG" }
{ "n01950731_12605.JPEG", "n01950731_12605.JPEG" }
{ "n01950731_8445.JPEG", "n01950731_8445.JPEG" }
{ "n01950731_23528.JPEG", "n01950731_23528.JPEG" }
{ "n01950731_16379.JPEG", "n01950731_16379.JPEG" }
{ "n01950731_15784.JPEG", "n01950731_15784.JPEG" }
{ "n01950731_2560.JPEG", "n01950731_2560.JPEG" }
{ "n01950731_3240.JPEG", "n01950731_3240.JPEG" }
{ "n01950731_12482.JPEG", "n01950731_12482.JPEG" }
{ "n01950731_16004.JPEG", "n01950731_16004.JPEG" }
{ "n01950731_15732.JPEG", "n01950731_15732.JPEG" }
{ "n01950731_16556.JPEG", "n01950731_16556.JPEG" }
{ "n01950731_23520.JPEG", "n01950731_23520.JPEG" }
{ "n01950731_17543.JPEG", "n01950731_17543.JPEG" }
{ "n01950731_10361.JPEG", "n01950731_10361.JPEG" }
{ "n01950731_15817.JPEG", "n01950731_15817.JPEG" }
{ "n01950731_28650.JPEG", "n01950731_28650.JPEG" }
{ "n01950731_10662.JPEG", "n01950731_10662.JPEG" }
{ "n01950731_12724.JPEG", "n01950731_12724.JPEG" }
{ "n01950731_28627.JPEG", "n01950731_28627.JPEG" }
{ "n01950731_9370.JPEG", "n01950731_9370.JPEG" }
{ "n01950731_12121.JPEG", "n01950731_12121.JPEG" }
{ "n01950731_4250.JPEG", "n01950731_4250.JPEG" }
{ "n01950731_21421.JPEG", "n01950731_21421.JPEG" }
{ "n01950731_21428.JPEG", "n01950731_21428.JPEG" }
{ "n01950731_13350.JPEG", "n01950731_13350.JPEG" }
{ "n01950731_11759.JPEG", "n01950731_11759.JPEG" }
{ "n01950731_8905.JPEG", "n01950731_8905.JPEG" }
{ "n01950731_9356.JPEG", "n01950731_9356.JPEG" }
{ "n01950731_11644.JPEG", "n01950731_11644.JPEG" }
{ "n01950731_2131.JPEG", "n01950731_2131.JPEG" }
{ "n01950731_21332.JPEG", "n01950731_21332.JPEG" }
{ "n01950731_10245.JPEG", "n01950731_10245.JPEG" }
{ "n01950731_8923.JPEG", "n01950731_8923.JPEG" }
{ "n01950731_13802.JPEG", "n01950731_13802.JPEG" }
{ "n01950731_13457.JPEG", "n01950731_13457.JPEG" }
{ "n01950731_20767.JPEG", "n01950731_20767.JPEG" }
{ "n01950731_9232.JPEG", "n01950731_9232.JPEG" }
{ "n01950731_15655.JPEG", "n01950731_15655.JPEG" }
{ "n01950731_13048.JPEG", "n01950731_13048.JPEG" }
{ "n01950731_14388.JPEG", "n01950731_14388.JPEG" }
{ "n01950731_171.JPEG", "n01950731_171.JPEG" }
{ "n01950731_16381.JPEG", "n01950731_16381.JPEG" }
{ "n01950731_8291.JPEG", "n01950731_8291.JPEG" }
{ "n01950731_11745.JPEG", "n01950731_11745.JPEG" }
{ "n01950731_10150.JPEG", "n01950731_10150.JPEG" }
{ "n01950731_18103.JPEG", "n01950731_18103.JPEG" }
{ "n01950731_651.JPEG", "n01950731_651.JPEG" }
{ "n01950731_1936.JPEG", "n01950731_1936.JPEG" }
{ "n01950731_16139.JPEG", "n01950731_16139.JPEG" }
{ "n01950731_13698.JPEG", "n01950731_13698.JPEG" }
{ "n01950731_5652.JPEG", "n01950731_5652.JPEG" }
{ "n01950731_3264.JPEG", "n01950731_3264.JPEG" }
{ "n01950731_33840.JPEG", "n01950731_33840.JPEG" }
{ "n01950731_15823.JPEG", "n01950731_15823.JPEG" }
{ "n01950731_8536.JPEG", "n01950731_8536.JPEG" }
{ "n01950731_11743.JPEG", "n01950731_11743.JPEG" }
{ "n01950731_399.JPEG", "n01950731_399.JPEG" }
{ "n01950731_1638.JPEG", "n01950731_1638.JPEG" }
{ "n01950731_14977.JPEG", "n01950731_14977.JPEG" }
{ "n01950731_12745.JPEG", "n01950731_12745.JPEG" }
{ "n01950731_9817.JPEG", "n01950731_9817.JPEG" }
{ "n01950731_16398.JPEG", "n01950731_16398.JPEG" }
{ "n01950731_5126.JPEG", "n01950731_5126.JPEG" }
{ "n01950731_15796.JPEG", "n01950731_15796.JPEG" }
{ "n01950731_11079.JPEG", "n01950731_11079.JPEG" }
{ "n01950731_14091.JPEG", "n01950731_14091.JPEG" }
{ "n01950731_15389.JPEG", "n01950731_15389.JPEG" }
{ "n01950731_6654.JPEG", "n01950731_6654.JPEG" }
{ "n01950731_14208.JPEG", "n01950731_14208.JPEG" }
{ "n01950731_16103.JPEG", "n01950731_16103.JPEG" }
{ "n01950731_12013.JPEG", "n01950731_12013.JPEG" }
{ "n01950731_13563.JPEG", "n01950731_13563.JPEG" }
{ "n01950731_16693.JPEG", "n01950731_16693.JPEG" }
{ "n01950731_33114.JPEG", "n01950731_33114.JPEG" }
{ "n01950731_17974.JPEG", "n01950731_17974.JPEG" }
{ "n01950731_16544.JPEG", "n01950731_16544.JPEG" }
{ "n01950731_2291.JPEG", "n01950731_2291.JPEG" }
{ "n01950731_8517.JPEG", "n01950731_8517.JPEG" }
{ "n01950731_8093.JPEG", "n01950731_8093.JPEG" }
{ "n01950731_10695.JPEG", "n01950731_10695.JPEG" }
{ "n01950731_21212.JPEG", "n01950731_21212.JPEG" }
{ "n01950731_141.JPEG", "n01950731_141.JPEG" }
{ "n01950731_17768.JPEG", "n01950731_17768.JPEG" }
{ "n01950731_12912.JPEG", "n01950731_12912.JPEG" }
{ "n01950731_23310.JPEG", "n01950731_23310.JPEG" }
{ "n01950731_3595.JPEG", "n01950731_3595.JPEG" }
{ "n01950731_4663.JPEG", "n01950731_4663.JPEG" }
{ "n01950731_12904.JPEG", "n01950731_12904.JPEG" }
{ "n01950731_10174.JPEG", "n01950731_10174.JPEG" }
{ "n01950731_9098.JPEG", "n01950731_9098.JPEG" }
{ "n01950731_18145.JPEG", "n01950731_18145.JPEG" }
{ "n01950731_11086.JPEG", "n01950731_11086.JPEG" }
{ "n01950731_1096.JPEG", "n01950731_1096.JPEG" }
{ "n01950731_4722.JPEG", "n01950731_4722.JPEG" }
{ "n01950731_16414.JPEG", "n01950731_16414.JPEG" }
{ "n01950731_13959.JPEG", "n01950731_13959.JPEG" }
{ "n01950731_21003.JPEG", "n01950731_21003.JPEG" }
{ "n01950731_1533.JPEG", "n01950731_1533.JPEG" }
{ "n01950731_17603.JPEG", "n01950731_17603.JPEG" }
{ "n01950731_4046.JPEG", "n01950731_4046.JPEG" }
{ "n01950731_15157.JPEG", "n01950731_15157.JPEG" }
{ "n01950731_13094.JPEG", "n01950731_13094.JPEG" }
{ "n01950731_4998.JPEG", "n01950731_4998.JPEG" }
{ "n01950731_1335.JPEG", "n01950731_1335.JPEG" }
{ "n01950731_20980.JPEG", "n01950731_20980.JPEG" }
{ "n01950731_6396.JPEG", "n01950731_6396.JPEG" }
{ "n01950731_22546.JPEG", "n01950731_22546.JPEG" }
{ "n01950731_8596.JPEG", "n01950731_8596.JPEG" }
{ "n01950731_4399.JPEG", "n01950731_4399.JPEG" }
{ "n01950731_14466.JPEG", "n01950731_14466.JPEG" }
{ "n01950731_6412.JPEG", "n01950731_6412.JPEG" }
{ "n01950731_11842.JPEG", "n01950731_11842.JPEG" }
{ "n01950731_14154.JPEG", "n01950731_14154.JPEG" }
{ "n01950731_7578.JPEG", "n01950731_7578.JPEG" }
{ "n01950731_10520.JPEG", "n01950731_10520.JPEG" }
{ "n01950731_26161.JPEG", "n01950731_26161.JPEG" }
{ "n01950731_13996.JPEG", "n01950731_13996.JPEG" }
{ "n01950731_13091.JPEG", "n01950731_13091.JPEG" }
{ "n01950731_12932.JPEG", "n01950731_12932.JPEG" }
{ "n01950731_17641.JPEG", "n01950731_17641.JPEG" }
{ "n01950731_6403.JPEG", "n01950731_6403.JPEG" }
{ "n01950731_11822.JPEG", "n01950731_11822.JPEG" }
{ "n01950731_17474.JPEG", "n01950731_17474.JPEG" }
{ "n01950731_12921.JPEG", "n01950731_12921.JPEG" }
{ "n01950731_7424.JPEG", "n01950731_7424.JPEG" }
{ "n01950731_12223.JPEG", "n01950731_12223.JPEG" }
{ "n01950731_2601.JPEG", "n01950731_2601.JPEG" }
{ "n01950731_10261.JPEG", "n01950731_10261.JPEG" }
{ "n01950731_9317.JPEG", "n01950731_9317.JPEG" }
{ "n01950731_17432.JPEG", "n01950731_17432.JPEG" }
{ "n01950731_6978.JPEG", "n01950731_6978.JPEG" }
{ "n01950731_21393.JPEG", "n01950731_21393.JPEG" }
{ "n01950731_9505.JPEG", "n01950731_9505.JPEG" }
{ "n01950731_968.JPEG", "n01950731_968.JPEG" }
{ "n01950731_15621.JPEG", "n01950731_15621.JPEG" }
{ "n01950731_11614.JPEG", "n01950731_11614.JPEG" }
{ "n01950731_4152.JPEG", "n01950731_4152.JPEG" }
{ "n01950731_12998.JPEG", "n01950731_12998.JPEG" }
{ "n01950731_9951.JPEG", "n01950731_9951.JPEG" }
{ "n01950731_7573.JPEG", "n01950731_7573.JPEG" }
{ "n01950731_14574.JPEG", "n01950731_14574.JPEG" }
{ "n01950731_20029.JPEG", "n01950731_20029.JPEG" }
{ "n01950731_1180.JPEG", "n01950731_1180.JPEG" }
{ "n01950731_5888.JPEG", "n01950731_5888.JPEG" }
{ "n01950731_15052.JPEG", "n01950731_15052.JPEG" }
{ "n01950731_14317.JPEG", "n01950731_14317.JPEG" }
{ "n01950731_24023.JPEG", "n01950731_24023.JPEG" }
{ "n01950731_30122.JPEG", "n01950731_30122.JPEG" }
{ "n01950731_12205.JPEG", "n01950731_12205.JPEG" }
{ "n01950731_7396.JPEG", "n01950731_7396.JPEG" }
{ "n01950731_13243.JPEG", "n01950731_13243.JPEG" }};
    std::future<void> f1 =
        std::async(std::launch::async,
                   [&](){ 
                     for (int i = 1; i <= LARGE_LIMIT/4; i++) {
                       string istr = to_string(i);
                       ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
                       string value;
                       ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
                     }
                   });  

    std::future<void> f2 =
        std::async(std::launch::async,
                   [&](){ 
                     for (int i = LARGE_LIMIT/4+1; i <= LARGE_LIMIT/2; i++) {
                       string istr = to_string(i);
                       ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
                       string value;
                       ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
                     }
                   });  
    std::future<void> f3 =
        std::async(std::launch::async,
                   [&](){ 
                     for (int i = LARGE_LIMIT/2+1; i <= LARGE_LIMIT/4*3; i++) {
                       string istr = to_string(i);
                       ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
                       string value;
                       ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
                     }
                   });  
    std::future<void> f4 =
        std::async(std::launch::async,
                   [&](){ 
                     for (int i = LARGE_LIMIT/4*3+1; i <= LARGE_LIMIT; i++) {
                       string istr = to_string(i);
                       ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
                       string value;
                       ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
                     }
                   });  

    f1.wait();
    f2.wait();
    f3.wait();
    f4.wait();

    for (int i = 1; i <= LARGE_LIMIT; i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK);
        std::cout << "*******" << value << std::endl;
        ASSERT_TRUE(value == (istr + "!"));
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 152455);
}

TEST_F(MVOidTest, LargeDescendingTest) {
    for (int i = LARGE_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, ("ABC" + istr)) == OK) << pmemobj_errormsg();
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == ("ABC" + istr));
    }
    for (int i = LARGE_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == ("ABC" + istr));
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 150000);
}

// =============================================================================================
// TEST RECOVERY OF LARGE TREE
// =============================================================================================

TEST_F(MVOidTest, LargeAscendingAfterRecoveryTest) {
    for (int i = 1; i <= LARGE_LIMIT; i++) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = 1; i <= LARGE_LIMIT; i++) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 152455);
}

TEST_F(MVOidTest, LargeDescendingAfterRecoveryTest) {
    for (int i = LARGE_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        ASSERT_TRUE(kv->Put(istr, ("ABC" + istr)) == OK) << pmemobj_errormsg();
    }
    Reopen();
    for (int i = LARGE_LIMIT; i >= 1; i--) {
        string istr = to_string(i);
        string value;
        ASSERT_TRUE(kv->Get(istr, &value) == OK && value == ("ABC" + istr));
    }
    Analyze();
    ASSERT_EQ(analysis.leaf_empty, 0);
    ASSERT_EQ(analysis.leaf_prealloc, 0);
    ASSERT_EQ(analysis.leaf_total, 150000);
}

// =============================================================================================
// TEST RUNNING OUT OF SPACE
// =============================================================================================

class MVOidFullTest : public testing::Test {
public:
    MVTree *kv;
    PMEMobjpool *pop;
    PMEMoid rootoid;

    MVOidFullTest() {
        std::remove(PATH.c_str());
        pop = pmemobj_create(PATH.c_str(), LAYOUT.c_str(), SIZE, S_IRWXU);
        rootoid = OID_NULL;
        Open();
        rootoid = kv->GetRootOid();
    }

    ~MVOidFullTest() {
        delete kv;
        pmemobj_close(pop);
    }

    void Reopen() {
        delete kv;
        kv = new MVTree(pop, rootoid);
    }

    void Validate() {
        for (int i = 1; i <= LARGE_LIMIT; i++) {
            string istr = to_string(i);
            string value;
            ASSERT_TRUE(kv->Get(istr, &value) == OK && value == (istr + "!"));
        }

        Reopen();

        ASSERT_TRUE(kv->Put("1", "!1") == OK);
        string value;
        ASSERT_TRUE(kv->Get("1", &value) == OK && value == ("!1"));
        ASSERT_TRUE(kv->Put("1", "1!") == OK);
        string value2;
        ASSERT_TRUE(kv->Get("1", &value2) == OK && value2 == ("1!"));

        for (int i = 1; i <= LARGE_LIMIT; i++) {
            string istr = to_string(i);
            string value3;
            ASSERT_TRUE(kv->Get(istr, &value3) == OK && value3 == (istr + "!"));
        }
    }

private:
    void Open() {
        std::cout << "!!! creating at " << PATH << "\n";
        kv = new MVTree(pop, rootoid);
        rootoid = kv->GetRootOid();
        for (int i = 1; i <= LARGE_LIMIT; i++) {
            string istr = to_string(i);
            ASSERT_TRUE(kv->Put(istr, (istr + "!")) == OK) << pmemobj_errormsg();
        }
    }
};

const string LONGSTR = "123456789A123456789A123456789A123456789A123456789A123456789A123456789A";

TEST_F(MVOidFullTest, OutOfSpace1Test) {
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put("100", "?") == FAILED);
    tx_alloc_should_fail = false;
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace2aTest) {
    ASSERT_TRUE(kv->Remove("100") == OK);
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put("100", LONGSTR) == FAILED);
    tx_alloc_should_fail = false;
    ASSERT_TRUE(kv->Put("100", "100!") == OK) << pmemobj_errormsg();
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace2bTest) {
    ASSERT_TRUE(kv->Remove("100") == OK);
    ASSERT_TRUE(kv->Put("100", "100!") == OK) << pmemobj_errormsg();
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put("100", LONGSTR) == FAILED);
    tx_alloc_should_fail = false;
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace3aTest) {
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put("100", LONGSTR) == FAILED);
    tx_alloc_should_fail = false;
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace3bTest) {
    tx_alloc_should_fail = true;
    for (int i = 0; i <= 99999; i++) {
        ASSERT_TRUE(kv->Put("123456", LONGSTR) == FAILED);
    }
    tx_alloc_should_fail = false;
    ASSERT_TRUE(kv->Remove("4567") == OK);
    ASSERT_TRUE(kv->Put("4567", "4567!") == OK) << pmemobj_errormsg();
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace4aTest) {
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put(to_string(LARGE_LIMIT + 1), "1") == FAILED);
    tx_alloc_should_fail = false;
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace4bTest) {
    tx_alloc_should_fail = true;
    for (int i = 0; i <= 99999; i++) {
        ASSERT_TRUE(kv->Put(to_string(LARGE_LIMIT + 1), "1") == FAILED);
    }
    tx_alloc_should_fail = false;
    ASSERT_TRUE(kv->Remove("98765") == OK);
    ASSERT_TRUE(kv->Put("98765", "98765!") == OK) << pmemobj_errormsg();
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace5aTest) {
    tx_alloc_should_fail = true;
    ASSERT_TRUE(kv->Put(LONGSTR, "1") == FAILED);
    ASSERT_TRUE(kv->Put(LONGSTR, LONGSTR) == FAILED);
    tx_alloc_should_fail = false;
    Validate();
}

TEST_F(MVOidFullTest, OutOfSpace5bTest) {
    tx_alloc_should_fail = true;
    for (int i = 0; i <= 99999; i++) {
        ASSERT_TRUE(kv->Put(LONGSTR, "1") == FAILED);
        ASSERT_TRUE(kv->Put(LONGSTR, LONGSTR) == FAILED);
    }
    tx_alloc_should_fail = false;
    ASSERT_TRUE(kv->Remove("34567") == OK);
    ASSERT_TRUE(kv->Put("34567", "34567!") == OK) << pmemobj_errormsg();
    Validate();
}

//TEST_F(MVOidFullTest, OutOfSpace6Test) {
//    tx_alloc_should_fail = true;
//    ASSERT_TRUE(kv->Put(LONGSTR, "?") == FAILED);
//    tx_alloc_should_fail = false;
//    std::string str;
//    ASSERT_TRUE(kv->Get(LONGSTR, &str) == NOT_FOUND) << pmemobj_errormsg();
//    Validate();
//}

TEST_F(MVOidFullTest, RepeatedRecoveryTest) {
    for (int i = 1; i <= 100; i++) Reopen();
    Validate();
}
