#include "core/App.hpp"

#include "core/Input.hpp"
#include "core/Layout.hpp"

namespace og {
namespace {

// Initial desktop window size: the logical canvas scaled down so it fits a
// monitor. On the phone the compositor makes it fullscreen anyway.
constexpr int kDevWindowWidth = layout::kWidth / 2;
constexpr int kDevWindowHeight = layout::kHeight / 2;

} // namespace

App::~App() {
    // Destroy SDL-owning members before tearing down the subsystems they used.
    canvas_.reset();
    fonts_.reset();
    renderer_.reset();
    window_.reset();
    if (ttfReady_) {
        TTF_Quit();
    }
    SDL_Quit();
}

bool App::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!TTF_Init()) {
        SDL_Log("TTF_Init failed: %s", SDL_GetError());
        return false;
    }
    ttfReady_ = true;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    if (!SDL_CreateWindowAndRenderer("Offline Games", kDevWindowWidth, kDevWindowHeight,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }
    window_.reset(window);
    renderer_.reset(renderer);

    // Fixed logical canvas, letterboxed onto whatever the real surface is.
    SDL_SetRenderLogicalPresentation(renderer_.get(), layout::kWidth, layout::kHeight,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);

    fonts_ = std::make_unique<FontManager>();
    canvas_ = std::make_unique<Canvas>(renderer_.get(), *fonts_);
    return true;
}

void App::dispatchPointer(const SDL_Event& converted) {
    Scene* scene = scenes_.current();
    if (scene == nullptr) {
        return;
    }
    PointerEvent pointer;
    switch (converted.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (converted.button.button != SDL_BUTTON_LEFT) {
            return;
        }
        pointer.phase = converted.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PointerEvent::Phase::Down
                                                                      : PointerEvent::Phase::Up;
        pointer.x = converted.button.x;
        pointer.y = converted.button.y;
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
        pointer.phase = converted.type == SDL_EVENT_FINGER_DOWN ? PointerEvent::Phase::Down
                                                                : PointerEvent::Phase::Up;
        pointer.x = converted.tfinger.x;
        pointer.y = converted.tfinger.y;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        pointer.phase = PointerEvent::Phase::Move;
        pointer.x = converted.motion.x;
        pointer.y = converted.motion.y;
        break;
    case SDL_EVENT_FINGER_MOTION:
        pointer.phase = PointerEvent::Phase::Move;
        pointer.x = converted.tfinger.x;
        pointer.y = converted.tfinger.y;
        break;
    default:
        return;
    }
    scene->handleInput(pointer);
}

void App::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running_ = false;
            continue;
        }
        // Map window/normalized coordinates into the logical canvas space so
        // scenes work purely in logical pixels.
        SDL_ConvertEventToRenderCoordinates(renderer_.get(), &event);
        dispatchPointer(event);
    }
}

void App::run() {
    if (canvas_ == nullptr) {
        SDL_Log("App::run called before successful init().");
        return;
    }
    running_ = true;
    Uint64 previousTicks = SDL_GetTicks();

    while (running_ && !scenes_.empty()) {
        const Uint64 nowTicks = SDL_GetTicks();
        const float dtSeconds = static_cast<float>(nowTicks - previousTicks) / 1000.0F;
        previousTicks = nowTicks;

        processEvents();

        if (Scene* scene = scenes_.current()) {
            scene->update(dtSeconds);
            canvas_->clear(colors::background);
            scene->render(*canvas_);
            SDL_RenderPresent(renderer_.get());
        }

        scenes_.applyPending();
        SDL_Delay(8); // cap to ~120 FPS; keeps phone CPU/battery use low
    }
}

} // namespace og
