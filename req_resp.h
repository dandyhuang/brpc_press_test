#pragma once
#include <vector>
#include <string>
#include <google/protobuf/message.h>
extern std::vector<std::string> imeis;
extern std::vector<std::string> contents;
void initialize();
void fill_request(std::shared_ptr<google::protobuf::Message>& request); 

void process_response(std::shared_ptr<google::protobuf::Message> response); 
void diff_process_response(std::shared_ptr<google::protobuf::Message> response); 
