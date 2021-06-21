#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <sstream>

#include <boost/fiber/all.hpp>

namespace core {

  struct session_event {
    std::string text;

//    bool need_response = false;
    size_t sender_id = 0;
  };

  struct messenger {
    virtual void send(size_t dialog_id_to, session_event event)  = 0;
    virtual void request( size_t dialog_id_to, 
                          session_event request,
                          session_event& response
                        )  = 0;
  };

  struct dialog {
      virtual void handle (session_event ) = 0;
      virtual void response (session_event, session_event& ) = 0;
      virtual size_t id()  = 0;
  };
}

namespace project {

  struct dialog_impl : public core::dialog {
    core::messenger & messenger_;
    size_t id_ = 0;
    size_t received_ = 0;
    size_t expected_response_from_ = 0;

    dialog_impl(core::messenger& m, size_t i) : messenger_(m), id_(i) {}

    void handle (core::session_event e ) {
      std::cout << std::this_thread::get_id() <<  " dialog_impl: dialog " << id_ << " received " << e.text << std::endl;
 
      if(!(++received_ % 10)) {
        std::stringstream ostr;
        size_t receiver =  id_ / 2;
        if(receiver == id_)
          return; // just for simplicity
        ostr << " send request from " << id_ << " to " << receiver;
        std::cout << ostr.str() << std::endl;

        core::session_event resp;
        messenger_.request(receiver, core::session_event{ ostr.str(), id_}, resp ); 

        std::cout << std::this_thread::get_id() << " dialog_impl: received response from " << e.sender_id 
              << " to " << id_
              << " \"" << e.text << "\""
              << std::endl;
 
      }
    }

    void response (core::session_event e, core::session_event& r) {
      std::stringstream ostr;
      ostr << "response to \"" << e.text << "\" from " << id_ << " to " << e.sender_id;
      std::cout << std::this_thread::get_id() << " dialog_impl: response: " << ostr.str() << std::endl;

      r = core::session_event{ ostr.str(), id_} ; 
    }
    size_t id() { return id_; }
  };

  core::dialog * create_dialog(core::messenger &m, size_t i) {
    return new dialog_impl(m, i);
  };
}

namespace core {
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
 
    dialog_executor( dialog *d, size_t queue_size)
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

 struct engine : public messenger {
    const size_t dialog_count = 5;
    const size_t queue_size = 8;
    const size_t messages_count = 1000000000;
    const size_t threads_count = 0;
    fiber_threads ft;
    std::vector <std::shared_ptr<core::dialog_executor>> dialogs;
 
    void send(size_t dialog_id_to, session_event event) {
      dialogs[dialog_id_to]->send(event);
    }

/*    void request(size_t dialog_id_from, size_t dialog_id_to, session_event event) {
      dialogs[dialog_id_to]->request(dialog_id_from, event);
    }*/
    void request( size_t dialog_id_to, 
                  session_event request,
                  session_event& response) 
    {
      dialogs[dialog_id_to]->request(request, response); 
    }
 
    void run() {
      try {
        ft.run(threads_count);

        for(size_t i = 0; i < dialog_count; ++i) {
          std::shared_ptr<core::dialog_executor> d(new core::dialog_executor {
             project::create_dialog(*this, i),
             queue_size
          });
          dialogs.push_back(d);
        }

        for(size_t i = 0; i < messages_count; ++i) {
          size_t dialog_index = i % dialog_count;
          core::session_event event{ std::to_string(i) };
          dialogs[dialog_index]->send(event);
        }

        for(size_t i = 0; i < dialog_count; ++i) {
          dialogs[i]->stop_ = true;
        }
        ft.stop();
      }
      catch(std::exception& e) {
        std::cerr << "caught: " << e.what() << std::endl;
      }
    }
  };
}

int main() {
  core::engine engine;
  engine.run(); 
  return 0;
} 

