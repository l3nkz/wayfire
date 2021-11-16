#ifndef WF_SUBSURFACE_HPP
#define WF_SUBSURFACE_HPP

#include "surface-impl.hpp"
#include <wayfire/nonstd/wlroots-full.hpp>

namespace wf
{
class subsurface_implementation_t : public wlr_child_surface_base_t
{
    wl_listener_wrapper on_map, on_unmap, on_destroy;
    wlr_subsurface *sub;

    wf::signal_connection_t on_removed;

  public:
    subsurface_implementation_t(wlr_subsurface *s);
    wf::point_t get_offset() final;
};
}


#endif /* end of include guard: WF_SUBSURFACE_HPP */
