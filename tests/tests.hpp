#pragma once
#include <iostream>
#include <list>
#include <vector>
#include <thread>
	

	template<size_t,size_t>	
	class worker;


		void bar(int counter)
		{
			std::list<int> ls;
			for(size_t i=0;i<1'000'00;i++)
		    ls.push_back(i);

		    std::cout<<"bar("<<counter<<")"<<std::endl;
		};


		void gadget(int counter)
		{
			std::list<int> ls;
			for(size_t i=0;i<1'000'00;i++)
		    ls.push_back(i);

		    std::cout<<"gadget("<<counter<<")"<<std::endl;
		};



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
		 		     workman.manager_tasks_out(it,std::move(bar),counter++);
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


		

		public:
		
		template<size_t Prod,size_t Cons>
		void start_tests()
		{	
			worker<Prod,Cons> workman;
			auto& in=workman.get_producer();

			test1(workman,in);
			workman.parallel_work();
		}
    };


	auto foo=[]()
	{
		throw std::runtime_error{"EXCEPTION!"};
	};

