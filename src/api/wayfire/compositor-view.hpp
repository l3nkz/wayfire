#pragma once

#include "wayfire/view.hpp"
#include <wayfire/config/types.hpp>

namespace wf
{
/**
 * Base class for compositor views that need to interact with the keyboard
 */
class compositor_interactive_view_t
{
  public:
    void handle_keyboard_enter()
    {}
    void handle_keyboard_leave()
    {}
    void handle_key(uint32_t key, uint32_t state)
    {}
};

compositor_interactive_view_t *interactive_view_from_view(
    wf::view_interface_t *view);

/**
 * Input surface implementation which denies all input.
 */
class no_input_surface_t : public wf::input_surface_t
{
  public:
    no_input_surface_t() = default;

    bool accepts_input(wf::pointf_t at) final;
    std::optional<wf::region_t> handle_pointer_enter(
        wf::pointf_t at, bool refocus) final;
    void handle_pointer_leave() final;
    void handle_pointer_button(
        uint32_t time_ms, uint32_t button, wlr_button_state state) final;
    void handle_pointer_motion(uint32_t time_ms, wf::pointf_t at) final;
    void handle_pointer_axis(uint32_t time_ms, wlr_axis_orientation orientation,
        double delta, int32_t delta_discrete, wlr_axis_source source) final;

    void handle_touch_down(uint32_t time_ms, int32_t id, wf::pointf_t at) final;
    void handle_touch_up(uint32_t time_ms, int32_t id, bool finger_lifted) final;
    void handle_touch_motion(uint32_t time_ms, int32_t id, wf::pointf_t at) final;
};

/**
 * Output surface implementation which mirrors an existing view.
 */
class mirror_surface_t : public wf::output_surface_t
{
  public:
    /**
     * Create a new mirror surface. It will get the dimensions of @source
     * and will have the same contents.
     *
     * @param toplevel_damage A callback to handle damage regions in the surface.
     */
    mirror_surface_t(
        wayfire_view source, std::function<void(wf::region_t)> damage_callback);

    wf::point_t get_offset() override;
    wf::dimensions_t get_size() const final;
    void schedule_redraw(const timespec& frame_end) final;
    void set_visible_on_output(wf::output_t *output, bool is_visible) override;
    wf::region_t get_opaque_region() final;
    void simple_render(const wf::framebuffer_t& fb, wf::point_t pos,
        const wf::region_t& damage) final;

  private:
    wayfire_view base_view;
    wf::signal_connection_t on_source_view_unmapped, on_source_view_damaged;
};

/**
 * mirror_view_t is a specialized type of views.
 *
 * They have the same contents as another view. A mirror view's size is
 * determined by the bounding box of the mirrored view. However, the mirror view
 * itself can be on another position, output, etc. and can have additional
 * transforms.
 *
 * A mirror view is mapped as long as its base view is mapped. Afterwards, the
 * view becomes unmapped, until it is destroyed.
 */
class mirror_view_t : public wf::view_interface_t,
    public wf::no_input_surface_t, public wf::mirror_surface_t
{
  protected:
    wf::signal_connection_t on_source_view_unmapped;
    wayfire_view base_view;
    int x;
    int y;

    mirror_view_t(const mirror_view_t&) = delete;
    mirror_view_t(mirror_view_t&&) = delete;
    mirror_view_t& operator =(const mirror_view_t&) = delete;
    mirror_view_t& operator =(mirror_view_t&&) = delete;

  public:
    /**
     * Create a new mirror view for the given base view. When using mirror
     * views, the view should be manually added to the appropriate layer in
     * workspace-manager.
     *
     * Note: a map event is not emitted for mirror views by default.
     */
    mirror_view_t(wayfire_view base_view);
    virtual ~mirror_view_t();

    /**
     * Close the view. This means unsetting the base view and setting the state
     * of the view to unmapped.
     *
     * This will also fire the unmap event on the mirror view.
     */
    virtual void close() override;

    /* surface_interface_t implementation */
    virtual bool is_mapped() const override;

    /* view_interface_t implementation */
    virtual void move(int x, int y) override;
    virtual wf::geometry_t get_output_geometry() override;

    virtual wlr_surface *get_keyboard_focus_surface() override;
    virtual bool is_focuseable() const override;
    virtual bool should_be_decorated() override;

    input_surface_t& input() override;
    output_surface_t& output() override;
};

/**
 * Output surface implementation which provides a solid color rectangle with
 * a border.
 */
class solid_bordered_surface_t : public wf::output_surface_t
{
  public:
    /**
     * Create a new solid bordered surface.
     *
     * @param damage_cb A callback to be called whenever the color, the border
     *   of the surface and/or its geometry changes.
     */
    solid_bordered_surface_t(std::function<void()> damage_cb);

    wf::point_t get_offset() override;
    wf::dimensions_t get_size() const final;
    void schedule_redraw(const timespec& frame_end) final;
    void set_visible_on_output(wf::output_t *output, bool is_visible) override;
    wf::region_t get_opaque_region() final;
    void simple_render(const wf::framebuffer_t& fb, wf::point_t pos,
        const wf::region_t& damage) final;

    /** Set the rectangle size. */
    void set_size(wf::dimensions_t new_size);
    /** Set the view color. Color's alpha is not premultiplied */
    void set_color(wf::color_t color);
    /** Set the view border color. Color's alpha is not premultiplied */
    void set_border_color(wf::color_t border);
    /** Set the border width. */
    void set_border(int width);

  protected:
    wf::color_t _color = {0, 0, 0, 0};
    wf::color_t _border_color = {0, 0, 0, 0};
    int border = 0;

    wf::dimensions_t size = {1, 1};
    std::function<void()> damage_cb;
};

/**
 * color_rect_view_t represents another common type of compositor view - a
 * view which is simply a colored rectangle with a border.
 */
class color_rect_view_t : public wf::view_interface_t,
    public wf::no_input_surface_t, public wf::solid_bordered_surface_t
{
    wf::point_t position = {0, 0};
    bool _is_mapped = true;

  public:
    /**
     * Create a colored rect view. The map signal is not fired by default.
     * The creator of the colored view should also add it to the desired layer.
     */
    color_rect_view_t();

    /**
     * Emit the unmap signal and then drop the internal reference.
     */
    virtual void close() override;

    /* required for surface_interface_t */
    virtual bool is_mapped() const override;

    input_surface_t& input() override;
    output_surface_t& output() override;

    /* required for view_interface_t */
    virtual void move(int x, int y) override;
    virtual void resize(int w, int h) override;
    virtual wf::geometry_t get_output_geometry() override;

    virtual wlr_surface *get_keyboard_focus_surface() override;
    virtual bool is_focuseable() const override;
    virtual bool should_be_decorated() override;
};
}
