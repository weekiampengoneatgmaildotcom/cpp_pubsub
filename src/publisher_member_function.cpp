// Copyright 2015 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <chrono>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "geometry_msgs/msg/twist.hpp"
//#include "keyop.hpp"
//#include "cpp_pubsub/msg/num.hpp" 
#include "tutorial_interfaces/msg/num.hpp"


#include "rtc/rtc.hpp"
#include "parse_cl.h"

#include <nlohmann/json.hpp>
#if 1

#include <algorithm>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#endif


using namespace std::chrono_literals;
using std::shared_ptr;
using std::weak_ptr;
template <class T> weak_ptr<T> make_weak_ptr(shared_ptr<T> ptr) { return ptr; }

using nlohmann::json;

std::string localId;
std::unordered_map<std::string, shared_ptr<rtc::PeerConnection>> peerConnectionMap;
std::unordered_map<std::string, shared_ptr<rtc::DataChannel>> dataChannelMap;

shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
                                                     weak_ptr<rtc::WebSocket> wws, std::string id);
std::string randomId(size_t length);



/* This example creates a subclass of Node and uses std::bind() to register a
 * member function as a callback from the timer. */

class MinimalPublisher : public rclcpp::Node
{
public:
  MinimalPublisher()
  : Node("minimal_publisher"), count_(0)
  {
    //publisher_ = this->create_publisher<std_msgs::msg::String>("topic", 10);
    //publisher_ = this->create_publisher<cpp_pubsub::msg::Num>("topic", 10);
    //publisher_ = this->create_publisher<tutorial_interfaces::msg::Num>("topic", 10);

    //velocity_publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 1);
    publisher_ = this->create_publisher<geometry_msgs::msg::Twist>("/velocity_smoother/input", 1);
    
    timer_ = this->create_wall_timer(
      500ms, std::bind(&MinimalPublisher::timer_callback, this));
  }

private:
  void timer_callback()
  {
    //auto message = std_msgs::msg::String();
    //message.data = "Hello, world! " + std::to_string(count_++);
    //RCLCPP_INFO(this->get_logger(), "Publishing: '%s'", message.data.c_str());
    auto message = tutorial_interfaces::msg::Num();                               // CHANGE
    message.num = this->count_++;                                        // CHANGE
    RCLCPP_INFO(this->get_logger(), "Publishing: '%d'", message.num);    // CHANGE

    publisher_->publish(message);

    




  }
  rclcpp::TimerBase::SharedPtr timer_;
  //rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  rclcpp::Publisher<tutorial_interfaces::msg::Num>::SharedPtr publisher_;
  size_t count_;
};

#if 0
int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MinimalPublisher>());
  rclcpp::shutdown();
  return 0;
}
#endif

