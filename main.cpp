//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <boost/fiber/all.hpp>

namespace core {

  struct session_event {
    std::string text;
  };

  struct dialog {
      virtual void handle (session_event ) = 0; 
  };
}

namespace project {

  struct dialog_impl : public core::dialog {
      size_t id_ = 0;

      dialog_impl(size_t i) : id_(i) {}

      void handle (core::session_event e ) {
	std::cout << std::this_thread::get_id() <<  " dialog " << id_ << " received " << e.text << std::endl;
      }
    };

  core::dialog * create_dialog(size_t i) {
    return new dialog_impl(i);
  };
}

namespace core {

  struct dialog_executor {
    std::shared_ptr<dialog> dialog_;
    boost::fibers::buffered_channel< session_event > channel_;
    session_event current_;
    boost::fibers::fiber fiber_;

    dialog_executor( dialog *d, size_t queue_size)
      : dialog_(d),
        channel_ (queue_size),
        fiber_ ( [this]{ read_loop(); }) 
    {}

    ~dialog_executor() {
       fiber_.join();
    }

    void read_loop() {
      while ( boost::fibers::channel_op_status::success == channel_.pop(current_) ) {
	dialog_->handle(current_);
      }
    }
  };

}

static bool finished = false;
static std::mutex mtx{};
static boost::fibers::condition_variable_any cond{};

void thread(/* boost::fibers::detail::thread_barrier * b*/ ) {
/*    std::ostringstream buffer;
    buffer << "thread started " << std::this_thread::get_id() << std::endl;
    std::cout << buffer.str() << std::flush;*/
    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >();
    std::unique_lock<std::mutex> lk( mtx);
    cond.wait( lk, []() { return finished;} );
}

int main() {
  try {
    const size_t dialog_count = 1000;
    const size_t queue_size = 2;
    const size_t messages_count = 100000;

    boost::fibers::use_scheduling_algorithm< boost::fibers::algo::shared_work >(); 
//    boost::fibers::detail::thread_barrier b( 4);
    std::thread threads[] = {
        std::thread( thread ),
        std::thread( thread),
        std::thread( thread)
    };
    std::vector <std::shared_ptr<core::dialog_executor>> dialogs;

    for(size_t i = 0; i < dialog_count; ++i) {
	std::shared_ptr<core::dialog_executor> d(new core::dialog_executor {
	   project::create_dialog(i),
	   queue_size
	});
	dialogs.push_back(d);
        d->fiber_.detach();
    }

    for(size_t i = 0; i < messages_count; ++i) {
      size_t dialog_index = i % dialog_count;
      core::session_event event{ std::to_string(i) };
      dialogs[dialog_index]->channel_.push(event);
    }
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
    std::unique_lock<std::mutex> lk( mtx);
    finished = true;

    for ( std::thread & t : threads) { /*< wait for threads to terminate >*/
        t.join();
    }
  }
  catch(std::exception& e) {
    std::cerr << "caught: " << e.what() << std::endl;
  }
  
  return 0;
} 

