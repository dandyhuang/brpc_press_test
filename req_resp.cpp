#include "req_resp.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/util/json_util.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>

#include "butil/time.h"
//#include "proto/fs.pb.h"
DEFINE_bool(print_request, false, "");
DEFINE_bool(print_response, false, "");
DEFINE_bool(save_response, false, "");
DEFINE_int32(message_mode, 0, "");
DEFINE_int32(item_num, 100, "");

size_t send_item_num = FLAGS_item_num;
std::vector<std::string> imeis;
std::vector<std::string> items;

void initialize() {
  {
    std::ifstream fin("conf/test/imeis.txt");
    imeis.reserve(1000);
    std::string s;
    while (getline(fin, s)) {
      imeis.push_back(s);
    }
    LOG(WARNING) << "imeis size: " << imeis.size();
  }
  {
    std::ifstream fin("conf/test/items.txt");
    items.reserve(1000);
    std::string s;
    while (getline(fin, s)) {
      items.push_back(s);
    }
    send_item_num = std::min(send_item_num, items.size());
    LOG(WARNING) << "items size: " << items.size() << "send item size:" << send_item_num;
  }
}

static std::string message_to_json(const google::protobuf::Message& message) {
  std::string json_string;
  google::protobuf::util::JsonPrintOptions options;
  auto status = google::protobuf::util::MessageToJsonString(message, &json_string, options);
  return json_string;
}

void fill_request(std::shared_ptr<google::protobuf::Message>& request, const std::string& file) {
  std::ifstream in(file);
  if (!in.is_open()) {
    LOG(WARNING) << "Error opening file";
    exit(1);
  }
  std::vector<std::string> data;
  while (!in.eof()) {
    std::string s;
    getline(in, s);
    data.push_back(s);
  }
  in.close();

  std::string json;
  for (auto v : data) {
    json += v;
  }
  LOG(INFO) << "data size:" << data.size() << " | json:" << json;

  google::protobuf::util::JsonParseOptions options;
  options.ignore_unknown_fields = true;
  google::protobuf::util::Status status =
      google::protobuf::util::JsonStringToMessage(json, request.get(), options);
}

void fill_request(std::shared_ptr<google::protobuf::Message>& request) {
//  fs::interface::FsRequest req;
//  auto time_us = butil::gettimeofday_us();
//  auto imei_idx = time_us % imeis.size();
//  auto item_idx = time_us % items.size();
//  req.set_user_id(imeis[imei_idx]);
//  auto item_num = FLAGS_item_num > items.size() ? items.size() : FLAGS_item_num;
//  for (int i = 0; i < item_num; i++) {
//    auto idx = i + item_idx;
//    if (idx >= items.size()) {
//      idx %= items.size();
//    }
//    req.add_ad_id(items[idx]);
//  }
//  ((fs::interface::FsRequest*)(request.get()))->Swap(&req);
//  if (FLAGS_print_request) {
//    std::cout << "request:" << message_to_json(*request) << std::endl;
//  }
}
void diff_process_response(std::shared_ptr<google::protobuf::Message> response) {
  if (FLAGS_print_response) {
    LOG(WARNING) << "response: \n" << message_to_json(*response);
  }
  if (FLAGS_save_response) {
    std::ofstream fout;
    fout.open("./response_2.txt", std::ios::out);
    fout << message_to_json(*response);
    fout.flush();
  }
}
void process_response(std::shared_ptr<google::protobuf::Message> response) {
  if (FLAGS_print_response) {
    std::cout << "response: \n" << message_to_json(*response) << std::endl;
  }
  if (FLAGS_save_response) {
    std::ofstream fout;
    fout.open("./response.txt", std::ios::out);
    fout << message_to_json(*response);
    fout.flush();
  }
}
