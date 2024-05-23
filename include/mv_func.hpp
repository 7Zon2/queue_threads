#pragma once
#include <type_traits>
#include <concepts>
#include <algorithm>
#include <functional>


    template<typename Ret,typename...Types>
    class move_func;

    template<typename Ret,typename...Types>
    class move_func<Ret(Types...)>
    {

        struct storage
        {
            void *ptr_{};
            void (*deleter_)(const storage&){};
            size_t sz_=0;

        public:

            storage& operator=(const storage&)=delete;

            storage(const storage&)=delete;

            constexpr storage(storage&& rhs) noexcept
            {
                std::swap(ptr_,rhs.ptr_);   
                std::swap(deleter_,rhs.deleter_);
                std::swap(sz_,rhs.sz_);
            }


            constexpr storage& operator=(storage&& rhs) noexcept
            {
                if(this==&rhs) return *this;
                std::swap(ptr_,rhs.ptr_); 
                std::swap(deleter_,rhs.deleter_);
                std::swap(sz_,rhs.sz_);
                return *this;
            }
            

            constexpr storage() noexcept{}


            template<typename Obj>
            constexpr storage (Obj&& obj) :  
            ptr_( ::new std::decay_t<Obj>(std::move(obj))),
            sz_(sizeof(std::decay_t<Obj>)),deleter_(delete_obj<std::decay_t<Obj>>) 
            {}


        public:

            template<typename Obj>
            constexpr void push_to_storage(Obj&& obj)
            {

                using type=std::decay_t<Obj>;    
                deleter_=delete_obj<type>;

                if(sizeof(type)>sz_)
                {
                    void *p=::new type(std::move(obj));
                    deleter_(*this);
                    std::swap(ptr_,p);
                    ::new(ptr_) type{std::move(obj)};
                }
                else
                {
                    ::new(ptr_) type{std::move(obj)};
                }
                sz_=sizeof(type);     
            }


            template<typename Obj>
            constexpr static void delete_obj(const storage& str) noexcept
            {   
                if(str.ptr_==nullptr)
                    return;

                auto temp_ptr=static_cast<Obj*>(str.ptr_);
                delete temp_ptr; 
            }

            ~storage()
            {
                if(deleter_)
                deleter_(*this);
            }

        };//struct storage 


      

        template<typename P>
        constexpr static Ret storage_func(storage& stor,Types...args)
        {
            using ptr_type=std::add_pointer_t<std::decay_t<P>>;

            static_assert(std::is_pointer_v<ptr_type>);

            auto ptr=static_cast<ptr_type>(stor.ptr_);

            return std::invoke(*ptr,std::forward<Types>(args)...);
        }



    private:

        storage depository_{};

        Ret (*ptr_)(storage&,Types...){};

    public:

        constexpr move_func() noexcept{}
        
        template<typename F,typename...Args>
        requires (std::is_invocable_v<F,Args...>)
        constexpr move_func(F&& f,Args&&...args) : depository_(std::move(f)),
        ptr_(storage_func<F>)
        {}


        template<typename F>
        requires (std::is_invocable_v<F,Types...>)
        constexpr move_func(F&& f) : depository_(std::move(f)),
        ptr_(storage_func<F>)
        {}


        template<typename F,typename...Args>
        constexpr move_func(move_func<F(Args...)>&& rhs) noexcept :  
        depository_(std::move(rhs.depository_))
        {
            std::swap(ptr_,rhs.ptr_);
        }


    public:

        move_func(const move_func&)=delete;

        move_func&  operator=(const move_func&)=delete;

    
        constexpr move_func& operator=(move_func&& rhs) noexcept
        {
            std::swap(ptr_,rhs.ptr_);
            depository_=std::move(rhs.depository_);
            return *this;
        }


        template<typename F>
        requires (std::is_invocable_v<F,Types...>)
        constexpr move_func& operator=(F&& f) noexcept
        {
            depository_.push_to_storage(std::move(f));
            ptr_=&storage_func<F>;
            return *this;
        }



            template<typename...Args>
            requires requires(Args&&...args)
            {
                ptr_(depository_,std::forward<Args>(args)...);
            }
           constexpr Ret operator()(Args&&...args)
            {
                if(ptr_==nullptr) throw std::bad_function_call{};

                return ptr_(depository_,std::forward<Args>(args)...);
            }

            constexpr
            bool is_already_call() const noexcept
            {
                return !ptr_;
            }


            template<typename...Args>
            constexpr bool may_be_call(Args&&...args) //check if the function may be potentionally call 
            {
                if constexpr
                (
                    requires()
                    {
                         operator()(std::forward<Args>(args)...);
                    }
                )
                    return true;
                else
                   return false;
            }

    };


    template<typename F,typename...Args>
    requires std::is_invocable_v<F,Args...>
    move_func(F&& f,Args&&...args) -> move_func<std::invoke_result_t<F,Args...>(Args...)>;


