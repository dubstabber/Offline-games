#include "core/App.hpp"
#include "scenes/MenuScene.hpp"

#include <memory>

int main() {
    og::App app;
    if (!app.init()) {
        return 1;
    }
    app.scenes().push(std::make_unique<og::MenuScene>(app.scenes()));
    app.scenes().applyPending();
    app.run();
    return 0;
}
