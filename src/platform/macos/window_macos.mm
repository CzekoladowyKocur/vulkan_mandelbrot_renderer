#include "include/window.hpp"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

#include <array>
#include <utility>

#include "include/vulkan_types.hpp"
#include <vulkan/vulkan_metal.h>

struct macos_window_state {
  std::uint32_t width{};
  std::uint32_t height{};
  std::string name{};
  bool maximized{};

  NSWindow *ns_window{nil};
  NSView *ns_view{nil};
  CAMetalLayer *metal_layer{nil};
  id<NSWindowDelegate> delegate{nil};
  const function_ref<void(const event &)> *callback{nullptr};

  void dispatch(const event &dispatched) noexcept {
    if (callback != nullptr) {
      (*callback)(dispatched);
    }
  }

  void update_size() noexcept {
    const NSRect bounds{[ns_view bounds]};
    const NSRect backing{[ns_view convertRectToBacking:bounds]};

    width = static_cast<std::uint32_t>(backing.size.width);
    height = static_cast<std::uint32_t>(backing.size.height);

    if (metal_layer != nil) {
      metal_layer.drawableSize =
          CGSizeMake(backing.size.width, backing.size.height);
    }
  }

  ~macos_window_state() {
    if (ns_window != nil) {
      [ns_window setDelegate:nil];
      [ns_window close];
      ns_window = nil;
    }
  }
};

@interface mandelbrot_window_delegate : NSObject <NSWindowDelegate> {
  macos_window_state *m_state;
}
- (instancetype)initWithState:(macos_window_state *)state;
@end

@implementation mandelbrot_window_delegate

- (instancetype)initWithState:(macos_window_state *)state {
  self = [super init];
  if (self != nil) {
    m_state = state;
  }
  return self;
}

- (BOOL)windowShouldClose:(NSWindow *)sender {
  m_state->dispatch(window_close_event{});
  return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
  m_state->update_size();
  m_state->dispatch(
      window_resize_event{.width{m_state->width}, .height{m_state->height}});
}

- (void)windowDidMiniaturize:(NSNotification *)notification {
  m_state->width = 0u;
  m_state->height = 0u;
  m_state->dispatch(window_minimize_event{});
}

- (void)windowDidDeminiaturize:(NSNotification *)notification {
  m_state->update_size();
  m_state->dispatch(
      window_resize_event{.width{m_state->width}, .height{m_state->height}});
}

@end

@interface mandelbrot_content_view : NSView
@end

@implementation mandelbrot_content_view

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (void)keyDown:(NSEvent *)event {
}

- (void)keyUp:(NSEvent *)event {
}

@end

static void ensure_application_running() noexcept {
  if (NSApp != nil && [NSApp isRunning]) {
    return;
  }

  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp finishLaunching];
  [NSApp activateIgnoringOtherApps:YES];
}

