#include <iostream>
#include <stdexcept>
#include <thread>
#include <sstream>

#include <boost/fiber/all.hpp>

#include "core.h"

namespace core 
{
  struct fiber_threads {
     bool finished = false;
     std::mutex mtx{};
     boost::fibers::condition_variable_any cond{};

     std::vector<std::thread> threads_;

     void run(size_t threads_count) {
       boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
       if(!threads_count)
         return;
       while(threads_count --)
         threads_.emplace_back (
           [&]() {
              boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
              std::unique_lock<std::mutex> lk( mtx);
              cond.wait( lk, [&]() { return finished;} );
           }
         );
     } 

     void stop() {
       {
         std::unique_lock<std::mutex> lk( mtx);
         finished = true;
         cond.notify_all();
       }

       for ( std::thread & t : threads_) { 
         t.join();
       }
     } 
  };
 
  static fiber_threads ft;

  struct session_event_ctx {
    session_event_ctx(const session_event& e) : event(e) {} 

    session_event event;
    session_event * response = nullptr;

    boost::fibers::condition_variable_any cond{};
    std::mutex mtx{};
    bool responsed = false;
  };
 
  struct dialog_executor {
    std::shared_ptr<dialog> dialog_;
    boost::fibers::buffered_channel< std::shared_ptr<session_event_ctx> > channel_;
    std::shared_ptr<session_event_ctx> current_;
    boost::fibers::fiber fiber_;
    std::atomic<bool> stop_ = false;
 
    dialog_executor( std::shared_ptr<dialog> d, size_t queue_size)
      : dialog_(d),
        channel_ (queue_size),
        fiber_ ( [this]{ read_loop(); }) 
        {}

    ~dialog_executor() {
       if(fiber_.joinable())
         fiber_.join();
    }

    void read_loop() {
      using namespace std::chrono_literals;
      while ( boost::fibers::channel_op_status::success == channel_.pop_wait_for(current_, 1s) ) {
        if(stop_)
          break;
        if(current_->response) {
          std::cout << std::this_thread::get_id() << " dialog_executor: received request: " << current_->event.text << std::endl;
 
          dialog_->response(current_->event, *current_->response);
          {
            std::unique_lock<std::mutex> lk(current_->mtx);
            current_->responsed = true;
            std::cout << std::this_thread::get_id() << " dialog_executor: responded to request: " << current_->event.text << std::endl;
            current_->cond.notify_all();
         }
        }
        else {
//          std::cout << "dialog_executor: received message: " << current_->event->text << std::endl;
          dialog_->handle(current_->event);
        }
      }
    }

    void send(session_event event) {
      auto ctx = std::make_shared<session_event_ctx>( event );  
      channel_.push( ctx ); // TODO: what if not?
    }


    void request( session_event request,
                  session_event& response) 
    { // callee fiber
      auto ctx = std::make_shared<session_event_ctx>( request ); 
      ctx->response = &response;
      std::cout << std::this_thread::get_id() << " dialog_executor: sending request: " << ctx->event.text << std::endl;
 
      std::unique_lock<std::mutex> lk( ctx->mtx);
      channel_.push( ctx ); // TODO: what if not?
      std::cout << std::this_thread::get_id() << " dialog_executor: waiting response: " << ctx->event.text << std::endl;
      ctx->cond.wait( lk, [&]() { return ctx->responsed;} );
    }
  };

  void engine::add(std::shared_ptr<dialog> pdlg) {
    std::shared_ptr<core::dialog_executor> d(new core::dialog_executor {
      pdlg,
      queue_size
    });
    dialogs.push_back(d);
  }

  void engine::send(size_t dialog_id_to, session_event event) {
    dialogs[dialog_id_to]->send(event);
  }

  void engine::request( size_t dialog_id_to, 
                session_event request,
                session_event& response) 
  {
    dialogs[dialog_id_to]->request(request, response); 
  }

  void engine::run() {
    try {
      ft.run(threads_count);
    }
    catch(std::exception& e) {
      std::cerr << "caught: " << e.what() << std::endl;
    }
  }

  void engine::stop() {
    try {
      for(auto& d: dialogs) 
        d->stop_ = true;
      
      ft.stop();
    }
    catch(std::exception& e) {
      std::cerr << "caught: " << e.what() << std::endl;
    }
  }

} 
