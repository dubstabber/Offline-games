#include "core/App.hpp"

#include "core/Input.hpp"
#include "core/Layout.hpp"
#include "core/Settings.hpp"
#include "core/Theme.hpp"

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

bool App::dispatchPointer(const SDL_Event& converted) {
    Scene* scene = scenes_.current();
    if (scene == nullptr) {
        return false;
    }
    // SDL cross-generates events between touch and mouse: a finger touch also
    // arrives as a synthetic mouse event (which == SDL_TOUCH_MOUSEID), and a mouse
    // click can arrive as a synthetic finger event (touchID == SDL_MOUSE_TOUCHID).
    // Dropping the synthetic copies leaves one pointer stream per physical input —
    // otherwise a single tap fires Down twice, e.g. popping two Tap Match tiles.
    PointerEvent pointer;
    switch (converted.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (converted.button.button != SDL_BUTTON_LEFT ||
            converted.button.which == SDL_TOUCH_MOUSEID) {
            return false;
        }
        pointer.phase = converted.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? PointerEvent::Phase::Down
                                                                      : PointerEvent::Phase::Up;
        pointer.x = converted.button.x;
        pointer.y = converted.button.y;
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP:
        if (converted.tfinger.touchID == SDL_MOUSE_TOUCHID) {
            return false;
        }
        pointer.phase = converted.type == SDL_EVENT_FINGER_DOWN ? PointerEvent::Phase::Down
                                                                : PointerEvent::Phase::Up;
        pointer.x = converted.tfinger.x;
        pointer.y = converted.tfinger.y;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        if (converted.motion.which == SDL_TOUCH_MOUSEID) {
            return false;
        }
        pointer.phase = PointerEvent::Phase::Move;
        pointer.x = converted.motion.x;
        pointer.y = converted.motion.y;
        break;
    case SDL_EVENT_FINGER_MOTION:
        if (converted.tfinger.touchID == SDL_MOUSE_TOUCHID) {
            return false;
        }
        pointer.phase = PointerEvent::Phase::Move;
        pointer.x = converted.tfinger.x;
        pointer.y = converted.tfinger.y;
        break;
    default:
        return false;
    }
    scene->handleInput(pointer);
    return true;
}

bool App::processEvents() {
    bool dispatched = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            running_ = false;
            continue;
        case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_SHOWN:
            // The compositor may have discarded the window contents; redraw once.
            needsRedraw_ = true;
            continue;
        default:
            break;
        }
        // Map window/normalized coordinates into the logical canvas space so
        // scenes work purely in logical pixels.
        SDL_ConvertEventToRenderCoordinates(renderer_.get(), &event);
        if (dispatchPointer(event)) {
            dispatched = true;
        }
    }
    return dispatched;
}

void App::run() {
    if (canvas_ == nullptr) {
        SDL_Log("App::run called before successful init().");
        return;
    }
    running_ = true;
    Uint64 previousTicks = SDL_GetTicks();
    const Uint64 perfFreq = SDL_GetPerformanceFrequency();

    while (running_ && !scenes_.empty()) {
        const Uint64 frameStart = SDL_GetPerformanceCounter();
        const Uint64 nowTicks = SDL_GetTicks();
        const float dtSeconds = static_cast<float>(nowTicks - previousTicks) / 1000.0F;
        previousTicks = nowTicks;

        const bool inputThisFrame = processEvents();

        if (Scene* scene = scenes_.current()) {
            const bool wasAnimating = scene->isAnimating();
            scene->update(dtSeconds);
            // Conservative idle: a static scene (a menu/settings screen at rest)
            // with no input and nothing forcing a redraw skips the draw and
            // present, so it stops driving the PinePhone's GPU when nothing
            // changed. wasAnimating covers the frame a fling settles to a stop.
            if (needsRedraw_ || inputThisFrame || wasAnimating || scene->isAnimating()) {
                canvas_->clear(theme().appBg);
                scene->render(*canvas_);
                SDL_RenderPresent(renderer_.get());
                needsRedraw_ = false;
            }
        }

        if (scenes_.applyPending()) {
            needsRedraw_ = true; // a push/pop/replace revealed a (possibly static) scene
        }

        // Frame cap driven by the Maximum FPS setting (always a valid stop, so
        // never zero). Sleep only the time left in this frame's budget, measured
        // against a high-resolution counter so a slow frame doesn't over-sleep.
        const Uint64 frameTicks = perfFreq / static_cast<Uint64>(settings().maxFps);
        const Uint64 frameEnd = frameStart + frameTicks;
        const Uint64 afterFrame = SDL_GetPerformanceCounter();
        if (afterFrame < frameEnd) {
            const auto ms = static_cast<Uint32>(((frameEnd - afterFrame) * 1000) / perfFreq);
            if (ms > 0) {
                SDL_Delay(ms);
            }
        }
    }
}

} // namespace og
