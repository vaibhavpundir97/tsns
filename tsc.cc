#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <csignal>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce662::Message;
using csce662::ListReply;
using csce662::Request;
using csce662::Reply;
using csce662::SNSService;

void sig_ignore(int sig) {
  std::cout << "Signal caught " + sig;
}

Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}


class Client : public IClient
{
public:
  Client(const std::string& hname,
	 const std::string& uname,
	 const std::string& p)
    :hostname(hname), username(uname), port(p) {}

  
protected:
  virtual int connectTo();
  virtual IReply processCommand(std::string& input);
  virtual void processTimeline();

private:
  std::string hostname;
  std::string username;
  std::string port;
  
  // You can have an instance of the client stub
  // as a member variable.
  std::unique_ptr<SNSService::Stub> stub_;
  
  IReply Login();
  IReply List();
  IReply Follow(const std::string &username);
  IReply UnFollow(const std::string &username);
  void   Timeline(const std::string &username);
};


///////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////
int Client::connectTo()
{
  // ------------------------------------------------------------
  // In this function, you are supposed to create a stub so that
  // you call service methods in the processCommand/porcessTimeline
  // functions. That is, the stub should be accessible when you want
  // to call any service methods in those functions.
  // Please refer to gRpc tutorial how to create a stub.
  // ------------------------------------------------------------
    
///////////////////////////////////////////////////////////
// YOUR CODE HERE
//////////////////////////////////////////////////////////

  // Create client stub by using NewStub function
  std::string login_info = hostname + ":" + port;
  stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
    grpc::CreateChannel(
    login_info, grpc::InsecureChannelCredentials())));
  IReply ire = Login();
  // If the user is already logged in, return -1
  // to fail the connection
  if(ire.comm_status == FAILURE_ALREADY_EXISTS)
    return -1;
  return 1;
}

IReply Client::processCommand(std::string& input)
{
  // ------------------------------------------------------------
  // GUIDE 1:
  // In this function, you are supposed to parse the given input
  // command and create your own message so that you call an 
  // appropriate service method. The input command will be one
  // of the followings:
  //
  // FOLLOW <username>
  // UNFOLLOW <username>
  // LIST
  // TIMELINE
  // ------------------------------------------------------------
  
  // ------------------------------------------------------------
  // GUIDE 2:
  // Then, you should create a variable of IReply structure
  // provided by the client.h and initialize it according to
  // the result. Finally you can finish this function by returning
  // the IReply.
  // ------------------------------------------------------------
  
  
  // ------------------------------------------------------------
  // HINT: How to set the IReply?
  // Suppose you have "FOLLOW" service method for FOLLOW command,
  // IReply can be set as follow:
  // 
  //     // some codes for creating/initializing parameters for
  //     // service method
  //     IReply ire;
  //     grpc::Status status = stub_->FOLLOW(&context, /* some parameters */);
  //     ire.grpc_status = status;
  //     if (status.ok()) {
  //         ire.comm_status = SUCCESS;
  //     } else {
  //         ire.comm_status = FAILURE_NOT_EXISTS;
  //     }
  //      
  //      return ire;
  // 
  // IMPORTANT: 
  // For the command "LIST", you should set both "all_users" and 
  // "following_users" member variable of IReply.
  // ------------------------------------------------------------

  IReply ire;
  
  /*********
  YOUR CODE HERE
  **********/

  // Parse input to find which command is passed by the user and
  // call the appropriate function with the arguments passed (if any).
  if(input.rfind("FOLLOW", 0) == 0) {
    const std::string arg = input.substr(7, std::string::npos);
    return Follow(arg);
  } else if(input.rfind("UNFOLLOW", 0) == 0) {
    const std::string arg = input.substr(9, std::string::npos);
    return UnFollow(arg);
  } else if(input == "LIST") {
    return List();
  } else if(input == "TIMELINE") {
    ire.comm_status = SUCCESS;
    return ire;
  }

  // If none of the above commands matches the command given by the user
  // set comm_status to FAILURE_INVALID
  ire.comm_status = FAILURE_INVALID;
  return ire;
}


void Client::processTimeline()
{
    Timeline(username);
}

// List Command
IReply Client::List() {

  IReply ire;

  /*********
  YOUR CODE HERE
  **********/

  // Context for the client
  ClientContext context;
  // Data to be sent to the server
  Request request;
  // variable to be used to store the reply
  ListReply list_reply;

  request.set_username(username);

  // Call client stub with appropriate parameters
  Status status = stub_->List(&context, request, &list_reply);
  ire.grpc_status = status;
  if(status.ok()) {
    ire.comm_status = SUCCESS;
    // Loop through list_reply.all_users() to populate
    // ire.all_users
    for(std::string user : list_reply.all_users())
      ire.all_users.push_back(user);
    // Loop through list_reply.followers() to populate
    // ire.followers
    for(std::string follower : list_reply.followers())
      ire.followers.push_back(follower);
  }

  return ire;
}

