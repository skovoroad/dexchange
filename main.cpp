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

    bool need_response = false;
    size_t sender_id = 0;
  };

  struct messenger {
    virtual void send(size_t dialog_id_to, session_event event)  = 0;
    virtual void request(size_t dialog_id_from, size_t dialog_id_to, session_event event)  = 0;
  };

  struct dialog {
      virtual void handle (session_event ) = 0;
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
      std::cout << std::this_thread::get_id() <<  " dialog " << id_ << " received " << e.text << std::endl;

      if(expected_response_from_) {
        if(e.sender_id != expected_response_from_ ) {
          std::cout << "============ DISORDER " << id_ << " ============  expected response from "
            << expected_response_from_
            << " received message from "
            << e.sender_id << " \"" << e.text << "\""
            << std::endl;
          }
          else { 
            expected_response_from_ = 0; 
            std::cout << "============ ORDER " << id_ << " ============  expected response from "
              << expected_response_from_
              << " received message from "
              << e.sender_id << " \"" << e.text << "\""
              << std::endl;
          }
      }

      if(e.need_response) 
        send_response(e);
      if(!(++received_ % 10))
        send_request();
    }

    void send_response(core::session_event& e) {
      std::stringstream ostr;
      ostr << "response to \"" << e.text << "\" from " << id_ << " to " << e.sender_id;
      std::cout << ostr.str() << std::endl;
      messenger_.send(e.sender_id, core::session_event{ ostr.str(), false, id_} ); 
    }

    void send_request() {
      std::stringstream ostr;
      size_t receiver =  id_ / 2;
      if(receiver == id_)
        return; // just for simplicity
      ostr << "send request from " << id_ << " to " << receiver;
      std::cout << ostr.str() << std::endl;
      expected_response_from_ = receiver;
      messenger_.send(receiver, core::session_event{ ostr.str(), true, id_} ); 
    }

    size_t id() { return id_; }
  };

  core::dialog * create_dialog(core::messenger &m, size_t i) {
    return new dialog_impl(m, i);
  };
}

namespace core {

  struct dialog_executor {
    std::shared_ptr<dialog> dialog_;
    boost::fibers::buffered_channel< session_event > channel_;
    session_event current_;
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
        dialog_->handle(current_);
      }
    }

    void send(session_event event) {
      channel_.try_push(event);
    }

    void request(size_t dialog_id_from, session_event event) {
      channel_.try_push(event);
    }
  };

  struct fiber_threads {
     bool finished = false;
     std::mutex mtx{};
     boost::fibers::condition_variable_any cond{};

     std::vector<std::thread> threads_;

     void run(size_t threads_count) {
       boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
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

  struct engine : public messenger {
    const size_t dialog_count = 1000;
    const size_t queue_size = 2;
    const size_t messages_count = 1000000000;
    const size_t threads_count = 0;
    fiber_threads ft;
    std::vector <std::shared_ptr<core::dialog_executor>> dialogs;
 
    void send(size_t dialog_id_to, session_event event) {
      dialogs[dialog_id_to]->send(event);
    }

    void request(size_t dialog_id_from, size_t dialog_id_to, session_event event) {
      dialogs[dialog_id_to]->request(dialog_id_from, event);
    }
 
    void run() {
      try {
        if(threads_count)
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
          dialogs[dialog_index]->channel_.push(event);
        }

        for(size_t i = 0; i < dialog_count; ++i) {
          dialogs[i]->stop_ = true;
        }
        if(threads_count)
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

