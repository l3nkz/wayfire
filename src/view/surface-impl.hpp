#pragma once

#include <map>
#include <wayfire/opengl.hpp>
#include <wayfire/surface.hpp>
#include <wayfire/util.hpp>
#include <wayfire/nonstd/wlroots-full.hpp>

namespace wf
{
class surface_interface_t::impl
{
  public:
    surface_interface_t *parent_surface;
    std::vector<std::unique_ptr<surface_interface_t>> surface_children_above;
    std::vector<std::unique_ptr<surface_interface_t>> surface_children_below;
    size_t last_cnt_surfaces = 0;

    /**
     * Remove all subsurfaces and emit signals for them.
     */
    void clear_subsurfaces(surface_interface_t *self);

    wf::output_t *output = nullptr;
    static int active_shrink_constraint;

    /**
     * Most surfaces don't have a wlr_surface. However, internal surface
     * implementations can set the underlying surface so that functions like
     *
     * subtract_opaque(), send_frame_done(), etc. work for the surface
     */
    wlr_surface *wsurface = nullptr;
};

/**
 * A base class for views and surfaces which are based on a wlr_surface
 * Any class that derives from wlr_surface_base_t must also derive from
 * surface_interface_t!
 */
class wlr_surface_base_t : public input_surface_t, public output_surface_t
{
  protected:
    wf::wl_listener_wrapper::callback_t handle_new_subsurface;
    wf::wl_listener_wrapper on_commit, on_destroy, on_new_subsurface;

    void apply_surface_damage();
    wlr_surface_base_t(wf::surface_interface_t *self);
    /* Pointer to this as surface_interface, see requirement above */
    wf::surface_interface_t *_as_si = nullptr;

    std::map<wf::output_t*, int> visibility;

  public:
    /* if surface != nullptr, then the surface is mapped */
    wlr_surface *surface = nullptr;

    virtual ~wlr_surface_base_t();

    wlr_surface_base_t(const wlr_surface_base_t &) = delete;
    wlr_surface_base_t(wlr_surface_base_t &&) = delete;
    wlr_surface_base_t& operator =(const wlr_surface_base_t&) = delete;
    wlr_surface_base_t& operator =(wlr_surface_base_t&&) = delete;

    /** @return The offset from the surface coordinates to the actual geometry */
    virtual wf::point_t get_window_offset();

    /** Implementation of input_surface_t */
    wlr_pointer_constraint_v1 *current_constraint = NULL;
    wf::wl_listener_wrapper on_current_constraint_destroy;

    bool accepts_input(wf::pointf_t at) final;
    std::optional<wf::region_t> handle_pointer_enter(wf::pointf_t at,
        bool reenter) final;
    void handle_pointer_leave() final;
    void handle_pointer_button(uint32_t time_ms, uint32_t button,
        wlr_button_state state) final;
    void handle_pointer_motion(uint32_t time_ms, wf::pointf_t at) final;
    void handle_pointer_axis(uint32_t time_ms,
        wlr_axis_orientation orientation, double delta,
        int32_t delta_discrete, wlr_axis_source source) final;

    void handle_touch_down(uint32_t time_ms, int32_t id, wf::pointf_t at) final;
    void handle_touch_up(uint32_t time_ms, int32_t id, bool finger_lifted) final;
    void handle_touch_motion(uint32_t time_ms, int32_t id, wf::pointf_t at) final;

    /** Implementation of output_surface_t */
    wf::point_t get_offset() override;
    wf::dimensions_t get_size() const final;
    void schedule_redraw(const timespec& frame_end) final;
    void set_visible_on_output(wf::output_t *output, bool is_visible) override;
    wf::region_t get_opaque_region() final;
    void simple_render(const wf::framebuffer_t& fb, wf::point_t pos,
        const wf::region_t& damage) final;

  protected:
    virtual void map(wlr_surface *surface);
    virtual void unmap();
    virtual void commit();

    virtual wlr_buffer *get_buffer();
};

/**
 * wlr_child_surface_base_t is a base class for wlr-surface based child
 * surfaces, i.e subsurfaces.
 *
 * However, they can still exist without a parent, for ex. drag icons.
 */
class wlr_child_surface_base_t :
    public surface_interface_t, public wlr_surface_base_t
{
  public:
    wlr_child_surface_base_t(surface_interface_t *self);
    virtual ~wlr_child_surface_base_t();

    wlr_child_surface_base_t(const wlr_child_surface_base_t &) = delete;
    wlr_child_surface_base_t(wlr_child_surface_base_t &&) = delete;
    wlr_child_surface_base_t& operator =(const wlr_child_surface_base_t&) = delete;
    wlr_child_surface_base_t& operator =(wlr_child_surface_base_t&&) = delete;

    bool is_mapped() const override
    {
        return priv->wsurface != nullptr;
    }

    virtual input_surface_t& input() override
    {
        return *this;
    }

    virtual output_surface_t& output() override
    {
        return *this;
    }
};
}
