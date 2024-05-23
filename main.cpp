#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include <condition_variable>
#include <utility>
#include <vector>
#include <new>
#include <queue>
#include <atomic>
#include <list>
#include <my_libs/exception_handler/exception_handler.hpp>
#include <my_libs/mv_func/mv_func.hpp>


	using namespace std::literals;



	template<size_t Prod, size_t Cons>
	class worker final
	{
		private:
			
			//alias for function-wrapper
			using work_foo=move_func<void(std::atomic<bool>&)>;

			struct In;

			//pair of threads and atomic flags to identity execution
			using atomic_pair=std::pair<std::jthread,std::atomic<bool>>;						 


			std::vector <atomic_pair> pjvec_;//vector of producers
			std::vector <atomic_pair> cjvec_;//vector of consumers

			//stores tasks that will be produced for consumers. List due to invalidation iterators is not needed
			std::list<In> list_prod;
			std::atomic<int> prod_counter = 0;
 

			//here will be stored the caught exceptions up to the specified limit
			exception_handler<10> ex_handler_;

			std::mutex mt_;
			std::mutex mt2_;

		private:

			struct In final
			{
				private:

					friend worker;

					//stores consumers functions
					using work_queue=std::queue<work_foo>;

					
					//Function-producer after execution of which the consumers start working
					work_foo Prod_foo;			

					//being executed future of producer
					std::shared_future<void> prod_future;

					//stores tasks  that will be consumed after any task was produced by any thread
					work_queue cons_out;

					//checks if the Prod_foo has been called
					std::atomic<bool> callable = 0;

					//counter of pushed and popped tasks 
					std::atomic<int> counter = 0;
			
				public:

					In(work_foo&& foo,std::shared_future<void>&& future) noexcept 
						: Prod_foo(std::move(foo)),prod_future(std::move(future)){}


					In(In&& rhs) noexcept
					: Prod_foo(std::move(rhs.Prod_foo)),prod_future(std::move(rhs.prod_future)),
					  cons_out(std::move(cons_out))
					  {
						counter.store(rhs.counter.load(std::memory_order_acquire),std::memory_order_relaxed);
						callable.store(rhs.callable.load(std::memory_order_acq_rel),std::memory_order_relaxed);
					  }

				public:

					/// @brief checks whether all of tasks for current producer have been executed 
					/// @return return true if tasks have been executed
					[[nodiscard]]
					bool is_finish() noexcept
					{
					   return (!counter.load(std::memory_order_relaxed) 
					   && callable.load(std::memory_order_relaxed));
					}

				private:


					void emplace(work_foo&& foo)
					{
						cons_out.emplace(std::move(foo));
						++counter;
					}


					[[nodiscard]]
					auto& front()
					{
					 	cons_out.front();
					}


					void pop()
					{
						cons_out.pop();
					}


					[[nodiscard]]
					size_t size() const noexcept
					{
						return cons_out.size();
					}


					[[nodiscard]]
					bool empty() const noexcept
					{
						return cons_out.empty();
					}
			};

		private:

			int fulfill_prod()
			{
				int executed=0;

				std::unique_lock<std::mutex> lk(mt2_,std::defer_lock);
				lk.lock();
					auto b=list_prod.begin();
					auto e=list_prod.end();
				lk.unlock();

				for(;b!=e;)
				{
					//if producer hasn't been called yet we do it
					//otherwise we check it on having any consumer work
					if(b->callable==false)
					{
						for(size_t i=0;i<pjvec_.size();i++)
						{
							//if the current thread in the vector is executing a producer task we pass it by
							if(pjvec_[i].second) continue;
						
							lk.lock();
								pjvec_[i].first=std::jthread{std::move(b->Prod_foo),std::ref(pjvec_[i].second)};
								b->callable=true;
								++b;
							lk.unlock();
							break;							
						}
					}
					else{
						++executed;
					}
					lk.lock();
						++b;
					lk.unlock();
				}
				return executed;
			}

			void fulfill_cons()
			{
			
				std::unique_lock<std::mutex> lk(mt2_,std::defer_lock);
				lk.lock();
					auto b=list_prod.begin();
					auto e=list_prod.end();
				lk.unlock();

				for(;b!=e;)
				{
					for (size_t j=0;j<cjvec_.size();j++)
					{              
						if(cjvec_[j].second) continue;
						{
							{
								std::lock_guard<std::mutex> lock(mt_);

								if(b->cons_out.empty()) break;

								cjvec_[j].second = true;
								
								cjvec_[j].first  = std::jthread{std::move(b->cons_out.front()),std::ref(cjvec_[j].second)};
								b->cons_out.pop();
							}
						}
					}

					lk.lock();
						++b;
					lk.unlock();		
				}
			}


			bool all_done()
			{
				for(auto &&i:list_prod)
				{
					if(i.is_finish()==false)
						return false;
				}
				return true;
			}

		public:


			worker():pjvec_(Prod),cjvec_(Cons){}


            /// @brief main function for producers
            /// @param f Capture any callable functions for sequential execution 
            /// @param ...arguments for function
            /// @return proxy class for checking executions status
            template<typename F,typename...Args>
            requires std::is_invocable_v<F,Args...> 
			[[nodiscard]]
            auto& manager_tasks_in(F&&f,Args&&...args)
            {

				using ret=std::invoke_result_t<F,Args...>;

				std::packaged_task<ret(Args...)> mv_{std::move(f)};   

				std::future<void> fut_=mv_.get_future();


					work_foo task{
					[mv=std::move(mv_),tup=std::make_tuple(std::forward<Args>(args)...)]
					(std::atomic<bool>& at) mutable
					{
						at=true;

						std::apply
						(
							[fun=std::move(mv)]<typename...Types>(Types&&...values)  mutable
							{
								fun(std::forward<Types>(values)...);
							}							
						,tup);

						at=false;
					}};


					std::lock_guard<std::mutex> lk(mt2_);
					list_prod.emplace_back(std::move(task),std::move(fut_));
					++prod_counter;

					return list_prod.back();
            }


			/// @brief //function for consumers that will be executed after their specified binding producer
			/// @param in binder-proxy class given by function-producer
			/// @param f callable function-consumer
			/// @param ...Arguments for its
			template<typename F,typename...Args>
			requires std::is_invocable_v<F,Args...>
			void manager_tasks_out(In& in, F&& f,Args&&...args)
			{
				using ret=std::invoke_result_t<F,Args...>;

				std::packaged_task<ret(Args...)> mv_{std::move(f)};   
				
		

					//saves the arrived consumer for the last producer, otherwise if the size of consumer-functions
					// is larger than size of producer-functions then every functions will be appended as the one that will be executed after it,
					// that is if there is 1 producer and 5 consumers, they will be executed concurrently ( depending on threads) 
					std::shared_future<void> foo=in.prod_future;
														
				
					work_foo task
					{[mv=std::move(mv_),tup=std::make_tuple(std::forward<Args>(args)...),
					 fut=std::move(foo),&handler=ex_handler_,&success=in.counter]
					(std::atomic<bool>& thread_execution) mutable
					{
						thread_execution=true;
						
						try
						{
							fut.get();
						}	
						catch(const std::exception& ex)
						{
							thread_execution=false;
							handler.push_ex(std::current_exception());
							--success; 
							return;
						}

						std::apply
						(
							[fun=std::move(mv)]<typename...Types>(Types&&...values) mutable
							{
								fun(std::forward<Types>(values)...);
							}
						,tup);

						thread_execution=false;
						--success;
					}};
					
				std::lock_guard<std::mutex> lock(mt_);		
				in.emplace(std::move(task));
			}	


			void work()
			{
				std::jthread worker1
                {
                    [this]
                    {
                        for(;;)
                        {
                            fulfill_prod();
							if(all_done())
								return;
                        }
                    }
                };

				std::jthread worker2
				{
					[this]
					{
						for(;;)
						{
							fulfill_cons();
							if(all_done())
								return;
						}
					}
				};
			}
	};


	auto pushes_and_print=[]<typename T>(T ls) 
	{
		std::cout<<"push:"<<typeid(ls).name()<<std::endl;
		for(size_t i=0;i<50000;i++)
		{
			ls.push_back(i/2);
		}
	};


	auto foo=[]()
	{
		throw std::runtime_error{"EXCEPTION!"};
	};


	auto bar=[]()
	{
		std::list<int> ls;
		for(size_t i=0;i<1'000'00;i++)
			ls.push_back(i);

		std::cout<<"bar"<<std::endl;
	};

	auto gadget()
	{
		std::list<int> ls;
		for(size_t i=0;i<1'000'00;i++)
			ls.push_back(i);

		std::cout<<"gadget"<<std::endl;
	}



void test0()
{
	for(size_t i=0;i<5;i++)
	{
		bar();
	}
}

template<typename W,typename T>
void test1(W& workman,T &&it)
{
	for(size_t i=0;i<100;i++)
	{
		workman.manager_tasks_out(it,std::move(bar));
	}
}

template<typename W>
void test2(W& workman)
{
	for(size_t i=0;i<100;i++)
	{
		workman.manager_tasks_in(std::move(gadget));
	}
}

void test3()
{
	std::cout<<"test3"<<std::endl;
}


int main()
{
	worker<2, 2> workman;

	auto& it1=workman.manager_tasks_in(bar);

	std::jthread th{test1<decltype(workman),decltype(it1)>,std::ref(workman),std::ref(it1)};

	std::jthread th2{test2<decltype(workman)>,std::ref(workman)};

	//workman.manager_tasks_out(it1,test3);

 	workman.work();


	return 0;
}


