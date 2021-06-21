#pragma once

#include <vector>
#include <memory>

#include "api.h"

namespace core {
  struct dialog_executor;

  class engine : public messenger {
    public:
      static constexpr size_t dialog_count = 5;
      static constexpr size_t queue_size = 8;
      static constexpr size_t messages_count = 1000000000;
      static constexpr size_t threads_count = 0;

      void send(size_t dialog_id_to, session_event event) override final;
      void request( size_t dialog_id_to, 
                  session_event request,
                  session_event& response) override final;
 
      void run();
      void stop();

      void add(std::shared_ptr<dialog>);
    private:
      std::vector <std::shared_ptr<dialog_executor>> dialogs;
  };
}


