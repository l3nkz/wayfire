#include <wayfire/plugin.hpp>
#include "wayfire/view.hpp"
#include "wayfire/view-transform.hpp"
#include "wayfire/output.hpp"
#include "wayfire/core.hpp"
#include <wayfire/workspace-manager.hpp>
#include <wayfire/signal-definitions.hpp>
#include <linux/input.h>

#include <glm/gtc/matrix_transform.hpp>

static const char *transformer_3d = "wrot-3d";
static const char *transformer_2d = "wrot-2d";

static double cross(double x1, double y1, double x2, double y2) // cross product
{
    return x1 * y2 - x2 * y1;
}

static double vlen(double x1, double y1) // length of vector centered at the origin
{
    return std::sqrt(x1 * x1 + y1 * y1);
}

enum class mode
{
    NONE,
    ROT_2D,
    ROT_3D,
};

class wf_wrot : public wf::plugin_interface_t
{
    wf::button_callback call;
    wf::option_wrapper_t<double> reset_radius{"wrot/reset_radius"};
    wf::option_wrapper_t<int> sensitivity{"wrot/sensitivity"};
    wf::option_wrapper_t<bool> invert{"wrot/invert"};

    wf::pointf_t last_position;
    wayfire_view current_view = nullptr;

    mode current_mode = mode::NONE;

    void reset_all()
    {
        for (auto v : output->workspace->get_views_in_layer(wf::LAYER_WORKSPACE))
        {
            v->pop_transformer(transformer_3d);
            v->pop_transformer(transformer_2d);
        }
    }

    wf::button_callback call_3d = [this] (auto)
    {
        if (current_mode != mode::NONE)
        {
            return false;
        }

        if (!output->activate_plugin(grab_interface))
        {
            return false;
        }

        current_view = wf::get_core().get_cursor_focus().view();
        if (!current_view || (current_view->role != wf::VIEW_ROLE_TOPLEVEL))
        {
            output->deactivate_plugin(grab_interface);

            return false;
        }

        output->focus_view(current_view, true);
        current_view->connect_signal("unmapped", &current_view_unmapped);
        grab_interface->grab();

        last_position = output->get_cursor_position();
        current_mode  = mode::ROT_3D;
        return true;
    };

    wf::key_callback reset = [this] (auto)
    {
        reset_all();
        return true;
    };

    wf::key_callback reset_one = [this] (auto)
    {
        auto view = output->get_active_view();
        if (view)
        {
            view->pop_transformer(transformer_3d);
            view->pop_transformer(transformer_2d);
        }

        return true;
    };

    wf::signal_connection_t current_view_unmapped = [this] (wf::signal_data_t *data)
    {
        auto view = wf::get_signaled_view(data);
        if (grab_interface->is_grabbed() && (current_view == view))
        {
            current_view = nullptr;
            input_released();
        }
    };

    void motion_2d(int x, int y)
    {
        if (!current_view->get_transformer(transformer_2d))
        {
            current_view->add_transformer(std::make_unique<wf::view_2D>(
                current_view), transformer_2d);
        }

        auto tr = dynamic_cast<wf::view_2D*>(current_view->get_transformer(
            transformer_2d).get());
        assert(tr);

        current_view->damage();

        auto g = current_view->get_wm_geometry();

        double cx = g.x + g.width / 2.0;
        double cy = g.y + g.height / 2.0;

        double x1 = last_position.x - cx, y1 = last_position.y - cy;
        double x2 = x - cx, y2 = y - cy;

        if (vlen(x2, y2) <= reset_radius)
        {
            return current_view->pop_transformer(transformer_2d);
        }

        /* cross(a, b) = |a| * |b| * sin(a, b) */
        tr->angle -= std::asin(cross(x1, y1, x2, y2) / vlen(x1, y1) / vlen(x2,
            y2));

        current_view->damage();

        last_position = {1.0 * x, 1.0 * y};
    }

