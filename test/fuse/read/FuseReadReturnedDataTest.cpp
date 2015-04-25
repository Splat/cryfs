#include <messmer/cpp-utils/data/DataBlockFixture.h>
#include "testutils/FuseReadTest.h"

#include "../../../fuse/FuseErrnoException.h"

#include <tuple>
#include <cstdlib>

using ::testing::_;
using ::testing::StrEq;
using ::testing::WithParamInterface;
using ::testing::Values;
using ::testing::Combine;
using ::testing::Eq;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::Action;

using std::tuple;
using std::get;
using std::min;
using std::unique_ptr;
using std::make_unique;
using cpputils::DataBlockFixture;

using namespace fspp::fuse;

// We can't test the count or size parameter directly, because fuse doesn't pass them 1:1.
// It usually asks to read bigger blocks (probably does some caching).
// But we can test that the data returned from the ::read syscall is the correct data region.

struct TestData {
  TestData(): count(0), offset(0), additional_bytes_at_end_of_file(0) {}
  TestData(const tuple<size_t, off_t, size_t> &data): count(get<0>(data)), offset(get<1>(data)), additional_bytes_at_end_of_file(get<2>(data)) {}
  size_t count;
  off_t offset;
  //How many more bytes does the file have after the read block?
  size_t additional_bytes_at_end_of_file;
  size_t fileSize() {
    return count + offset + additional_bytes_at_end_of_file;
  }
};

// The testcase creates random data in memory, offers a mock read() implementation to read from this
// memory region and check methods to check for data equality of a region.
class FuseReadReturnedDataTest: public FuseReadTest, public WithParamInterface<tuple<size_t, off_t, size_t>> {
public:
  unique_ptr<DataBlockFixture> testFile;
  TestData testData;

  FuseReadReturnedDataTest() {
    testData = GetParam();
    testFile = make_unique<DataBlockFixture>(testData.fileSize());

    ReturnIsFileOnLstatWithSize(FILENAME, testData.fileSize());
    OnOpenReturnFileDescriptor(FILENAME, 0);
    EXPECT_CALL(fsimpl, read(0, _, _, _))
      .WillRepeatedly(ReadFromFile);
  }

  // This read() mock implementation reads from the stored virtual file.
  Action<int(int, void*, size_t, off_t)> ReadFromFile = Invoke([this](int, void *buf, size_t count, off_t offset) {
    return testFile->read(buf, count, offset);
  });
};
INSTANTIATE_TEST_CASE_P(FuseReadReturnedDataTest, FuseReadReturnedDataTest, Combine(Values(0,1,10,1000,1024, 10*1024*1024), Values(0, 1, 10, 1024, 10*1024*1024), Values(0, 1, 10, 1024, 10*1024*1024)));


TEST_P(FuseReadReturnedDataTest, ReturnedDataRangeIsCorrect) {
  char *buf = new char[testData.count];
  ReadFile(FILENAME, buf, testData.count, testData.offset);
  EXPECT_TRUE(testFile->fileContentEqual(buf, testData.count, testData.offset));
  delete[] buf;
}