#include <brpc/channel.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>

#include <atomic>
#include <thread>

#include "brpc/callback.h"
#include "butil/time.h"
#include "google/protobuf/message.h"
#include "press_test/req_resp.h"

DEFINE_string(service_name, "", "");
DEFINE_string(method_name, "", "");
DEFINE_string(protocol, "", "");
DEFINE_string(connection_type, "", "");
DEFINE_int32(timeout, 0, "");
DEFINE_int32(max_retry, 0, "");
DEFINE_int32(backup_timeout, 0, "");
DEFINE_int32(qps, 0, "");
DEFINE_int32(thrd_num, 1, "");
DEFINE_int64(duration, 0, "");
DEFINE_string(bns, "", "");
DEFINE_string(diff_bns, "", "");
DEFINE_string(lb, "", "");
DEFINE_string(request_message_name, "", "");
DEFINE_string(response_message_name, "", "");
DEFINE_string(proto_includes, "", "");
DEFINE_string(input_proto_file, "", "");
DEFINE_bool(once, false, " once");

std::atomic<uint64_t> total_time{0};
std::atomic<uint64_t> failed_num{0};
std::atomic<uint64_t> suc_num{0};

class ImportErrorPrinter : public google::protobuf::compiler::MultiFileErrorCollector {
 public:
  // Line and column numbers are zero-based.  A line number of -1 indicates
  // an error with the entire file (e.g. "not found").
  virtual void AddError(const std::string& filename, int line, int /*column*/,
                        const std::string& message) {
    LOG(WARNING) << filename << ":" << line << " " << message;
  }
};

std::shared_ptr<google::protobuf::Message> createMessage(const std::string& typeName) {
  google::protobuf::Message* message = nullptr;
  const google::protobuf::Descriptor* descriptor =
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(typeName);
  if (!descriptor) {
    LOG(WARNING) << "createMessage failed, message name: " << typeName << std::endl;
  } else {
    const google::protobuf::Message* prototype =
        google::protobuf::MessageFactory::generated_factory()->GetPrototype(descriptor);
    if (prototype) {
      message = prototype->New();
    } else {
      LOG(WARNING) << "createMessage failed, message name: " << typeName << std::endl;
    }
  }
  return std::shared_ptr<google::protobuf::Message>(message);
}

