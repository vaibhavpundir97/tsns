/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"


using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce662::Message;
using csce662::ListReply;
using csce662::Request;
using csce662::Reply;
using csce662::SNSService;


struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client*> client_db;

Client * search_user(std::string username) {
    for(auto & user : client_db) {
        if(user->username == username)
          return user;
    }
    return nullptr;
}

bool is_following(std::vector<Client*> following_list, Client client) {
  for(auto &f : following_list) {
    if(client == *f)
      return true;
  }
  return false;
}

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    /*********
    YOUR CODE HERE
    **********/
    
    Client * user = search_user(request->username());
    for(auto & c : client_db) {
        list_reply->add_all_users(c->username);
    }

    list_reply->add_followers(user->username);
    for(auto & follower : user->client_followers) {
        list_reply->add_followers(follower->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/

    Client * user = search_user(request->username());
    Client * f_user = search_user(request->arguments(0));
    if(!f_user) {
        reply->set_msg("ERROR -- Invalid username");
    } else if(*user == *f_user || is_following(user->client_following,
        *f_user)) {
        reply->set_msg("ERROR -- Already following");
    } else {
        user->client_following.push_back(f_user);
        f_user->client_followers.push_back(user);
        reply->set_msg("SUCCESS -- Following");
    }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/

    Client * user = search_user(request->username());
    Client * f_user = search_user(request->arguments(0));
    if(!f_user || *user == *f_user) {
        reply->set_msg("ERROR -- Invalid username");
    } else if(!is_following(user->client_following, *f_user)) {
        reply->set_msg("ERROR -- Not following");
    } else {
        std::vector<Client*>::iterator iter = user->client_following.begin();
        for(iter; iter < user->client_following.end(); ++iter) {
          if(**iter == *f_user)
            break;
        }
        user->client_following.erase(iter);
        iter = f_user->client_followers.begin();
        for(iter; iter < f_user->client_followers.end(); ++iter) {
          if(**iter == *user)
            break;
        }
        f_user->client_followers.erase(iter);
        reply->set_msg("SUCCESS -- Unfollowed");
    }
    return Status::OK;
  }

  // RPC Login
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {

    /*********
    YOUR CODE HERE
    **********/
    if(search_user(request->username())) {
      reply->set_msg("Already logged in");
    } else {
      Client * user = new Client();
      user->username = request->username();
      client_db.push_back(user);
      reply->set_msg("Login Successful!");
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {

    /*********
    YOUR CODE HERE
    **********/

    Message msg;

    while(stream->Read(&msg)) {
      Client * user = search_user(msg.username());
      std::string file_name = user->username + ".txt";
      std::ofstream user_file(file_name,
                              std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp timestamp = msg.timestamp();
      std::int64_t time = google::protobuf::util::TimeUtil::TimestampToSeconds(
        timestamp);
      std::string input = std::to_string(time) + "|" + msg.username() + ":" +
                          msg.msg();

      if(msg.msg() != "initialize timeline") {
        user_file << input;

        for(auto & follower : user->client_followers) {
          if(follower->stream != 0)
            follower->stream->Write(msg);

          std::string follower_file = follower->username + "_following.txt";
          std::ofstream following_file(follower_file,
                                      std::ios::app|std::ios::out|std::ios::in);
          following_file << input;
          follower->following_file_size++;
        }

      } else {
        if(user->stream == 0)
          user->stream = stream;
        std::string line;
        std::vector<std::string> latest_20;
        std::ifstream following_file(user->username + "_following.txt");

        int i = 0;
        while(getline(following_file, line)) {
          if(user->following_file_size > 20 &&
            i < user->following_file_size - 20) {
            ++i;
            continue;
          }
          latest_20.push_back(line);
        }

        reverse(latest_20.begin(), latest_20.end());
        Message _msg;
        for(auto & post : latest_20) {
          int i1 = post.find('|');
          int i2 = post.find(':');

          _msg.set_username(post.substr(i1 + 1, i2 - i1 - 1));
          _msg.set_msg(post.substr(i2 + 1, std::string::npos));

          google::protobuf::Timestamp* msg_time =
            new google::protobuf::Timestamp();
          msg_time->set_seconds(std::stoll(post.substr(0, i1)));
          msg_time->set_nanos(0);
          _msg.set_allocated_timestamp(msg_time);
          stream->Write(_msg);
        }
      }
    }
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

int main(int argc, char** argv) {

  std::string port = "3010";
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::string log_file_name = std::string("server-") + port;
  google::InitGoogleLogging(log_file_name.c_str());
  log(INFO, "Logging Initialized. Server starting...");
  RunServer(port);

  return 0;
}
