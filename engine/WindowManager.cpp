#include "WindowManager.h"

#include <algorithm>

namespace ECSEngine {

WindowManager::WindowManager(unsigned int width, unsigned int height, const std::string& title)
    : mWindow(std::make_shared<sf::RenderWindow>(sf::VideoMode({width, height}), title)),
      mWorldCenter(0, 0),
      mWorldUnitsPerPixel(1.f) {}

std::shared_ptr<sf::RenderWindow> WindowManager::GetWindow() const { return mWindow; }

Rect WindowManager::GetWindowBounds() const {
    return WindowToWorld(
        Rect{Point2D(0, 0), (float)mWindow->getSize().x, (float)mWindow->getSize().y});
}

void WindowManager::DebugDrawRect(const Rect& r, int red, int green, int blue, int alpha) {
    Rect wr = WorldToWindow(r);
    sf::RectangleShape shape({wr.width, wr.height});
    shape.setPosition({wr.topLeft.x, wr.topLeft.y});
    shape.setFillColor(sf::Color(red, green, blue, alpha));
    mWindow->draw(shape);
}

void WindowManager::DebugFrameRect(const Rect& r, int red, int green, int blue, int alpha) {
    Rect wr = WorldToWindow(r);
    sf::RectangleShape shape({wr.width, wr.height});
    shape.setPosition({wr.topLeft.x, wr.topLeft.y});
    shape.setFillColor(sf::Color::Transparent);
    shape.setOutlineColor(sf::Color(red, green, blue, alpha));
    shape.setOutlineThickness(1.f);
    mWindow->draw(shape);
}

Rect WindowManager::WindowToWorld(const Rect& rect) const {
    Point2D tl = WindowToWorld(rect.topLeft);
    return Rect{tl, rect.width * mWorldUnitsPerPixel, rect.height * mWorldUnitsPerPixel};
}

Point2D WindowManager::WindowToWorld(const Point2D& pt) const {
    float halfW = mWindow->getSize().x / 2.f, halfH = mWindow->getSize().y / 2.f;
    return Point2D((pt.x - halfW) * mWorldUnitsPerPixel + mWorldCenter.x,
                   (pt.y - halfH) * mWorldUnitsPerPixel + mWorldCenter.y);
}

Rect WindowManager::WorldToWindow(const Rect& rect) const {
    Point2D tl = WorldToWindow(rect.topLeft);
    return Rect{tl, rect.width / mWorldUnitsPerPixel, rect.height / mWorldUnitsPerPixel};
}

Point2D WindowManager::WorldToWindow(const Point2D& pt) const {
    float halfW = mWindow->getSize().x / 2.f, halfH = mWindow->getSize().y / 2.f;
    return Point2D((pt.x - mWorldCenter.x) / mWorldUnitsPerPixel + halfW,
                   (pt.y - mWorldCenter.y) / mWorldUnitsPerPixel + halfH);
}

void WindowManager::SetCamera(const Point2D& worldPt, const Point2D& screenPt) {
    Point2D currentWorldAtScreenPt = WindowToWorld(screenPt);
    mWorldCenter.x += worldPt.x - currentWorldAtScreenPt.x;
    mWorldCenter.y += worldPt.y - currentWorldAtScreenPt.y;
}

Point2D WindowManager::GetWorldCenter() const { return mWorldCenter; }

float WindowManager::GetWorldScale() const { return mWorldUnitsPerPixel; }

void WindowManager::SetWorldCenter(const Point2D& worldCenter) { mWorldCenter = worldCenter; }

void WindowManager::SetWorldScale(float worldUnitsPerPixel) {
    mWorldUnitsPerPixel = worldUnitsPerPixel;
}

void WindowManager::SetWorldRect(const Rect& worldCameraRect) {
    mWorldUnitsPerPixel = std::max(worldCameraRect.width / mWindow->getSize().x,
                                   worldCameraRect.height / mWindow->getSize().y);
    mWorldCenter = Point2D(worldCameraRect.topLeft.x + worldCameraRect.width / 2.f,
                           worldCameraRect.topLeft.y + worldCameraRect.height / 2.f);
}

float WindowManager::WindowToWorld(float x) const { return x * mWorldUnitsPerPixel; }

float WindowManager::WorldToWindow(float x) const { return x / mWorldUnitsPerPixel; }

}  // namespace ECSEngine