int main(int argc, char **argv) try {
        Cmdline params(argc, argv);

	rclcpp::init(argc, argv);

        rtc::InitLogger(rtc::LogLevel::Info);

        rtc::Configuration config;
        std::string stunServer = "";
        if (params.noStun()) {
                std::cout
                    << "No STUN server is configured. Only local hosts and public IP addresses supported."
                    << std::endl;
        } else {
                if (params.stunServer().substr(0, 5).compare("stun:") != 0) {
                        stunServer = "stun:";
                }
                stunServer += params.stunServer() + ":" + std::to_string(params.stunPort());
                std::cout << "STUN server is " << stunServer << std::endl;
                config.iceServers.emplace_back(stunServer);
        }

        if (params.udpMux()) {
                std::cout << "ICE UDP mux enabled" << std::endl;
                config.enableIceUdpMux = true;
        }

        localId = randomId(4);
        std::cout << "The local ID is " << localId << std::endl;

        auto ws = std::make_shared<rtc::WebSocket>();

        std::promise<void> wsPromise;
        auto wsFuture = wsPromise.get_future();

        ws->onOpen([&wsPromise]() {
                std::cout << "WebSocket connected, signaling ready" << std::endl;
                wsPromise.set_value();
        });

        ws->onError([&wsPromise](std::string s) {
                std::cout << "WebSocket error" << std::endl;
                wsPromise.set_exception(std::make_exception_ptr(std::runtime_error(s)));
        });

        ws->onClosed([]() { std::cout << "WebSocket closed" << std::endl; });

        ws->onMessage([&config, wws = make_weak_ptr(ws)](auto data) {
                // data holds either std::string or rtc::binary
                if (!std::holds_alternative<std::string>(data))
                        return;

                json message = json::parse(std::get<std::string>(data));

                auto it = message.find("id");
                if (it == message.end())
                        return;

                auto id = it->get<std::string>();

                it = message.find("type");
                if (it == message.end())
                        return;

                auto type = it->get<std::string>();

                shared_ptr<rtc::PeerConnection> pc;
                if (auto jt = peerConnectionMap.find(id); jt != peerConnectionMap.end()) {
                        pc = jt->second;
                } else if (type == "offer") {
                        std::cout << "Answering to " + id << std::endl;
                        pc = createPeerConnection(config, wws, id);
                } else {
                        return;
                }

                if (type == "offer" || type == "answer") {
                        auto sdp = message["description"].get<std::string>();
                        pc->setRemoteDescription(rtc::Description(sdp, type));
                } else if (type == "candidate") {
                        auto sdp = message["candidate"].get<std::string>();
                        auto mid = message["mid"].get<std::string>();
                        pc->addRemoteCandidate(rtc::Candidate(sdp, mid));
                }
        });

        const std::string wsPrefix =
            params.webSocketServer().find("://") == std::string::npos ? "ws://" : "";
        const std::string url = wsPrefix + params.webSocketServer() + ":" +
                                std::to_string(params.webSocketPort()) + "/" + localId;

        std::cout << "WebSocket URL is " << url << std::endl;
        ws->open(url);

        std::cout << "Waiting for signaling to be connected..." << std::endl;
        wsFuture.get();

        while (true) {
                std::string id;
                std::cout << "Enter a remote ID to send an offer:" << std::endl;
                std::cin >> id;
                std::cin.ignore();

                if (id.empty())
                        break;

                if (id == localId) {
                        std::cout << "Invalid remote ID (This is the local ID)" << std::endl;
                        continue;
                }

                std::cout << "Offering to " + id << std::endl;
                auto pc = createPeerConnection(config, ws, id);

                // We are the offerer, so create a data channel to initiate the process
                const std::string label = "test";
                std::cout << "Creating DataChannel with label \"" << label << "\"" << std::endl;
                auto dc = pc->createDataChannel(label);

                dc->onOpen([id, wdc = make_weak_ptr(dc)]() {
                        std::cout << "1DataChannel from " << id << " open" << std::endl;
                        if (auto dc = wdc.lock())
                                dc->send("Hello from " + localId);
			//rclcpp::init();
			rclcpp::spin(std::make_shared<MinimalPublisher>());
                });

                dc->onClosed([id]() { 
			std::cout << "1DataChannel from " << id << " closed" << std::endl;
			rclcpp::shutdown();       
				});

                dc->onMessage([id, wdc = make_weak_ptr(dc)](auto data) {
                        // data holds either std::string or rtc::binary
                        if (std::holds_alternative<std::string>(data))
                                std::cout << "1Message from " << id << " received: " << std::get<std::string>(data)
                                          << std::endl;
                        else
                                std::cout << "1Binary message from " << id
                                          << " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
                });

                dataChannelMap.emplace(id, dc);
        }

        std::cout << "Cleaning up..." << std::endl;

        dataChannelMap.clear();
        peerConnectionMap.clear();
        return 0;

} catch (const std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
        dataChannelMap.clear();
        peerConnectionMap.clear();
        return -1;
}




// Create and setup a PeerConnection
shared_ptr<rtc::PeerConnection> createPeerConnection(const rtc::Configuration &config,
                                                     weak_ptr<rtc::WebSocket> wws, std::string id) {
        auto pc = std::make_shared<rtc::PeerConnection>(config);

        pc->onStateChange(
            [](rtc::PeerConnection::State state) { std::cout << "State: " << state << std::endl; });

        pc->onGatheringStateChange([](rtc::PeerConnection::GatheringState state) {
                std::cout << "Gathering State: " << state << std::endl;
        });

        pc->onLocalDescription([wws, id](rtc::Description description) {
                json message = {{"id", id},
                                {"type", description.typeString()},
                                {"description", std::string(description)}};

                if (auto ws = wws.lock())
                        ws->send(message.dump());
        });

        pc->onLocalCandidate([wws, id](rtc::Candidate candidate) {
                json message = {{"id", id},
                                {"type", "candidate"},
                                {"candidate", std::string(candidate)},
                                {"mid", candidate.mid()}};

                if (auto ws = wws.lock())
                        ws->send(message.dump());
        });

        pc->onDataChannel([id](shared_ptr<rtc::DataChannel> dc) {
                std::cout << "2DataChannel from " << id << " received with label \"" << dc->label() << "\""
                          << std::endl;

                dc->onOpen([wdc = make_weak_ptr(dc)]() {
                        if (auto dc = wdc.lock())
                                dc->send("Hello from " + localId);
                });

                dc->onClosed([id]() { std::cout << "2DataChannel from " << id << " closed" << std::endl; });

                dc->onMessage([id](auto data) {
                        // data holds either std::string or rtc::binary
                        if (std::holds_alternative<std::string>(data))
                                std::cout << "2Message from " << id << " received: " << std::get<std::string>(data)
                                          << std::endl;
                        else
                                std::cout << "2Binary message from " << id
                                          << " received, size=" << std::get<rtc::binary>(data).size() << std::endl;
                });

                dataChannelMap.emplace(id, dc);
        });

        peerConnectionMap.emplace(id, pc);
        return pc;
}


// Helper function to generate a random ID
std::string randomId(size_t length) {
        static const std::string characters(
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
        std::string id(length, '0');
        std::default_random_engine rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, int(characters.size() - 1));
        std::generate(id.begin(), id.end(), [&]() { return characters.at(dist(rng)); });
        return id;
}


