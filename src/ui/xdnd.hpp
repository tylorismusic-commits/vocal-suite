// Vocal Suite — minimal XDND drop target for preset files
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Both the CLAP editor and the LV2 UI use an X11 window, so a small XDND
// target keeps drag-and-drop available without adding GTK/Qt.

#pragma once

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <utility>

namespace vocal {

class XdndDropTarget {
public:
  using Callback = std::function<void(const std::string&)>;

  void init(Display* dpy, Window win, Callback callback) {
    dpy_ = dpy;
    win_ = win;
    callback_ = std::move(callback);
    aware_ = XInternAtom(dpy_, "XdndAware", False);
    enter_ = XInternAtom(dpy_, "XdndEnter", False);
    position_ = XInternAtom(dpy_, "XdndPosition", False);
    status_ = XInternAtom(dpy_, "XdndStatus", False);
    drop_ = XInternAtom(dpy_, "XdndDrop", False);
    finished_ = XInternAtom(dpy_, "XdndFinished", False);
    leave_ = XInternAtom(dpy_, "XdndLeave", False);
    selection_ = XInternAtom(dpy_, "XdndSelection", False);
    actionCopy_ = XInternAtom(dpy_, "XdndActionCopy", False);
    uriList_ = XInternAtom(dpy_, "text/uri-list", False);
    long version = 5;
    XChangeProperty(dpy_, win_, aware_, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(&version), 1);
  }

  bool handle(const XEvent& ev) {
    if (!dpy_) {
      return false;
    }
    if (ev.type == ClientMessage) {
      if (ev.xclient.message_type == enter_) {
        source_ = static_cast<Window>(ev.xclient.data.l[0]);
        return true;
      }
      if (ev.xclient.message_type == position_) {
        const Window source = static_cast<Window>(ev.xclient.data.l[0]);
        if (source) {
          source_ = source;
          sendStatus(source, true);
        }
        return true;
      }
      if (ev.xclient.message_type == drop_) {
        const Window source = static_cast<Window>(ev.xclient.data.l[0]);
        if (source) {
          source_ = source;
          const Time when = static_cast<Time>(ev.xclient.data.l[2]);
          XConvertSelection(dpy_, selection_, uriList_, selection_, win_,
                            when ? when : CurrentTime);
        }
        return true;
      }
      if (ev.xclient.message_type == leave_) {
        source_ = 0;
        return true;
      }
    }
    if (ev.type == SelectionNotify && ev.xselection.property == selection_ &&
        ev.xselection.target == uriList_) {
      std::string path;
      if (readSelection(path) && !path.empty() && callback_) {
        callback_(path);
      }
      sendFinished();
      return true;
    }
    return false;
  }

  void reset() { source_ = 0; }

private:
  void sendStatus(Window source, bool accept) {
    if (!source) {
      return;
    }
    XEvent reply{};
    reply.xclient.type = ClientMessage;
    reply.xclient.window = source;
    reply.xclient.message_type = status_;
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = static_cast<long>(win_);
    reply.xclient.data.l[1] = accept ? 1 : 0;
    reply.xclient.data.l[2] = 0;
    reply.xclient.data.l[3] = 0;
    reply.xclient.data.l[4] = accept ? static_cast<long>(actionCopy_) : 0;
    XSendEvent(dpy_, source, False, NoEventMask, &reply);
    XFlush(dpy_);
  }

  void sendFinished() {
    if (!source_) {
      return;
    }
    XEvent reply{};
    reply.xclient.type = ClientMessage;
    reply.xclient.window = source_;
    reply.xclient.message_type = finished_;
    reply.xclient.format = 32;
    reply.xclient.data.l[0] = static_cast<long>(win_);
    reply.xclient.data.l[1] = 1;
    reply.xclient.data.l[2] = static_cast<long>(actionCopy_);
    XSendEvent(dpy_, source_, False, NoEventMask, &reply);
    XFlush(dpy_);
    source_ = 0;
  }

  bool readSelection(std::string& path) const {
    Atom actualType = 0;
    int actualFormat = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char* data = nullptr;
    const int rc = XGetWindowProperty(
        dpy_, win_, selection_, 0, 0x1FFFFFFF, False, AnyPropertyType,
        &actualType, &actualFormat, &count, &remaining, &data);
    if (rc != 0 || !data || count == 0) {
      if (data) {
        XFree(data);
      }
      return false;
    }
    const std::string text(reinterpret_cast<char*>(data), count);
    XFree(data);
    size_t start = 0;
    while (start < text.size()) {
      size_t end = text.find_first_of("\r\n", start);
      if (end == std::string::npos) {
        end = text.size();
      }
      const std::string line = text.substr(start, end - start);
      start = end + 1;
      if (line.rfind("file://", 0) != 0) {
        continue;
      }
      std::string uri = line.substr(7);
      if (!uri.empty() && uri[0] != '/') {
        const size_t slash = uri.find('/');
        if (slash == std::string::npos) {
          continue;
        }
        uri = uri.substr(slash);
      }
      path = percentDecode(uri);
      return !path.empty();
    }
    return false;
  }

  static std::string percentDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    auto hex = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return c - 'a' + 10;
      if (c >= 'A' && c <= 'F') return c - 'A' + 10;
      return -1;
    };
    for (size_t i = 0; i < in.size(); ++i) {
      if (in[i] == '%' && i + 2 < in.size()) {
        const int a = hex(in[i + 1]);
        const int b = hex(in[i + 2]);
        if (a >= 0 && b >= 0) {
          out += static_cast<char>((a << 4) | b);
          i += 2;
          continue;
        }
      }
      out += in[i];
    }
    return out;
  }

  Display* dpy_ = nullptr;
  Window win_ = 0;
  Window source_ = 0;
  Callback callback_;
  Atom aware_ = 0;
  Atom enter_ = 0;
  Atom position_ = 0;
  Atom status_ = 0;
  Atom drop_ = 0;
  Atom finished_ = 0;
  Atom leave_ = 0;
  Atom selection_ = 0;
  Atom actionCopy_ = 0;
  Atom uriList_ = 0;
};

} // namespace vocal