    void motion_3d(int x, int y)
    {
        if ((x == last_position.x) && (y == last_position.y))
        {
            return;
        }

        if (!current_view->get_transformer(transformer_3d))
        {
            current_view->add_transformer(std::make_unique<wf::view_3D>(
                current_view), transformer_3d);
        }

        auto tr = dynamic_cast<wf::view_3D*>(current_view->get_transformer(
            transformer_3d).get());
        assert(tr);

        current_view->damage();
        float dx     = x - last_position.x;
        float dy     = y - last_position.y;
        float ascale = glm::radians(sensitivity / 60.0f);
        float dir    = invert ? -1.f : 1.f;
        tr->rotation = glm::rotate<float>(tr->rotation, vlen(dx, dy) * ascale,
            {dir *dy, dir * dx, 0});
        current_view->damage();

        last_position = {(double)x, (double)y};
    }

  public:
    void init() override
    {
        grab_interface->name = "wrot";
        grab_interface->capabilities = wf::CAPABILITY_GRAB_INPUT;

        call = [=] (auto)
        {
            if (current_mode != mode::NONE)
            {
                return false;
            }

            if (!output->activate_plugin(grab_interface))
            {
                return false;
            }

            current_view = wf::get_core().get_cursor_focus().view();
            if (!current_view || (current_view->role != wf::VIEW_ROLE_TOPLEVEL))
            {
                output->deactivate_plugin(grab_interface);

                return false;
            }

            output->focus_view(current_view, true);
            current_view->connect_signal("unmapped", &current_view_unmapped);
            grab_interface->grab();

            last_position = output->get_cursor_position();
            current_mode  = mode::ROT_2D;
            return true;
        };

        output->add_button(
            wf::option_wrapper_t<wf::buttonbinding_t>("wrot/activate"), &call);
        output->add_button(
            wf::option_wrapper_t<wf::buttonbinding_t>("wrot/activate-3d"), &call_3d);
        output->add_key(wf::option_wrapper_t<wf::keybinding_t>{"wrot/reset"},
            &reset);
        output->add_key(wf::option_wrapper_t<wf::keybinding_t>{"wrot/reset-one"},
            &reset_one);

        grab_interface->callbacks.pointer.motion = [=] (int x, int y)
        {
            if (current_mode == mode::ROT_2D)
            {
                motion_2d(x, y);
            } else if (current_mode == mode::ROT_3D)
            {
                motion_3d(x, y);
            }
        };

        grab_interface->callbacks.pointer.button = [=] (uint32_t, uint32_t s)
        {
            if (s == WLR_BUTTON_RELEASED)
            {
                input_released();
            }
        };

        grab_interface->callbacks.cancel = [=] ()
        {
            if (grab_interface->is_grabbed())
            {
                input_released();
            }
        };
    }

    void input_released()
    {
        grab_interface->ungrab();
        output->deactivate_plugin(grab_interface);
        current_view_unmapped.disconnect();
        if ((current_mode == mode::ROT_3D) && current_view)
        {
            auto tr = dynamic_cast<wf::view_3D*>(current_view->get_transformer(
                transformer_3d).get());
            if (tr)
            {
                /* check if the view was rotated to perpendicular to the screen
                 * and move it a bit more, so it will not get "stuck" that way */
                glm::vec4 n{0, 0, 1.f, 0};
                glm::vec4 x = tr->rotation * n;
                auto dot    = glm::dot(n, x);
                if (std::abs(dot) < 0.05)
                {
                    /* rotate 2.5 degrees around an axis perpendicular to x */
                    current_view->damage();
                    tr->rotation = glm::rotate<float>(tr->rotation, glm::radians(
                        dot < 0 ? -2.5f : 2.5f), {x.y, -x.x, 0});
                    current_view->damage();
                }
            }
        }

        current_mode = mode::NONE;
    }

    void fini() override
    {
        if (grab_interface->is_grabbed())
        {
            input_released();
        }

        reset_all();

        output->rem_binding(&call);
        output->rem_binding(&call_3d);
        output->rem_binding(&reset);
        output->rem_binding(&reset_one);
    }
};

DECLARE_WAYFIRE_PLUGIN(wf_wrot);
