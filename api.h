#pragma once

#include <cstdlib>
#include <string>

namespace core {

  struct session_event {
    std::string text;
    size_t sender_id = 0;
  };

  class messenger {
    public:
      virtual void send (
         size_t dialog_id_to, 
         session_event event
      )  = 0;

      virtual void request ( 
         size_t dialog_id_to, 
         session_event request,
         session_event& response
      )  = 0;
  };

  class dialog {
    public:
      virtual void handle (session_event ) = 0;
      virtual void response (
        session_event, 
        session_event&
      ) = 0;
  };
}


