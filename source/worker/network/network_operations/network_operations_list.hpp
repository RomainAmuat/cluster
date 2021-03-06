#ifndef NETWORK_OPERATIONS_LIST_HPP
#define NETWORK_OPERATIONS_LIST_HPP

#include "interface/network_operation_function.hpp"
#include "netop_new_worker.hpp"
#include "task_related_ops/netop_math_task.hpp"


namespace Network {
class NetworkOperationList {
public:
	NetworkOperationList() { 
		network_operation_functor * new_worker = new netop_new_worker();
		m_operations.insert(std::pair<std::string, network_operation_functor *>("0", new_worker));

		network_operation_functor * math_task = new netop_math_task();
		m_operations.insert(std::pair<std::string, network_operation_functor *>("331", math_task));
	}
	~NetworkOperationList() { 
		// TODO : I have a small memory leak there, I should free functors before exiting program
		;
	}

	network_operation_functor * get_operation(std::string operation_name) {
		return (m_operations.find(operation_name))->second;
	}

private:
	std::map<std::string, network_operation_functor *> m_operations;
};
};

#endif