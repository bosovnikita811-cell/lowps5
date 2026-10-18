#include "controllerNavigation.h"

#include <QApplication>
#include <QKeyEvent>
#include <QSet>
#include <QWidget>

namespace {
constexpr int GAMEPAD_SCAN_INTERVAL_FRAMES = 15;
constexpr int REPEAT_DELAY_MS = 320;
constexpr int REPEAT_INTERVAL_MS = 95;
constexpr Sint16 STICK_DEADZONE = 16000;
}

ControllerNavigation::ControllerNavigation(QObject* parent) : QObject(parent) {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
        // Gamepad navigation is optional; keyboard and mouse input remain available.
        return;
    }

    m_initialized = true;
    m_clock.start();
    DiscoverGamepad();

    m_timer.setInterval(16);
    QObject::connect(&m_timer, &QTimer::timeout, this, [this] { Poll(); });
    m_timer.start();
}

ControllerNavigation::~ControllerNavigation() {
    m_timer.stop();
    if (m_gamepad != nullptr) {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
    if (m_initialized) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
}

void ControllerNavigation::DiscoverGamepad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    SDL_JoystickID candidate = (ids != nullptr && count > 0) ? ids[0] : 0;
    if (ids != nullptr) {
        SDL_free(ids);
    }

    if (candidate == m_gamepad_id && m_gamepad != nullptr &&
        SDL_GamepadConnected(m_gamepad)) {
        return;
    }

    if (m_gamepad != nullptr) {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
    m_gamepad_id = candidate;

    if (candidate != 0) {
        m_gamepad = SDL_OpenGamepad(candidate);
        if (m_gamepad == nullptr) {
            m_gamepad_id = 0;
        }
    }

    m_previous_south = false;
    m_previous_east = false;
    m_previous_start = false;
    m_previous_left_shoulder = false;
    m_previous_right_shoulder = false;
    m_next_repeat.clear();
}

void ControllerNavigation::SendTap(int key, Qt::KeyboardModifiers modifiers) {
    QWidget* target = QApplication::activeModalWidget();
    if (target == nullptr) {
        target = QApplication::focusWidget();
    }
    if (target == nullptr) {
        target = QApplication::activeWindow();
    }
    if (target == nullptr || !target->isEnabled()) {
        return;
    }

    QKeyEvent press(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers);
    QApplication::sendEvent(target, &release);
}

void ControllerNavigation::UpdateDirections(const QSet<int>& directions, qint64 now) {
    for (int key : directions) {
        auto it = m_next_repeat.find(key);
        if (it == m_next_repeat.end()) {
            SendTap(key);
            m_next_repeat.insert(key, now + REPEAT_DELAY_MS);
        } else if (now >= it.value()) {
            SendTap(key);
            it.value() = now + REPEAT_INTERVAL_MS;
        }
    }

    for (auto it = m_next_repeat.begin(); it != m_next_repeat.end();) {
        if (!directions.contains(it.key())) {
            it = m_next_repeat.erase(it);
        } else {
            ++it;
        }
    }
}

void ControllerNavigation::Poll() {
    SDL_PumpEvents();
    if (++m_scan_frames >= GAMEPAD_SCAN_INTERVAL_FRAMES ||
        (m_gamepad != nullptr && !SDL_GamepadConnected(m_gamepad))) {
        m_scan_frames = 0;
        DiscoverGamepad();
    }
    if (m_gamepad == nullptr) {
        return;
    }

    QSet<int> directions;
    if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)) {
        directions.insert(Qt::Key_Up);
    }
    if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
        directions.insert(Qt::Key_Down);
    }
    if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) {
        directions.insert(Qt::Key_Left);
    }
    if (SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) {
        directions.insert(Qt::Key_Right);
    }

    const Sint16 axis_y = SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_LEFTY);
    const Sint16 axis_x = SDL_GetGamepadAxis(m_gamepad, SDL_GAMEPAD_AXIS_LEFTX);
    if (axis_y < -STICK_DEADZONE) directions.insert(Qt::Key_Up);
    if (axis_y > STICK_DEADZONE) directions.insert(Qt::Key_Down);
    if (axis_x < -STICK_DEADZONE) directions.insert(Qt::Key_Left);
    if (axis_x > STICK_DEADZONE) directions.insert(Qt::Key_Right);
    UpdateDirections(directions, m_clock.elapsed());

    const bool south = SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    const bool east = SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_EAST);
    const bool start = SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_START);
    const bool left_shoulder =
        SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    const bool right_shoulder =
        SDL_GetGamepadButton(m_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);

    if (south && !m_previous_south) SendTap(Qt::Key_Return);
    if (east && !m_previous_east) SendTap(Qt::Key_Escape);
    if (start && !m_previous_start) SendTap(Qt::Key_Return);
    if (left_shoulder && !m_previous_left_shoulder) {
        SendTap(Qt::Key_Tab, Qt::ShiftModifier);
    }
    if (right_shoulder && !m_previous_right_shoulder) SendTap(Qt::Key_Tab);

    m_previous_south = south;
    m_previous_east = east;
    m_previous_start = start;
    m_previous_left_shoulder = left_shoulder;
    m_previous_right_shoulder = right_shoulder;
}
