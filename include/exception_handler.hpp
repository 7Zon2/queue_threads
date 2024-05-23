#pragma once
#include <concepts>
#include <type_traits>
#include <array>

    template<typename T>
    concept is_exception=std::is_base_of_v<std::exception,T>;
    	
    template<int sz>
    requires (sz>0)
    class exception_handler
    {

      std::array<std::exception_ptr,sz> excarr_;
      int last_id=0;
      std::mutex mt_;

     private:

            template<is_exception T>
            [[nodiscard]] 
            auto find_ex() 
            {
                std::lock_guard<std::mutex> lock(mt_);
                for(int i=0;i<last_id;i++)
                {
                    try 
                    {
                        std::rethrow_exception(excarr_[i]);
                    }
                    catch(T& tx)
                    {
                        return std::make_pair(excarr_[i],i);
                    }
                    catch(...)
                    {
                        continue;
                    }
                }

                return std::make_pair(std::exception_ptr{},-1);
            }

     public:


            void push_ex(const std::exception_ptr& ptr) 
            {   
                std::lock_guard<std::mutex> lock(mt_);
                //if the limit established exceptions have been exceeded then it is assumed that
                //the executable program should be emergency terminated
                if(sz<=last_id) std::terminate();
                excarr_[last_id++]=std::move(ptr);
            }


            [[nodiscard]]
            constexpr 
            size_t size() noexcept
            {
                return sz;
            }         


            void erase_ex(const size_t id)
            {
                std::lock_guard<std::mutex> lock(mt_);
                if(id==last_id-1)
                {
                    excarr_[--last_id]=nullptr;
                }
                else
                {
                    std::copy(excarr_.begin()+id+1,excarr_.begin()+last_id,
                              excarr_.begin()+id);
                    excarr_[--last_id]=nullptr;
                }          
            }            


            [[nodiscard]]
            std::exception_ptr operator[](const size_t id)
            {
                return excarr_[id];
            }


            /* returns either a nullptr if the exception has not been found or an exception of the given type. 
            Erases this exception into the array */
            template<is_exception T>
            [[nodiscard]] 
            std::exception_ptr find_and_get() 
            {
                std::lock_guard<std::mutex> lock(mt_);

                auto pair=find_ex<T>();
                if(pair.first!=nullptr)
                {
                    excarr_[pair.second]=nullptr;
                    std::copy(excarr_.begin()+pair.second+1,excarr_.begin()+last_id,
                              excarr_.begin()+pair.second);
                    excarr_[--last_id]=nullptr;
                    return pair.first;  
                }
                return nullptr;
            }


            /* rethrows the exception of the given type if it has been found 
            and also erases this exception into the array*/
            template<is_exception T>
            void find_and_rethrow() 
            {
                std::lock_guard<std::mutex> lock(mt_);
                auto pair=find_ex<T>();
                if(pair.first)
                {
                    excarr_[pair.second]=nullptr;
                    std::copy(excarr_.begin()+pair.second+1,excarr_.begin()+last_id,
                              excarr_.begin()+pair.second);
                    excarr_[--last_id]=nullptr;
                   std::rethrow_exception(pair.first);
                }          
            }


            //if array of exceptions isn't empty, then the last exception is rethrown and deleted
            //otherwise  "false" is returned
            [[nodiscard]]
            bool pop() 
            {
                std::lock_guard<std::mutex> lock(mt_);
                if(last_id<0)
                    return false;

                auto ex=excarr_[--last_id];
                excarr_[last_id]=nullptr;
                std::rethrow_exception(ex);
            }

    };



