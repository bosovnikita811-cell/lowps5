#ifndef LAUNCHER_INCLUDE_CONTROLLER_NAVIGATION_H_
#define LAUNCHER_INCLUDE_CONTROLLER_NAVIGATION_H_

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTimer>

#include <SDL3/SDL.h>

class ControllerNavigation final : public QObject {
public:
    explicit ControllerNavigation(QObject* parent = nullptr);
    ~ControllerNavigation() override;

private:
    void Poll();
    void DiscoverGamepad();
    void SendTap(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void UpdateDirections(const QSet<int>& directions, qint64 now);

    QTimer m_timer;
    QElapsedTimer m_clock;
    SDL_Gamepad* m_gamepad = nullptr;
    SDL_JoystickID m_gamepad_id = 0;
    QHash<int, qint64> m_next_repeat;
    int m_scan_frames = 0;
    bool m_initialized = false;
    bool m_previous_south = false;
    bool m_previous_east = false;
    bool m_previous_start = false;
    bool m_previous_left_shoulder = false;
    bool m_previous_right_shoulder = false;
};

#endif // LAUNCHER_INCLUDE_CONTROLLER_NAVIGATION_H_