[[nodiscard]] static key_code
key_code_from_ns_key(const unsigned short ns_key) noexcept {
  switch (ns_key) {
  case 0:
    return key_code::a;
  case 11:
    return key_code::b;
  case 8:
    return key_code::c;
  case 2:
    return key_code::d;
  case 14:
    return key_code::e;
  case 3:
    return key_code::f;
  case 5:
    return key_code::g;
  case 4:
    return key_code::h;
  case 34:
    return key_code::i;
  case 38:
    return key_code::j;
  case 40:
    return key_code::k;
  case 37:
    return key_code::l;
  case 46:
    return key_code::m;
  case 45:
    return key_code::n;
  case 31:
    return key_code::o;
  case 35:
    return key_code::p;
  case 12:
    return key_code::q;
  case 15:
    return key_code::r;
  case 1:
    return key_code::s;
  case 17:
    return key_code::t;
  case 32:
    return key_code::u;
  case 9:
    return key_code::v;
  case 13:
    return key_code::w;
  case 7:
    return key_code::x;
  case 16:
    return key_code::y;
  case 6:
    return key_code::z;
  case 51:
    return key_code::backspace;
  case 48:
    return key_code::tab;
  case 36:
    return key_code::enter;
  case 76:
    return key_code::enter;
  case 53:
    return key_code::escape;
  case 49:
    return key_code::space;
  case 57:
    return key_code::caps_lock;
  case 116:
    return key_code::page_up;
  case 121:
    return key_code::page_down;
  case 119:
    return key_code::end;
  case 115:
    return key_code::home;
  case 114:
    return key_code::insert;
  case 117:
    return key_code::del;
  case 123:
    return key_code::left;
  case 126:
    return key_code::up;
  case 124:
    return key_code::right;
  case 125:
    return key_code::down;
  case 82:
    return key_code::numpad0;
  case 83:
    return key_code::numpad1;
  case 84:
    return key_code::numpad2;
  case 85:
    return key_code::numpad3;
  case 86:
    return key_code::numpad4;
  case 87:
    return key_code::numpad5;
  case 88:
    return key_code::numpad6;
  case 89:
    return key_code::numpad7;
  case 91:
    return key_code::numpad8;
  case 92:
    return key_code::numpad9;
  case 67:
    return key_code::multiply;
  case 69:
    return key_code::add;
  case 78:
    return key_code::subtract;
  case 65:
    return key_code::decimal;
  case 75:
    return key_code::divide;
  case 122:
    return key_code::f1;
  case 120:
    return key_code::f2;
  case 99:
    return key_code::f3;
  case 118:
    return key_code::f4;
  case 96:
    return key_code::f5;
  case 97:
    return key_code::f6;
  case 98:
    return key_code::f7;
  case 100:
    return key_code::f8;
  case 101:
    return key_code::f9;
  case 109:
    return key_code::f10;
  case 103:
    return key_code::f11;
  case 111:
    return key_code::f12;
  case 105:
    return key_code::f13;
  case 107:
    return key_code::f14;
  case 113:
    return key_code::f15;
  case 106:
    return key_code::f16;
  case 64:
    return key_code::f17;
  case 79:
    return key_code::f18;
  case 80:
    return key_code::f19;
  case 90:
    return key_code::f20;
  case 41:
    return key_code::semicolon;
  case 24:
    return key_code::plus;
  case 43:
    return key_code::comma;
  case 27:
    return key_code::minus;
  case 47:
    return key_code::period;
  case 44:
    return key_code::slash;
  case 50:
    return key_code::grave;
  default:
    return key_code::none;
  }
}

[[nodiscard]] static std::pair<key_code, key_code>
modifier_keys_from_ns_key(const unsigned short ns_key) noexcept {
  switch (ns_key) {
  case 56:
    return {key_code::left_shift, key_code::shift};
  case 60:
    return {key_code::right_shift, key_code::shift};
  case 59:
    return {key_code::left_control, key_code::control};
  case 62:
    return {key_code::right_control, key_code::control};
  case 58:
    return {key_code::left_alt, key_code::alt};
  case 61:
    return {key_code::right_alt, key_code::alt};
  case 55:
    return {key_code::left_super, key_code::none};
  case 54:
    return {key_code::right_super, key_code::none};
  default:
    return {key_code::none, key_code::none};
  }
}

[[nodiscard]] static NSEventModifierFlags
modifier_flag_from_ns_key(const unsigned short ns_key) noexcept {
  switch (ns_key) {
  case 56:
  case 60:
    return NSEventModifierFlagShift;
  case 59:
  case 62:
    return NSEventModifierFlagControl;
  case 58:
  case 61:
    return NSEventModifierFlagOption;
  case 55:
  case 54:
    return NSEventModifierFlagCommand;
  default:
    return 0;
  }
}

struct window::detail final : macos_window_state {
  explicit detail(window_props &&props)
      : macos_window_state{.width{props.width},
                           .height{props.height},
                           .name{std::move(props.name)},
                           .maximized{props.maximized}} {}
};

std::span<const char *const> window::get_required_extensions() noexcept {
  static constexpr std::array<const char *, 2u> extensions{
      VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_METAL_SURFACE_EXTENSION_NAME};

  return extensions;
}

std::expected<window, std::error_code>
window::create(window_props &&props) noexcept {
  try {
    window result{std::move(props)};

    if (const auto initialized{result.initialize()}; !initialized) {
      return std::unexpected{initialized.error()};
    }

    return result;
  } catch (const std::bad_alloc &) {
    return std::unexpected{std::make_error_code(std::errc::not_enough_memory)};
  }
}

window::window(window_props &&props)
    : m_detail{std::make_unique<detail>(std::move(props))} {}

window::window(window &&) noexcept = default;

