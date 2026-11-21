// Vocal Suite - Cocoa view host
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Draws the cairo Panel into an NSView and forwards mouse, wheel and keyboard
// events.  Coordinates map from the view bounds to the 1040x640 logical panel,
// keeping the aspect ratio and centring.

#include "ui/cocoa_view.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <cairo.h>

#import <Cocoa/Cocoa.h>

#include "params.hpp"
#include "ui/panel.hpp"

using namespace vocal;

namespace {

struct ViewState {
  Panel* panel = nullptr;
  Shared* shared = nullptr;
  PanelState* state = nullptr;
  std::function<void()> requestFlush;
};

struct Placement {
  double scale = 1.0;
  double offsetX = 0.0;
  double offsetY = 0.0;
};

Placement placementFor(double width, double height) {
  Placement p;
  p.scale = std::min(width / static_cast<double>(kPanelWidth),
                     height / static_cast<double>(kPanelHeight));
  if (!(p.scale > 0.0)) p.scale = 1.0;
  p.offsetX = (width - kPanelWidth * p.scale) * 0.5;
  p.offsetY = (height - kPanelHeight * p.scale) * 0.5;
  return p;
}

void logicalPoint(NSView* view, NSPoint point, double* lx, double* ly) {
  const Placement place = placementFor(view.bounds.size.width,
                                       view.bounds.size.height);
  const double y = view.bounds.size.height - point.y;  // panel is y-down
  *lx = (point.x - place.offsetX) / place.scale;
  *ly = (y - place.offsetY) / place.scale;
}

} // namespace

@interface VsCocoaView : NSView {
 @public
  ViewState* viewState;
  cairo_surface_t* surface;
  cairo_t* context;
  int surfaceWidth;
  int surfaceHeight;
  NSTimer* refreshTimer;
}
- (void)tick:(NSTimer*)timer;
@end

@implementation VsCocoaView

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)isOpaque {
  return YES;
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [refreshTimer invalidate];
  refreshTimer = nil;
  if (self.window) {
    refreshTimer = [NSTimer timerWithTimeInterval:1.0 / 30.0
                                           target:self
                                         selector:@selector(tick:)
                                         userInfo:nil
                                          repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:refreshTimer
                                 forMode:NSRunLoopCommonModes];
  }
}

- (void)tick:(NSTimer*)timer {
  (void)timer;
  if (viewState && viewState->panel && viewState->shared && viewState->state) {
    viewState->panel->tick(*viewState->shared, *viewState->state);
    if (viewState->requestFlush && viewState->shared->anyDirty()) {
      viewState->requestFlush();
    }
    [self setNeedsDisplay:YES];
  }
}

- (void)dealloc {
  [refreshTimer invalidate];
  refreshTimer = nil;
  if (context) cairo_destroy(context);
  if (surface) cairo_surface_destroy(surface);
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  const double width = self.bounds.size.width;
  const double height = self.bounds.size.height;
  if (width < 1.0 || height < 1.0) return;

  const double backing = self.window ? self.window.backingScaleFactor : 1.0;
  const int pixelWidth = static_cast<int>(std::lround(width * backing));
  const int pixelHeight = static_cast<int>(std::lround(height * backing));
  if (pixelWidth <= 0 || pixelHeight <= 0) return;

  if (!surface || surfaceWidth != pixelWidth || surfaceHeight != pixelHeight) {
    if (context) cairo_destroy(context);
    if (surface) cairo_surface_destroy(surface);
    surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, pixelWidth,
                                         pixelHeight);
    context = cairo_create(surface);
    surfaceWidth = pixelWidth;
    surfaceHeight = pixelHeight;
  }
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) return;

  const Placement place = placementFor(width, height);
  cairo_set_source_rgb(context, 0.07, 0.07, 0.07);
  cairo_paint(context);
  cairo_save(context);
  cairo_translate(context, place.offsetX * backing, place.offsetY * backing);
  cairo_scale(context, place.scale * backing, place.scale * backing);
  viewState->panel->draw(context, *viewState->shared, *viewState->state);
  cairo_restore(context);
  cairo_surface_flush(surface);

  CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider = CGDataProviderCreateWithData(
      nullptr, cairo_image_surface_get_data(surface),
      static_cast<size_t>(pixelWidth) * pixelHeight * 4, nullptr);
  CGImageRef image = CGImageCreate(
      pixelWidth, pixelHeight, 8, 32, cairo_image_surface_get_stride(surface),
      space, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little,
      provider, nullptr, false, kCGRenderingIntentDefault);
  if (image) {
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
    CGContextDrawImage(ctx, CGRectMake(0.0, 0.0, width, height), image);
    CGImageRelease(image);
  }
  CGDataProviderRelease(provider);
  CGColorSpaceRelease(space);
}

