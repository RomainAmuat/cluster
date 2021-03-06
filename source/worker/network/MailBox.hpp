#ifndef MARKET_HPP
#define MARKET_HPP

#include <zmq.hpp>
#include <unistd.h>
#include <deque> 
#include <thread>
#include <string>
#include <iostream>
#include <map>

#include "../misc/protected_output.hpp"
#include "interfaces/networker.hpp"
#include "network_operations/network_operations_list.hpp"

namespace Network {
class MailBox : public Networker {
public:
	MailBox(std::string master_pub, std::string master_pull, std::string local_port) : context(1) {
		m_me = Address("tcp://localhost:" + local_port);
		read_market = master_pub;
		send_market = master_pull;

		Misc::ProtectedOut::out(std::string("Creating socket ZMQ_SUB connecting to '" + read_market + "'"));
		sockets.insert(std::pair<std::string,zmq::socket_t>(read_market, zmq::socket_t(context, ZMQ_SUB)));
		(sockets.find(read_market)->second).connect(read_market);
		(sockets.find(read_market)->second).setsockopt(ZMQ_SUBSCRIBE, "", 0);
		Misc::ProtectedOut::out(std::string("socket ZMQ_SUB to master : Ok!"));

		Misc::ProtectedOut::out(std::string("Creating socket ZMQ_PUSH connecting to '" + send_market + "'"));
		sockets.insert(std::pair<std::string,zmq::socket_t>(send_market, zmq::socket_t(context, ZMQ_PUSH)));
		(sockets.find(send_market)->second).connect(send_market);
		Misc::ProtectedOut::out(std::string("socket ZMQ_PUSH to master : Ok!"));
		
		Misc::ProtectedOut::out(std::string("Creating socket ZMQ_PUB binding to '" + m_me.adr + "'"));
		sockets.insert(std::pair<std::string,zmq::socket_t>(m_me.adr, zmq::socket_t(context, ZMQ_PUB)));
		(sockets.find(m_me.adr)->second).bind("tcp://*:" + local_port);
		Misc::ProtectedOut::out(std::string("socket ZMQ_PUB : Ok!"));

		open_shop();

		Misc::ProtectedOut::out(std::string("Sending handshake to " + master_pull));
	    send(Address(send_market), Message(std::string("tcp://localhost:5552")));
	    Misc::ProtectedOut::out(std::string("Handsake sent"));

	    wait_ms = 10;
	}

	~MailBox() {
		close_shop();
	}
	
	void open_shop() {
		Misc::ProtectedOut::out(std::string("Getting into the market"));
		read_offers = true;
		market_mng_thread = std::thread(&MailBox::answer_offers, this);
		mailbox_mng_thread = std::thread(&MailBox::check_own_box, this);
	}

	void close_shop() {
		read_offers = false;
		if (market_mng_thread.joinable())
			market_mng_thread.join();
		if (mailbox_mng_thread.joinable())
			mailbox_mng_thread.join();
	}

	void answer_offers() {
		int val = -1;
	  	zmq::message_t introduction;

	  	Misc::ProtectedOut::out(std::string("Answering offers published on '" + read_market + "'"));;

		while(read_offers) {
	        try {
		        val = (sockets.find(read_market)->second).recv(&introduction, ZMQ_NOBLOCK);
		    }
		    catch (...) {
		    	val = -1;
		    }
	        if (val >= 0 && introduction.size() > 0) {
	        	Misc::ProtectedOut::out(std::string("-- Worker -- New market offer"));;
	        	try {
			        Message smessage(introduction);
	        		Misc::ProtectedOut::out(std::string("-- Worker -- Operation : '" + smessage.operation + "'"));
			        network_operation_functor * op2do = net_op_list.get_operation(smessage.operation);
	        		Misc::ProtectedOut::out(std::string("-- Worker -- Launching Operation"));
			        (*op2do)(&smessage, this, m_operation_pool, m_operation_pool->get_resource_manager(), &net_op_list);
			        Misc::ProtectedOut::out(std::string("Actions done"));
			        //std::string smessage(static_cast<char*>(introduction.data()), introduction.size());
	        		//std::cout << "New worker : " << smessage << std::endl;
			    }
			    catch (...) {
			    	Misc::ProtectedOut::out(std::string("Error when adding new tasks"));
			    }
		  	}
			usleep(wait_ms);
		}		
	}

	void check_own_box() {
		int val = -1;
	  	zmq::message_t introduction;

	  	Misc::ProtectedOut::out(std::string("Reading Own Mailbox"));

		while(read_offers) {
	        try {
		        val = (sockets.find(m_me.adr)->second).recv(&introduction, ZMQ_NOBLOCK);
		    }
		    catch (...) {
		    	val = -1;
		    }
	        if (val >= 0 && introduction.size() > 0) {
	        	Misc::ProtectedOut::out(std::string("Message personnal mailbox"));
	        	try {
			        Message smessage(introduction);
			        network_operation_functor * op2do = net_op_list.get_operation(smessage.operation);
			        (*op2do)(&smessage, this, m_operation_pool, m_operation_pool->get_resource_manager(), &net_op_list);
			        std::cout << "Actions done" << std::endl;
			        //std::string smessage(static_cast<char*>(introduction.data()), introduction.size());
	        		//std::cout << "New worker : " << smessage << std::endl;
			    }
			    catch (...) {
			    	Misc::ProtectedOut::out(std::string("Error when adding new tasks"));
			    }
		  	}
			usleep(wait_ms);
		}		
	}

	Message send(Address adr, Message msg, bool pub_delivery=false) {
		if (msg.type == MsgReturnType::NoAnswerNeeded) {
			msg.author = m_me;
			msg.destination = adr;
			std::string msg2send = msg.construct_message();
		    zmq::message_t message(msg2send.size());
		    memcpy(message.data(), msg2send.c_str(), msg2send.size());
		    Misc::ProtectedOut::out(std::string("Mailbox : Sending '" + msg2send + "' to '" + adr.name + "'"));
			(sockets.find(adr.name)->second).send(message);
			return Message();
		}
		else {
			throw std::string("Error, network answer is not managed yet!");
		}
	}

	void connect_operation_pool(Operation::OperationPool * operation_pool) {
		m_operation_pool = operation_pool;
	}

private:
	std::map<std::string, zmq::socket_t> sockets;
	zmq::context_t context;

	Operation::OperationPool * m_operation_pool;
	std::thread market_mng_thread;
	std::thread mailbox_mng_thread;
	bool read_offers;
	int wait_ms;

	NetworkOperationList net_op_list;

	std::string read_market;
	std::string send_market;

	Address m_me;
};
};

#endif