window &window::operator=(window &&) noexcept = default;

window::~window() = default;

std::expected<void, std::error_code> window::initialize() noexcept {
  @autoreleasepool {
    ensure_application_running();

    const NSRect content_rect{
        NSMakeRect(0.0, 0.0, static_cast<CGFloat>(m_detail->width),
                   static_cast<CGFloat>(m_detail->height))};

    const NSWindowStyleMask style_mask{
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
        NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable};

    NSWindow *const ns_window{
        [[NSWindow alloc] initWithContentRect:content_rect
                                    styleMask:style_mask
                                      backing:NSBackingStoreBuffered
                                        defer:NO]};

    if (ns_window == nil) {
      return std::unexpected{
          std::make_error_code(std::errc::not_enough_memory)};
    }

    mandelbrot_content_view *const view{
        [[mandelbrot_content_view alloc] initWithFrame:content_rect]};

    CAMetalLayer *const metal_layer{[CAMetalLayer layer]};
    [view setLayer:metal_layer];
    [view setWantsLayer:YES];

    m_detail->ns_window = ns_window;
    m_detail->ns_view = view;
    m_detail->metal_layer = metal_layer;
    m_detail->delegate =
        [[mandelbrot_window_delegate alloc] initWithState:m_detail.get()];

    [ns_window setContentView:view];
    [ns_window setDelegate:m_detail->delegate];
    [ns_window
        setTitle:[NSString stringWithUTF8String:m_detail->name.c_str()]];
    [ns_window center];
    [ns_window makeFirstResponder:view];
    [ns_window makeKeyAndOrderFront:nil];

    if (m_detail->maximized) {
      [ns_window zoom:nil];
    }

    [metal_layer
        setContentsScale:[ns_window backingScaleFactor]];

    m_detail->update_size();

    return {};
  }
}

std::expected<VkSurfaceKHR, std::error_code>
window::get_surface(const VkInstance instance) noexcept {
  const VkMetalSurfaceCreateInfoEXT surface_create_info{
      .sType{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT},
      .pNext{nullptr},
      .flags{0u},
      .pLayer{m_detail->metal_layer},
  };

  VkSurfaceKHR surface{VK_NULL_HANDLE};
  const VkResult result{vkCreateMetalSurfaceEXT(instance, &surface_create_info,
                                                nullptr, &surface)};

  if (result != VK_SUCCESS) {
    return make_vulkan_error(result);
  }

  return surface;
}

void window::poll(const function_ref<void(const event &)> callback) noexcept {
  @autoreleasepool {
    m_detail->callback = &callback;

    while (true) {
      NSEvent *const ns_event{
          [NSApp nextEventMatchingMask:NSEventMaskAny
                             untilDate:nil
                                inMode:NSDefaultRunLoopMode
                               dequeue:YES]};

      if (ns_event == nil) {
        break;
      }

      switch ([ns_event type]) {
      case NSEventTypeKeyDown: {
        if (![ns_event isARepeat]) {
          m_detail->dispatch(key_pressed_event{
              .code{key_code_from_ns_key([ns_event keyCode])}});
        }
        break;
      }

      case NSEventTypeKeyUp: {
        m_detail->dispatch(key_released_event{
            .code{key_code_from_ns_key([ns_event keyCode])}});
        break;
      }

      case NSEventTypeFlagsChanged: {
        const unsigned short ns_key{[ns_event keyCode]};
        const auto [specific, generic]{modifier_keys_from_ns_key(ns_key)};

        if (specific != key_code::none) {
          const bool pressed{([ns_event modifierFlags] &
                              modifier_flag_from_ns_key(ns_key)) != 0};

          if (pressed) {
            m_detail->dispatch(key_pressed_event{.code{specific}});
            if (generic != key_code::none) {
              m_detail->dispatch(key_pressed_event{.code{generic}});
            }
          } else {
            m_detail->dispatch(key_released_event{.code{specific}});
            if (generic != key_code::none) {
              m_detail->dispatch(key_released_event{.code{generic}});
            }
          }
        }
        break;
      }

      default:
        break;
      }

      [NSApp sendEvent:ns_event];
    }

    m_detail->callback = nullptr;
  }
}

std::pair<std::uint32_t, std::uint32_t> window::get_size() const noexcept {
  return {m_detail->width, m_detail->height};
}
