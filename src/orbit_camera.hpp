#pragma once

#include <dr/app/app.hpp>
#include <dr/app/camera.hpp>

namespace dr
{

struct OrbitCamera
{
    struct
    {
        Camera current;
        Camera target;
    } transform;

    struct
    {
        f32 fov_y{deg_to_rad(60.0f)};
        f32 clip_near{0.01f};
        f32 clip_far{1000.0f};
    } frustum;

    struct
    {
        Orbit orbit{pi<f32> * 0.25f, pi<f32> * -0.25f};
        Zoom zoom{1.0f, 1.0f, 0.01, 1000.0};
        Pan pan{};
        f32 stiffness{7.5f};
    } controls;

    struct
    {
        Vec3<f32> position{};
        f32 radius{1.0f};
    } target;

    struct
    {
        Vec2<f32> last_touch_points[2];
        i8 last_num_touches;
        bool mouse_down[3];
    } input;

    OrbitCamera();

    void update();
    void frame_target();
    void handle_event(App::Event const& event);

    Mat4<f32> make_world_to_view() const;
    Mat4<f32> make_view_to_clip() const;
};

} // namespace dr