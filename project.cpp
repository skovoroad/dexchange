#include <iostream>
#include <thread>
#include <sstream>

#include "project.h"


namespace project 
{

  class dialog_impl : public core::dialog {
      size_t id_ = 0;
    public:
      dialog_impl(size_t id, core::messenger& m);

      void handle(core::session_event) override final;
      void response(core::session_event e, core::session_event& r) override final;

    private:
      core::messenger & messenger_;
      size_t received_ = 0; // every 10 received message triggers request
  };


    dialog_impl::dialog_impl(size_t id, core::messenger& m) 
      : id_(id),
        messenger_(m) 
    {}

    void dialog_impl::handle (core::session_event e )
    {
      std::cout << std::this_thread::get_id() <<  " dialog_impl: dialog " << id_ << " received " << e.text << std::endl;
 
      if(!(++received_ % 10)) {
        std::stringstream ostr;
        size_t receiver =  id_ / 2;
        if(receiver == id_)
          return;
 
        ostr << " send request from " << id_ << " to " << receiver;
        std::cout << ostr.str() << std::endl;

        core::session_event resp;
        messenger_.request(receiver, core::session_event{ ostr.str(), id_}, resp ); 

        std::cout << std::this_thread::get_id() << " dialog_impl: received response from "
              << e.sender_id 
              << " to " << id_
              << " \"" << e.text << "\""
              << std::endl;
 
      }
    }

    void dialog_impl::response (core::session_event e, core::session_event& r) {
      std::stringstream ostr;
      ostr << "response to \"" << e.text << "\" from " << id_ << " to " << e.sender_id;
      std::cout << std::this_thread::get_id() << " dialog_impl: response: " << ostr.str() << std::endl;

      r = core::session_event{ ostr.str(), id_} ; 
    }
 
  core::dialog * create_dialog(size_t id, core::messenger &m) {
    return new dialog_impl(id, m);
  };

}
