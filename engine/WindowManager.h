#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "MathUtil.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

namespace ECSEngine {

// owns the SFML window and the camera (which world point is centered and the zoom)
// - world coordinates are what entities use; window coordinates are pixels on screen
// - all values are returned by value except GetWindow, which shares ownership of the window
class WindowManager {
public:
    // creates the window, with the camera centered on world (0, 0) at 1 world unit per pixel
    // the frame rate is capped at 60 fps because all movement is per frame
    WindowManager(unsigned int width, unsigned int height, const std::string& title);
    ~WindowManager() = default;

    // returns the SFML window; the shared_ptr keeps it alive as long as the caller holds it
    std::shared_ptr<sf::RenderWindow> GetWindow() const;

    // the part of the world currently visible, in world coordinates (computed each call)
    Rect GetWindowBounds() const;

    // moves the camera so worldPt appears at screenPt; scale is unchanged
    void SetCamera(const Point2D& worldPt, const Point2D& screenPt);

    // sets the world point shown at the center of the window
    void SetWorldCenter(const Point2D& worldCenter);

    // makes the window show the given world rectangle (centers on it and sets the scale
    // so the whole rect fits; the other axis may show extra)
    void SetWorldRect(const Rect& worldCameraRect);

    // world units per pixel: larger zooms out, smaller zooms in
    void SetWorldScale(float worldUnitsPerPixel);

    // the world point currently shown at the center of the window
    Point2D GetWorldCenter() const;

    // the current world units per pixel
    float GetWorldScale() const;

    // convert lengths / rects / points between window pixels and world units
    // lengths are only scaled; rects and points are also shifted by the camera position
    float WindowToWorld(float x) const;
    float WorldToWindow(float x) const;
    Rect WindowToWorld(const Rect& rect) const;
    Rect WorldToWindow(const Rect& rect) const;
    Point2D WorldToWindow(const Point2D& pt) const;
    Point2D WindowToWorld(const Point2D& pt) const;

    // draws a filled rectangle given in world coordinates (debug only)
    void DebugDrawRect(const Rect& r, int red, int green, int blue, int alpha = 255);

    // draws only the outline of a rectangle given in world coordinates (debug only)
    void DebugFrameRect(const Rect& r, int red, int green, int blue, int alpha = 255);

private:
    std::shared_ptr<sf::RenderWindow> mWindow;

    // the world point shown at the center of the window
    Point2D mWorldCenter;

    // how many world units one window pixel covers (the zoom)
    float mWorldUnitsPerPixel;
};

}  // namespace ECSEngine
