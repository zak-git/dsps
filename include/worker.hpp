#ifndef WORKER_HPP
#define WORKER_HPP
#include <string>
#include <iostream>
#include <vector>
#include <chrono>

#include "caf/all.hpp"
#include "caf/io/all.hpp"

#include "base.hpp"
#include "actions.hpp"

using namespace std;
using namespace std::chrono;
using namespace caf;

//struct for worker internal state
struct worker_state {
    actor scheduler;
    vector<actor> current_servers;
};

class message;

class worker_node : public node { 
public:
    worker_node(config& cfg) : node(cfg) {
        worker_ = actor_manager->get()->system()->spawn(worker_node::worker);
        //publish worker on the internet through middleman
        this->publish(worker_); 
        localhost_ = get_local_ip();
        anon_send(worker_,connect_atom::value);
    }
    void push(const message& msg) {
        messenger_->send(msg);        
    }
    void pull(message& msg) {
        messenger_->recv(msg);
    }
    void ask_for_blocking(const block_group& group) {
        scoped_actor blocking_actor{actor_manager->get()->system()};
        blocking_actor.request(worker_,infinite,block_atom::value,group).receive(
            [&](const continue_atom ){
                aout(self) << "continue" << endl;
            },
            [&](const error& err) {
                aout(self)  << system.render(err) << endl;
            }
    }
    static void worker(stateful_actor<worker_state>* self) {
        return {
            [=](connect_atom atom) {
                auto scheduler_host = this->scheduler_host();
                auto scheduler_port = this->scheduler_port();
                auto scheduler = connect(
                    reinpreter_cast<actor*>(self),
                    host,port);
                self->state().scheduler = scheduler;
                self->request(actor_cast<actor>(scheduler),
                    infinite,connect_to_opponant_atom::value,
                    localhost_,bound_port_,
                    node_role::worker
                    );
            },
            //connect to registering server
            [=](connect_to_opponant_atom atom,
                const string& host,uint16_t port) {
                auto incoming_node = this->connect(
                    reinpreter_cast<actor*>(self),
                    host,port);
                self->state().current_servers.push_back(incoming_node);                      
            },
            //connect to existing servers
            [=](connect_back_atom atom,
                vector<pair<string,uint16_t>> server_host_and_ports) {
                for(auto server_host_and_port : server_host_and_ports) {
                    auto incoming_node = this->connect(
                    reinpreter_cast<actor*>(self),
                    server_host_and_port.fist,
                    server_host_and_port.second);
                }
            },
            [=](block_atom atom atom,const block_group& group) {
                return self->delegate(self->state().scheduler,block_atom::value,group);    
            } 
        };      
    }
private:
    messager messager_;
    uint16_t bound_port_;
    string localhost_;
    actor worker_;
}; 
#endif