// Follow Command        
IReply Client::Follow(const std::string& username2) {

  IReply ire; 
    
  /***
  YOUR CODE HERE
  ***/

  // Context for the client
  ClientContext context;
  // Data to be sent to the server
  Request request;
  // Variable to be used to store the reply
  Reply reply;

  request.set_username(username);
  request.add_arguments(username2);

  // Make call to the client stub with appropriate parameters
  Status status = stub_->Follow(&context, request, &reply);
  ire.grpc_status = status;
  if(status.ok()) {
    // If user is not found on the server db
    if(reply.msg() == "ERROR -- Invalid username")
      ire.comm_status = FAILURE_INVALID_USERNAME;
    // If the current user is already following user with username2
    else if(reply.msg() == "ERROR -- Already following")
      ire.comm_status = FAILURE_ALREADY_EXISTS;
    // If follow command is successful
    else if(reply.msg() == "SUCCESS -- Following")
      ire.comm_status = SUCCESS;
    else
      ire.comm_status = FAILURE_UNKNOWN;
  }
  return ire;
}

// UNFollow Command  
IReply Client::UnFollow(const std::string& username2) {

  IReply ire;

  /***
  YOUR CODE HERE
  ***/

  // Context for the client
  ClientContext context;
  // Data to be sent to the server
  Request request;
  // Variable to be used to store the reply
  Reply reply;

  request.set_username(username);
  request.add_arguments(username2);

  // Make call to the client stub
  Status status = stub_->UnFollow(&context, request, &reply);
  ire.grpc_status = status;
  if(status.ok()) {
    // If user is not found on the server db
    if(reply.msg() == "ERROR -- Invalid username") 
      ire.comm_status = FAILURE_INVALID_USERNAME;
    // If the current user is not following user with username2
    else if(reply.msg() == "ERROR -- Not following")
      ire.comm_status = FAILURE_NOT_EXISTS;
    // If the unfollow command is successful
    else if(reply.msg() == "SUCCESS -- Unfollowed")
      ire.comm_status = SUCCESS;
    else
      ire.comm_status = FAILURE_UNKNOWN;
  }
  return ire;
}

// Login Command  
IReply Client::Login() {

  IReply ire;

  /***
   YOUR CODE HERE
  ***/
  // Context for the client
  ClientContext context;
  // Data to be sent to the server
  Request request;
  // Variable to be used to store the reply
  Reply reply;
  
  request.set_username(username);

  // Make call to the client stub
  Status status = stub_->Login(&context, request, &reply);
  ire.grpc_status = status;
  if(status.ok()) {
    // If the user is already logged in
    if(reply.msg() == "Already logged in")
      ire.comm_status = FAILURE_ALREADY_EXISTS;
    // If login succeeds
    else if(reply.msg() == "Login Successful!")
      ire.comm_status = SUCCESS;
    else
      ire.comm_status = FAILURE_UNKNOWN;
  }
  return ire;
}

// Timeline Command
void Client::Timeline(const std::string& username) {

  // ------------------------------------------------------------
  // In this function, you are supposed to get into timeline mode.
  // You may need to call a service method to communicate with
  // the server. Use getPostMessage/displayPostMessage functions 
  // in client.cc file for both getting and displaying messages 
  // in timeline mode.
  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // IMPORTANT NOTICE:
  //
  // Once a user enter to timeline mode , there is no way
  // to command mode. You don't have to worry about this situation,
  // and you can terminate the client program by pressing
  // CTRL-C (SIGINT)
  // ------------------------------------------------------------

  /***
  YOUR CODE HERE
  ***/

  // Context for the client
  ClientContext context;
  // Variable to be used for bidirectional read-write stream
  std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
    stub_->Timeline(&context));

  // writer thread that is used to write post to the stream
  // to send to the server
  std::thread writer([username, stream]() {
    Message msg;
    std::string input = "initialize timeline";
    msg = MakeMessage(username, input);
    stream->Write(msg);
    while(true) {
      input = getPostMessage();
      msg = MakeMessage(username, input);
      stream->Write(msg);
    }
    stream->WritesDone();
  });

  // reader thread to read messages sent by the server
  std::thread reader([username, stream]() {
    Message msg;
    while(stream->Read(&msg)) {
      google::protobuf::Timestamp timestamp = msg.timestamp();
      std::time_t time = timestamp.seconds();
      displayPostMessage(msg.username(), msg.msg(), time);
    }
  });

  // Wait for the threads to complete execution
  writer.join();
  reader.join();
}



//////////////////////////////////////////////
// Main Function
/////////////////////////////////////////////
int main(int argc, char** argv) {

  std::string hostname = "localhost";
  std::string username = "default";
  std::string port = "3010";
    
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
    switch(opt) {
    case 'h':
      hostname = optarg;break;
    case 'u':
      username = optarg;break;
    case 'p':
      port = optarg;break;
    default:
      std::cout << "Invalid Command Line Argument\n";
    }
  }
      
  std::cout << "Logging Initialized. Client starting...";
  
  Client myc(hostname, username, port);
  
  myc.run();
  
  return 0;
}