void diff_handle_response(std::shared_ptr<brpc::Controller> cntl,
                          std::shared_ptr<google::protobuf::Message> response) {
  if (cntl->Failed()) {
    failed_num.fetch_add(1);
    LOG(WARNING) << "rpc failed:" << cntl->ErrorText();
  } else {
    suc_num.fetch_add(1);
    diff_process_response(response);
  }
  total_time.fetch_add(cntl->latency_us());
}
void handle_response(std::shared_ptr<brpc::Controller> cntl,
                     std::shared_ptr<google::protobuf::Message> response) {
  if (cntl->Failed()) {
    failed_num.fetch_add(1);
    LOG(WARNING) << "rpc failed:" << cntl->ErrorText();
  } else {
    suc_num.fetch_add(1);
    process_response(response);
  }
  total_time.fetch_add(cntl->latency_us());
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, false);
  // google::SetCommandLineOption("flagfile", "conf/press_gflags.conf");

  initialize();

  std::string service_name = FLAGS_service_name;
  std::string method_name = FLAGS_method_name;
  std::string input_proto_file = FLAGS_input_proto_file;
  ImportErrorPrinter error_printer;

  int pos = input_proto_file.find_last_of('/');
  std::cout << "input_proto_file " << input_proto_file << std::endl;
  std::string proto_file(input_proto_file.substr(pos + 1));
  std::string proto_path(input_proto_file.substr(0, pos));
  google::protobuf::compiler::DiskSourceTree source_tree;
  // look up .proto file in the same directory
  source_tree.MapPath("", proto_path.c_str());
  LOG(WARNING) << "include path: " << proto_path;
  // Add paths in -inc
  if (!FLAGS_proto_includes.empty()) {
    butil::StringSplitter sp(FLAGS_proto_includes.c_str(), ';');
    for (; sp; ++sp) {
      source_tree.MapPath("", std::string(sp.field(), sp.length()));
      LOG(WARNING) << "include path: " << std::string(sp.field(), sp.length());
    }
  }

  auto import = new ::google::protobuf::compiler::Importer(&source_tree, &error_printer);
  auto fd = import->Import(proto_file.c_str());
  if (!fd) {
    LOG(WARNING) << "Import:" << proto_file << " failed";
  }
  const google::protobuf::ServiceDescriptor* service_desc =
      fd->pool()->FindServiceByName(service_name);
  if (service_desc == nullptr) {
    LOG(WARNING) << "get service_name:" << service_name << " nullptr";
    return -1;
  }
  const google::protobuf::MethodDescriptor* m_desc = service_desc->FindMethodByName(method_name);

  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol.c_str();
  options.connection_type = FLAGS_connection_type.c_str();
  options.timeout_ms = FLAGS_timeout;
  options.max_retry = FLAGS_max_retry;
  options.backup_request_ms = FLAGS_backup_timeout;
  std::shared_ptr<brpc::Channel> channel(new brpc::Channel());
  if (channel->Init(FLAGS_bns.c_str(), FLAGS_lb.c_str(), &options) != 0) {
    LOG(WARNING) << "Fail to initialize channel";
    return -1;
  }
  if (FLAGS_qps == 0) {
    return 0;
  }
  brpc::ChannelOptions diff_options;
  diff_options.protocol = FLAGS_protocol.c_str();
  diff_options.connection_type = FLAGS_connection_type.c_str();
  diff_options.timeout_ms = FLAGS_timeout;
  diff_options.max_retry = FLAGS_max_retry;
  diff_options.backup_request_ms = FLAGS_backup_timeout;
  std::shared_ptr<brpc::Channel> diff_channel(new brpc::Channel());
  if (diff_channel->Init(FLAGS_diff_bns.c_str(), FLAGS_lb.c_str(), &diff_options) != 0) {
    LOG(WARNING) << "Fail to initialize diff channel";
    return -1;
  }
  if (FLAGS_qps == 0) {
    return 0;
  }

  auto sleep_time_us = 1000 * 1000 * FLAGS_thrd_num / FLAGS_qps;
  auto start = butil::gettimeofday_us();
  uint64_t end_time_point = start + FLAGS_duration * 1000 * 1000;
  LOG(WARNING) << "duration: " << FLAGS_duration << "s current: " << start
               << " end: " << end_time_point << " dur: " << FLAGS_duration * 1000 * 1000;
  LOG(WARNING) << "sleep_time_us: " << sleep_time_us << "us thread num:" << FLAGS_thrd_num;

  auto request_cb = [&]() {
    while (static_cast<uint64_t>(butil::gettimeofday_us()) <= end_time_point) {
      auto begin_time = butil::gettimeofday_us();
      auto request = createMessage(FLAGS_request_message_name);
      auto response = createMessage(FLAGS_response_message_name);
      auto diff_response = createMessage(FLAGS_response_message_name);
      if (!request || !response) {
        LOG(WARNING) << "create request or response is failed";
        // usleep(sleep_time_us);
        continue;
      }
      fill_request(request);
      auto cntl = std::make_shared<brpc::Controller>();
      google::protobuf::Closure* done = brpc::NewCallback(&handle_response, cntl, response);
      channel->CallMethod(m_desc, cntl.get(), request.get(), response.get(), done);
      google::protobuf::Closure* diff_done =
          brpc::NewCallback(&diff_handle_response, cntl, diff_response);
      auto diff_cntl = std::make_shared<brpc::Controller>();
      diff_channel->CallMethod(m_desc, diff_cntl.get(), request.get(), diff_response.get(),
                               diff_done);
      auto exp_time = butil::gettimeofday_us() - begin_time;  // 执行时间
      if (exp_time < sleep_time_us) {
        usleep(sleep_time_us - exp_time);
      } else {
        LOG(WARNING) << "exp_time:" << exp_time << " >= sleep_time_us:" << exp_time;
      }
      if (FLAGS_once) {
        break;
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_thrd_num; ++i) {
    threads.emplace_back(request_cb);
  }
  for (int i = 0; i < FLAGS_thrd_num; ++i) {
    threads[i].join();
  }

  auto total_requests = suc_num.load() + failed_num.load();
  LOG(WARNING) << "total requests: " << total_requests;
  LOG(WARNING) << "suc request: " << suc_num.load() << " failed request: " << failed_num.load();
  if (total_requests != 0) {
    LOG(WARNING) << "avg time: " << total_time.load() / (float)total_requests;
  }
  return 0;
}
