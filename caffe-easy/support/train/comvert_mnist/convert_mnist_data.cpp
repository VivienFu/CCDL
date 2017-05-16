// This script converts the MNIST dataset to a lmdb (default) or
// leveldb (--backend=leveldb) format used by caffe to load data.
// Usage:
//    convert_mnist_data [FLAGS] input_image_file input_label_file
//                        output_db_file
// The MNIST dataset could be downloaded at
//    http://yann.lecun.com/exdb/mnist/

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/text_format.h>
#include <support-common.h>

#if defined(USE_LMDB)
//#include <leveldb/db.h>
//#include <leveldb/write_batch.h>
#include <lmdb.h>
#endif

#if defined(_MSC_VER)
#include <direct.h>
#define mkdir(X, Y) _mkdir(X)
#endif

#include <stdint.h>
#include <sys/stat.h>

#include <fstream>  // NOLINT(readability/streams)
#include <string>

#include "boost/scoped_ptr.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/db.hpp"
#include "caffe/util/format.hpp"
#include <Windows.h>
#if defined(USE_LMDB)

using namespace caffe;  // NOLINT(build/namespaces)
using boost::scoped_ptr;
using std::string;

DEFINE_string(backend, "lmdb", "The backend for storing the result");

uint32_t swap_endian(uint32_t val) {
    val = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0xFF00FF);
    return (val << 16) | (val >> 16);
}

void convert_dataset(const char* image_filename, const char* label_filename,
        const char* db_path, const string& db_backend) {
  // Open files
  std::ifstream image_file(image_filename, std::ios::in | std::ios::binary);
  std::ifstream label_file(label_filename, std::ios::in | std::ios::binary);
  CHECK(image_file) << "Unable to open file " << image_filename;
  CHECK(label_file) << "Unable to open file " << label_filename;
  // Read the magic and the meta data
  uint32_t magic;
  uint32_t num_items;
  uint32_t num_labels;
  uint32_t rows;
  uint32_t cols;

  image_file.read(reinterpret_cast<char*>(&magic), 4);
  magic = swap_endian(magic);
  CHECK_EQ(magic, 2051) << "Incorrect image file magic.";
  label_file.read(reinterpret_cast<char*>(&magic), 4);
  magic = swap_endian(magic);
  CHECK_EQ(magic, 2049) << "Incorrect label file magic.";
  image_file.read(reinterpret_cast<char*>(&num_items), 4);
  num_items = swap_endian(num_items);
  label_file.read(reinterpret_cast<char*>(&num_labels), 4);
  num_labels = swap_endian(num_labels);
  CHECK_EQ(num_items, num_labels);
  image_file.read(reinterpret_cast<char*>(&rows), 4);
  rows = swap_endian(rows);
  image_file.read(reinterpret_cast<char*>(&cols), 4);
  cols = swap_endian(cols);


  scoped_ptr<db::DB> db(db::GetDB(db_backend));
  db->Open(db_path, db::NEW);
  scoped_ptr<db::Transaction> txn(db->NewTransaction());

  // Storing to db
  char label;
  char* pixels = new char[rows * cols];
  int count = 0;
  string value;

  Datum datum;
  datum.set_channels(1);
  datum.set_height(rows);
  datum.set_width(cols);
  LOG(INFO) << "A total of " << num_items << " items.";
  LOG(INFO) << "Rows: " << rows << " Cols: " << cols;
  for (int item_id = 0; item_id < num_items; ++item_id) {
    image_file.read(pixels, rows * cols);
    label_file.read(&label, 1);
    datum.set_data(pixels, rows*cols);
    datum.set_label(label);
    string key_str = caffe::format_int(item_id, 8);
    datum.SerializeToString(&value);

    txn->Put(key_str, value);

    if (++count % 1000 == 0) {
      txn->Commit();
    }
  }
  // write the last batch
  if (count % 1000 != 0) {
      txn->Commit();
  }
  LOG(INFO) << "Processed " << count << " files.";
  delete[] pixels;
  db->Close();
}

extern "C" int __stdcall convert_mnist(char* args) {
#ifndef GFLAGS_GFLAGS_H_
  namespace gflags = google;
#endif

  int argc = 0;
  char** argv = 0;

  vector<string> argsvec;
  char path[260];
  GetModuleFileNameA(0, path, sizeof(path));
  argsvec.push_back(path);

  char* token = strtok(args, " ");
  while (token){
	  argsvec.push_back(token);
	  token = strtok(0, " ");
  }

  argc = argsvec.size();
  argv = new char*[argc];
  for (int i = 0; i < argc; ++i){
	  argv[i] = (char*)argsvec[i].c_str();
  }

  FLAGS_alsologtostderr = 1;

  gflags::SetUsageMessage("This script converts the MNIST dataset to\n"
        "the lmdb/leveldb format used by Caffe to load data.\n"
        "Usage:\n"
        "    convert_mnist_data [FLAGS] input_image_file input_label_file "
        "output_db_file\n"
        "The MNIST dataset could be downloaded at\n"
        "    http://yann.lecun.com/exdb/mnist/\n"
        "You should gunzip them after downloading,"
        "or directly use data/mnist/get_mnist.sh\n");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  const string& db_backend = FLAGS_backend;

  if (argc != 4) {
    gflags::ShowUsageWithFlagsRestrict(argv[0],
        "examples/mnist/convert_mnist_data");
  } else {
    
    convert_dataset(argv[1], argv[2], argv[3], db_backend);
  }
  delete[]argv;
  return 0;
}

#if 0
BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved  
	)  
{  
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
		google::InitGoogleLogging("");
	return TRUE;  
}  
#endif

#else
int main(int argc, char** argv) {
  LOG(FATAL) << "This example requires LevelDB and LMDB; " <<
  "compile with USE_LEVELDB and USE_LMDB.";
}
#endif  // USE_LEVELDB and USE_LMDB