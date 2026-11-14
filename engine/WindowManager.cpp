#include "WindowManager.h"

#include <algorithm>

namespace ECSEngine {

// creates the window, with the camera centered on world (0, 0) at 1 world unit per pixel
WindowManager::WindowManager(unsigned int width, unsigned int height, const std::string& title)
    : mWindow(std::make_shared<sf::RenderWindow>(sf::VideoMode({width, height}), title)),
      mWorldCenter(0, 0),
      mWorldUnitsPerPixel(1.f) {
    // all movement is per frame, so cap the frame rate to keep game speed the same on every machine
    mWindow->setFramerateLimit(60);
}

// the shared_ptr keeps the window alive for as long as someone holds it
std::shared_ptr<sf::RenderWindow> WindowManager::GetWindow() const { return mWindow; }

// the part of the world the window currently shows, in world coordinates
Rect WindowManager::GetWindowBounds() const {
    return WindowToWorld(
        Rect{Point2D(0, 0), (float)mWindow->getSize().x, (float)mWindow->getSize().y});
}

// draws a filled rectangle given in world coordinates
void WindowManager::DebugDrawRect(const Rect& r, int red, int green, int blue, int alpha) {
    Rect wr = WorldToWindow(r);
    sf::RectangleShape shape({wr.width, wr.height});
    shape.setPosition({wr.topLeft.x, wr.topLeft.y});
    shape.setFillColor(sf::Color(red, green, blue, alpha));
    mWindow->draw(shape);
}

// draws only the outline of a rectangle given in world coordinates
void WindowManager::DebugFrameRect(const Rect& r, int red, int green, int blue, int alpha) {
    Rect wr = WorldToWindow(r);
    sf::RectangleShape shape({wr.width, wr.height});
    shape.setPosition({wr.topLeft.x, wr.topLeft.y});
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color(red, green, blue, alpha));
    shape.setOutlineThickness(1.f);
    mWindow->draw(shape);
}

// window rect -> world rect: convert the corner, then scale the size up to world units
Rect WindowManager::WindowToWorld(const Rect& rect) const {
    Point2D tl = WindowToWorld(rect.topLeft);
    return Rect{tl, rect.width * mWorldUnitsPerPixel, rect.height * mWorldUnitsPerPixel};
}

// window point -> world point
// the window center shows mWorldCenter, so measure from the window center,
// scale pixels to world units, then add the world center
Point2D WindowManager::WindowToWorld(const Point2D& pt) const {
    float halfW = mWindow->getSize().x / 2.f, halfH = mWindow->getSize().y / 2.f;
    return Point2D((pt.x - halfW) * mWorldUnitsPerPixel + mWorldCenter.x,
                   (pt.y - halfH) * mWorldUnitsPerPixel + mWorldCenter.y);
}

// world rect -> window rect: convert the corner, then scale the size down to pixels
Rect WindowManager::WorldToWindow(const Rect& rect) const {
    Point2D tl = WorldToWindow(rect.topLeft);
    return Rect{tl, rect.width / mWorldUnitsPerPixel, rect.height / mWorldUnitsPerPixel};
}

// world point -> window point (the reverse of WindowToWorld)
// measure from the world center, scale world units to pixels, then add the window center
Point2D WindowManager::WorldToWindow(const Point2D& pt) const {
    float halfW = mWindow->getSize().x / 2.f, halfH = mWindow->getSize().y / 2.f;
    return Point2D((pt.x - mWorldCenter.x) / mWorldUnitsPerPixel + halfW,
                   (pt.y - mWorldCenter.y) / mWorldUnitsPerPixel + halfH);
}

// moves the camera so that worldPt is shown at screenPt (scale stays the same)
void WindowManager::SetCamera(const Point2D& worldPt, const Point2D& screenPt) {
    // find what world point is under screenPt now, then shift the center by the difference
    Point2D currentWorldAtScreenPt = WindowToWorld(screenPt);
    mWorldCenter.x += worldPt.x - currentWorldAtScreenPt.x;
    mWorldCenter.y += worldPt.y - currentWorldAtScreenPt.y;
}

Point2D WindowManager::GetWorldCenter() const { return mWorldCenter; }

float WindowManager::GetWorldScale() const { return mWorldUnitsPerPixel; }

// the world point shown at the center of the window
void WindowManager::SetWorldCenter(const Point2D& worldCenter) { mWorldCenter = worldCenter; }

// world units per pixel: bigger values zoom out, smaller values zoom in
void WindowManager::SetWorldScale(float worldUnitsPerPixel) {
    mWorldUnitsPerPixel = worldUnitsPerPixel;
}

// makes the window show the given world rectangle
void WindowManager::SetWorldRect(const Rect& worldCameraRect) {
    // use the larger of the two ratios so the whole rect fits (the other axis shows extra)
    mWorldUnitsPerPixel = std::max(worldCameraRect.width / mWindow->getSize().x,
                                   worldCameraRect.height / mWindow->getSize().y);
    // center the camera on the middle of the rect
    mWorldCenter = Point2D(worldCameraRect.topLeft.x + worldCameraRect.width / 2.f,
                           worldCameraRect.topLeft.y + worldCameraRect.height / 2.f);
}

// converts a single length (not a position) between window pixels and world units
float WindowManager::WindowToWorld(float x) const { return x * mWorldUnitsPerPixel; }

float WindowManager::WorldToWindow(float x) const { return x / mWorldUnitsPerPixel; }

}  // namespace ECSEngine
