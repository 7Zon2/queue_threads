#pragma once
#include <iostream>
#include <list>
#include <vector>
#include <thread>
#include "policy.hpp"

	using namespace std::literals;

	template<size_t Prod, size_t Cons>
	requires ((Prod > 0) && (Cons > 0))
	class worker;


		void bar(int counter)
		{
			std::list<int> ls;
			for(size_t i=0;i<1'000'0;i++)
		    ls.push_back(i);

		    std::cout<<"bar("<<counter<<")"<<std::endl;
		};


		void gadget(int counter)
		{
			std::list<int> ls;
			for(size_t i=0;i<1'000'0;i++)
		    ls.push_back(i);

		    std::cout<<"gadget("<<counter<<")"<<std::endl;
		};


		int bar1()
		{
			return 5;
		}

		void gadget1(int x)
		{
			std::cout<<"x="<<x<<std::endl;
		}

    class TESTS
    {

		std::jthread th1;
		std::jthread th2;

		private:

        template<typename W,typename T>
        void test1(W& workman,T &&it)
        {
			th1=std::jthread
			{
			   [&]()
			   {
		          int counter=0;

	  		      for(size_t i=0;i<1000;i++)
	  		      {
		 		     workman.manager_tasks_out
					 (policy::producer_return_void{},std::forward<T>(it),std::move(bar),counter++);
	  		      }
			   }
			};

			th2=std::jthread
			{
				[&]()
				{
					int counter=0;

					for(size_t i=0;i<1000;i++)
					{
						workman.manager_tasks_in(std::move(gadget),counter++);
					}
				}	
			};

        }


		template<typename W,typename T>
        void test2(W& workman,T &&it)
		{	
			for(size_t i=0;i<1000;i++)
			{
				workman.manager_tasks_out(policy::producer_return_any{},std::forward<T>(it),std::move(gadget1),5);		
			}
		}


		public:
		
		template<size_t Prod,size_t Cons>
		void start_tests()
		{	
			worker<Prod,Cons> workman;
			auto& in=workman.get_producer();

			test1(workman,in);
			workman.parallel_work();
			
			while(workman.clear_all()==false){}

			auto& in2=workman.manager_tasks_in(std::move(bar1));	
			test2(workman,in2);
			workman.work();
		}
    };


	auto foo=[]()
	{
		throw std::runtime_error{"EXCEPTION!"};
	};

