#include "project.h"
#include "core.h"

int main() {

  core::engine engine;

  for(size_t i = 0; i < core::engine::dialog_count; ++i) {
    std::shared_ptr<core::dialog> pdlg(project::create_dialog(i, engine));
    engine.add(pdlg);
  }

  engine.run();

  for(size_t i = 0; i < core::engine::messages_count; ++i) {
    size_t dialog_index = i % core::engine::dialog_count;
    core::session_event event{ std::to_string(i) };
    engine.send(dialog_index, event);
  }

  engine.stop();

  return 0;
} 

