#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void *getMetalLayerFromWindow(void *nsWindow) {
  NSWindow *window = (__bridge NSWindow *)nsWindow;
  NSView *view = [window contentView];
  [view setWantsLayer:YES];
  CAMetalLayer *metalLayer = [CAMetalLayer layer];
  [view setLayer:metalLayer];
  return (__bridge void *)metalLayer;
}