- (void)mouseDown:(NSEvent*)event {
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  [self.window makeFirstResponder:self];
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  double lx = 0.0, ly = 0.0;
  logicalPoint(self, point, &lx, &ly);
  const bool shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
  viewState->panel->pointerDown(*viewState->shared, *viewState->state, lx, ly,
                                shift);
  [self setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent*)event {
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  [self.window makeFirstResponder:self];
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  double lx = 0.0, ly = 0.0;
  logicalPoint(self, point, &lx, &ly);
  viewState->panel->contextDown(*viewState->shared, *viewState->state, lx, ly);
  [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent*)event {
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  double lx = 0.0, ly = 0.0;
  logicalPoint(self, point, &lx, &ly);
  const bool shift = (event.modifierFlags & NSEventModifierFlagShift) != 0;
  viewState->panel->pointerMove(*viewState->shared, *viewState->state, lx, ly,
                                shift);
  [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent*)event {
  (void)event;
  if (viewState && viewState->panel && viewState->shared) {
    viewState->panel->pointerUp(*viewState->shared);
    [self setNeedsDisplay:YES];
  }
}

- (void)scrollWheel:(NSEvent*)event {
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
  double lx = 0.0, ly = 0.0;
  logicalPoint(self, point, &lx, &ly);
  double dy = event.scrollingDeltaY;
  if (event.isDirectionInvertedFromDevice) dy = -dy;
  viewState->panel->wheel(*viewState->shared, *viewState->state, lx, ly, dy);
  [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent*)event {
  if (!viewState || !viewState->panel || !viewState->shared ||
      !viewState->state) {
    return;
  }
  int key = 0;
  switch (event.keyCode) {
    case 36:
    case 76:
      key = 13;  // Return / keypad Enter
      break;
    case 53:
      key = 27;  // Escape
      break;
    case 51:
      key = 8;  // Backspace
      break;
    default:
      break;
  }
  const char* text = nullptr;
  if (key == 0) {
    text = [event.characters UTF8String];
    if (!text || !*text) return;
  }
  viewState->panel->keyEvent(*viewState->shared, *viewState->state, key, text);
  [self setNeedsDisplay:YES];
}

@end

namespace vocal {

struct CocoaView::Impl {
  VsCocoaView* view = nil;
  ViewState state;
};

CocoaView::CocoaView() : impl_(std::make_unique<Impl>()) {}

CocoaView::~CocoaView() { detach(); }

void CocoaView::setContent(Panel* panel, Shared* shared, PanelState* state) {
  impl_->state.panel = panel;
  impl_->state.shared = shared;
  impl_->state.state = state;
  if (impl_->view) impl_->view->viewState = &impl_->state;
}

void CocoaView::setFlushCallback(std::function<void()> callback) {
  impl_->state.requestFlush = std::move(callback);
  if (impl_->view) impl_->view->viewState = &impl_->state;
}

void CocoaView::attach(void* parentNsView) {
  if (!impl_->view) {
    impl_->view = [[VsCocoaView alloc]
        initWithFrame:NSMakeRect(0.0, 0.0, kPanelWidth, kPanelHeight)];
    impl_->view->viewState = &impl_->state;
    impl_->view->surface = nullptr;
    impl_->view->context = nullptr;
    impl_->view->surfaceWidth = 0;
    impl_->view->surfaceHeight = 0;
    impl_->view->refreshTimer = nil;
    impl_->view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  }
  NSView* parent = (__bridge NSView*)parentNsView;
  if (parent) {
    [parent addSubview:impl_->view];
    [impl_->view setFrame:[parent bounds]];
    [impl_->view setNeedsDisplay:YES];
  }
}

void CocoaView::detach() {
  if (!impl_->view) return;
  [impl_->view removeFromSuperview];
  impl_->view = nil;
}

void CocoaView::resize(int width, int height) {
  if (!impl_->view) return;
  [impl_->view
      setFrame:NSMakeRect(0.0, 0.0, std::max(1, width), std::max(1, height))];
  [impl_->view setNeedsDisplay:YES];
}

} // namespace vocal
