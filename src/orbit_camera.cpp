#include "orbit_camera.hpp"

#include <dr/app/event_handlers.hpp>
#include <dr/app/gfx_utils.hpp>

namespace dr
{

OrbitCamera::OrbitCamera()
{
    controls.orbit.apply(transform.current);
    controls.zoom.apply(transform.current);
    controls.pan.apply(transform.current);
}

void OrbitCamera::update()
{
    // Apply controls to target camera
    controls.orbit.apply(transform.target);
    controls.zoom.apply(transform.target);
    controls.pan.apply(transform.target);
    transform.target.pivot.position = target.position;

    // Transition current camera to target
    f64 const dt_s = App::delta_time_s();
    f32 const t = saturate(controls.stiffness * dt_s);
    camera_transition(transform.current, transform.target, t);
}

void OrbitCamera::frame_target()
{
    controls.zoom.distance = target.radius / std::sin(frustum.fov_y * 0.5);
    controls.pan.offset = {};
}

void OrbitCamera::handle_event(App::Event const& event)
{
    camera_handle_mouse_event(
        event,
        controls.zoom,
        &controls.orbit,
        &controls.pan,
        1.0f,
        0.1f,
        input.mouse_down);

    camera_handle_touch_event(
        event,
        controls.zoom,
        &controls.orbit,
        &controls.pan,
        1.0f,
        input.last_touch_points,
        input.last_num_touches);
}

Mat4<f32> OrbitCamera::make_world_to_view() const
{
    return transform.current.transform().inverse_to_matrix();
}

Mat4<f32> OrbitCamera::make_view_to_clip() const
{
    return make_perspective<NdcType_OpenGl>(
        frustum.fov_y,
        App::aspect(),
        frustum.clip_near,
        frustum.clip_far);
}

} // namespace dr