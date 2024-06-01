#pragma once
#include <concepts>

	//here is defined a policy of trasfer return value of producer 
	namespace policy
	{
		//return value doesn't anything
		struct producer_return_void {};

		//return value must match the first argument a passed function
		struct producer_return_any  {};
	}

	template<typename T>
	concept is_policy =
	(
		(std::is_same_v<policy::producer_return_any,std::remove_reference_t<T>>) 
		||
		(std::is_same_v<policy::producer_return_void,std::remove_reference_t<T>>)
	);
