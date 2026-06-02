#include "core/SceneManager.hpp"

namespace og {

void SceneManager::push(std::unique_ptr<Scene> scene) {
    pendingScene_ = std::move(scene);
    pending_ = Action::Push;
}

void SceneManager::pop() {
    pending_ = Action::Pop;
}

void SceneManager::replace(std::unique_ptr<Scene> scene) {
    pendingScene_ = std::move(scene);
    pending_ = Action::Replace;
}

Scene* SceneManager::current() const {
    return scenes_.empty() ? nullptr : scenes_.back().get();
}

void SceneManager::applyPending() {
    switch (pending_) {
    case Action::Push:
        scenes_.push_back(std::move(pendingScene_));
        break;
    case Action::Pop:
        if (!scenes_.empty()) {
            scenes_.pop_back();
        }
        break;
    case Action::Replace:
        if (!scenes_.empty()) {
            scenes_.pop_back();
        }
        scenes_.push_back(std::move(pendingScene_));
        break;
    case Action::None:
        break;
    }
    pending_ = Action::None;
    pendingScene_.reset();
}

} // namespace og
