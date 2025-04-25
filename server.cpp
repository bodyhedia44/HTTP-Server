#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <thread>
#include <fstream>

struct HttpRequest {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::string body;
};

int initServerAndSocket();
HttpRequest parseRequest(const std::string& rawRequest);
std::string makeResponse(int statusCode, const std::string& contentType,const std::string& body);
std::vector<std::string> parsePath(const std::string& path);
void handleRequest(int client,HttpRequest request, std::vector<std::string> path,std::string dir = "");
void handleClient(int client_fd,std::string dir);


int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  std::string dir;
  if (argc == 3 && strcmp(argv[1], "--directory") == 0)
  {
  	dir = argv[2];
  } 

  int server_fd = initServerAndSocket();
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";

  while (true)
  {
      int client = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
      if (client < 0) {
        std::cerr << "accept failed\n";
        continue;
      }
      std::cout << "Client connected " << client << "\n"; 

      std::thread client_thread(handleClient, client,dir);
      client_thread.detach();
  }
  
  close(server_fd);

  return 0;
}



int initServerAndSocket(){
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }

  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }

  return server_fd;
}

void handleClient(int client_fd,std::string dir) {
  char msg[65536] = {};
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);

  if (recvfrom(client_fd, msg, sizeof(msg) - 1, 0, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len) == -1) {
    std::cerr << "recvfrom failed on client fd " << client_fd << "\n";
    close(client_fd);
    return;
  }
  std::string receivedData(msg);
  HttpRequest request = parseRequest(receivedData);
  std::vector<std::string> path = parsePath(request.path);
  handleRequest(client_fd, request, path,dir);

  close(client_fd);
}

HttpRequest parseRequest(const std::string& rawRequest) {
  HttpRequest request;
  std::istringstream requestStream(rawRequest);
  std::string line;

  if (std::getline(requestStream, line)) {
      std::istringstream lineStream(line);
      lineStream >> request.method >> request.path;
  }

  while (std::getline(requestStream, line) && !line.empty() && line != "\r") {
      size_t colonPos = line.find(':');
      if (colonPos != std::string::npos) {
          std::string headerName = line.substr(0, colonPos);
          std::string headerValue = line.substr(colonPos + 1);
          size_t firstNotSpace = headerValue.find_first_not_of(" \t");
          if (std::string::npos != firstNotSpace) {
              headerValue.erase(0, firstNotSpace);
          }
          size_t lastNotSpace = headerValue.find_last_not_of(" \t\r");
          if (std::string::npos != lastNotSpace) {
              headerValue.erase(lastNotSpace + 1);
          }
          request.headers[headerName] = headerValue;
      }
  }

  std::stringstream bodyStream;
  while (std::getline(requestStream, line)) {
      bodyStream << line << "\n";
  }
  request.body = bodyStream.str();
  if (!request.body.empty() && request.body.back() == '\n') {
      request.body.pop_back();
  }

  return request;
}

std::string makeResponse(int statusCode, const std::string& contentType,const std::string& body) {
  std::stringstream response;

  response << "HTTP/1.1 " << statusCode << " ";

  switch (statusCode) {
      case 200: response << "OK"; break;
      case 201: response << "Created"; break;
      case 204: response << "No Content"; break;
      case 400: response << "Bad Request"; break;
      case 404: response << "Not Found"; break;
      case 500: response << "Internal Server Error"; break;
      default: response << "Unknown Status"; break;
  }
  response << "\r\n";

  response << "Content-Type: " << contentType << "\r\n";
  response << "Content-Length: " << body.length() << "\r\n";
  response << "\r\n";


  response << body;

  

  return response.str();
}

std::vector<std::string> parsePath(const std::string& path) {
  std::vector<std::string> segments;
  std::stringstream ss(path);
  std::string segment;

  if (path.rfind('/', 0) == 0 && path.length() > 1) {
      ss.ignore(1);
  }

  while (std::getline(ss, segment, '/')) {
      if (!segment.empty()) {
          segments.push_back(segment);
      }
  }
  return segments;
}

void handleRequest(int client,HttpRequest request, std::vector<std::string> path,std::string dir){
    std::string messsage = "";

    if ( path.size() == 0){
      messsage = makeResponse(200,"text/plain","");
    }else if(path[0] == "user-agent") {
      messsage = makeResponse(200,"text/plain",request.headers["User-Agent"]);
    }
    else if(path[0] == "echo") {
      messsage = makeResponse(200,"text/plain",path[1]);
    }
    else if(path[0] == "files") {

      if (request.method == "GET"){
        std::string fileContent;
        std::ifstream MyReadFile(dir + path[1]);
        if (MyReadFile.good())
        {
          while (getline (MyReadFile, fileContent)) {}
          messsage = makeResponse(200,"application/octet-stream",fileContent);
        }else{
          messsage = makeResponse(404,"text/plain","");
        }
        MyReadFile.close();

      }else{

        std::ofstream MyFile(dir + path[1]);
        MyFile << request.body;
        MyFile.close();
        messsage = makeResponse(201,"text/plain","");


      }
    }else{
      messsage = makeResponse(404,"text/plain","");
    }

    send(client, messsage.c_str(), messsage.size(), 0);
}