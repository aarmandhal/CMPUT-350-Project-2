#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "CameraComponent.h"
#include "CollisionComponent.h"
#include "EntityManager.h"
#include "InputComponent.h"
#include "LocationComponent.h"
#include "MathUtil.h"
#include "ScoreComponent.h"
#include "SoundManager.h"
#include "SpawnComponent.h"
#include "SystemManager.h"

namespace ECSEngine {

// screen shake
const float kShakeImpactThreshold = 4.f;   // hits slower than this (resting contact) do not shake
const float kMaxShakeMagnitude = 10.f;     // largest shake offset in pixels
const int kShakeFrameLimit = 10;           // how many frames a shake lasts
const float kShakeMagnitudeScale = 0.25f;  // shake size = impact speed * this

// bouncing
const float kStarRestitution = 0.6f;  // fraction of speed kept when two stars hit
const float kWallRestitution = 0.8f;  // fraction of speed kept when a star hits a wall

// other
const float kLandSoundSpeed = 4.f;   // falls slower than this do not play the landing sound
const float kSideEpsilon = 0.01f;    // small tolerance for floating point edge comparisons

// which side of the moving entity touched the static one
// ground = landed on top of it, ceiling = hit it from below,
// left/right wall = the wall is on that side of the entity
enum class ContactSide { kNone, kGround, kCeiling, kLeftWall, kRightWall };

// finds overlaps after movement and resolves them:
// - dynamic vs static: push the entity out of tiles and set its contact flags
// - player vs star: collect the star
// - star vs star: bounce them apart
template <typename... Components>
class CollisionSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("CollisionSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();

        // get the sound manager from the engine each frame (only used during this Run)
        mSounds = &engine->GetSoundManager();

        // split collidable objects into static and dynamic
        std::vector<EntityID> statics, dynamics;
        for (auto& entity : em) {
            EntityID id = entity.GetID();

            // must have a collision box and a position
            if (!em.template HasComponent<CollisionComponent>(id) ||
                !em.template HasComponent<LocationComponent>(id))
                continue;

            // the spawner tile is skipped, so stars spawned inside it are not pushed out
            if (em.template HasComponent<SpawnComponent>(id)) continue;

            auto& c = em.template GetComponent<CollisionComponent>(id);

            // a dynamic entity without a velocity can't move, so it can't collide
            if (!c.staticEntity && !em.template HasComponent<MovementComponent>(id)) continue;

            // clear last frame's contact flags, they get set again below
            c.onGround = c.onCeiling = c.onLeftWall = c.onRightWall = 0;

            // tiles go in statics, the player and stars go in dynamics
            if (c.staticEntity)
                statics.push_back(id);
            else
                dynamics.push_back(id);
        }

        // dynamic vs static
        for (EntityID dynId : dynamics) {
            // find every tile this entity overlaps and how much
            Rect dynBox = WorldBox(em, dynId);
            std::vector<std::pair<float, EntityID>> hits;
            for (EntityID staticId : statics) {
                float area = OverlapArea(dynBox, WorldBox(em, staticId));
                if (area > 0.f) hits.emplace_back(area, staticId);
            }

            // sort so the biggest overlap is handled first
            // pushing out of the main tile usually also clears small overlaps with the
            // tile next to it, which stops the player catching on seams between tiles
            std::sort(hits.begin(), hits.end(),
                      [](const auto& l, const auto& r) { return l.first > r.first; });

            for (const auto& hit : hits) {
                EntityID staticId = hit.second;

                // an earlier push may have already moved us out of this tile
                if (OverlapArea(WorldBox(em, dynId), WorldBox(em, staticId)) <= 0.f) continue;

                // true means the player was respawned, so ignore the remaining tiles
                if (ResolveStaticDynamic(em, staticId, dynId)) break;
            }
        }

        // dynamic vs dynamic
        // check every pair once (j starts after i)
        for (size_t i = 0; i < dynamics.size(); ++i) {
            for (size_t j = i + 1; j < dynamics.size(); ++j) {
                EntityID idA = dynamics[i], idB = dynamics[j];
                // a star may already have been collected earlier this frame
                if (!em.template HasComponent<CollisionComponent>(idA) ||
                    !em.template HasComponent<CollisionComponent>(idB))
                    continue;
                if (OverlapArea(WorldBox(em, idA), WorldBox(em, idB)) <= 0.f) continue;

                // the player is the only dynamic entity with an InputComponent
                bool aIsPlayer = em.template HasComponent<InputComponent>(idA);
                bool bIsPlayer = em.template HasComponent<InputComponent>(idB);

                if (aIsPlayer != bIsPlayer) {
                    // exactly one is the player, so the other one is a star
                    // A is the player -> collect B, otherwise -> collect A
                    CollectStar(em, aIsPlayer ? idB : idA);
                } else if (!aIsPlayer) {
                    // neither is the player: two stars hit each other
                    ResolveDynamicDynamic(em, idA, idB);
                }
            }
        }
        return kContinue;
    }

private:
    // collision box in world coordinates (collisionBox is relative to the entity location)
    template <typename EM>
    static Rect WorldBox(EM& em, EntityID id) {
        Rect box = em.template GetComponent<CollisionComponent>(id).collisionBox;
        const auto& loc = em.template GetComponent<LocationComponent>(id).loc;
        box.topLeft.x += loc.x;
        box.topLeft.y += loc.y;
        return box;
    }

    // area of intersection between two rectangles
    static float OverlapArea(const Rect& a, const Rect& b) {
        // width of the overlap = leftmost right edge - rightmost left edge
        float w = std::min(a.topLeft.x + a.width, b.topLeft.x + b.width) -
                  std::max(a.topLeft.x, b.topLeft.x);
        // height of the overlap = highest bottom edge - lowest top edge
        float h = std::min(a.topLeft.y + a.height, b.topLeft.y + b.height) -
                  std::max(a.topLeft.y, b.topLeft.y);
        // a negative or zero width/height means they don't overlap -> area 0
        return (w > 0.f && h > 0.f) ? w * h : 0.f;
    }

    // uses where the entity was at the start of this frame to decide which side it hit
    // prev = entity box before moving, s = the static tile box
    static ContactSide SideFromPrevious(const Rect& prev, const Rect& s) {
        // was the entity completely on one side of the tile before it moved?
        bool wasAbove = prev.topLeft.y + prev.height <= s.topLeft.y + kSideEpsilon;
        bool wasBelow = prev.topLeft.y >= s.topLeft.y + s.height - kSideEpsilon;
        bool wasLeftOf = prev.topLeft.x + prev.width <= s.topLeft.x + kSideEpsilon;
        bool wasRightOf = prev.topLeft.x >= s.topLeft.x + s.width - kSideEpsilon;

        // separated vertically and/or horizontally before moving
        bool vertical = wasAbove || wasBelow;
        bool horizontal = wasLeftOf || wasRightOf;

        // only vertically separated -> the entity came from above (landed) or below (hit ceiling)
        if (vertical && !horizontal) return wasAbove ? ContactSide::kGround : ContactSide::kCeiling;

        // only horizontally separated -> came from the left (tile is on its right)
        // or from the right (tile is on its left)
        if (horizontal && !vertical)
            return wasLeftOf ? ContactSide::kRightWall : ContactSide::kLeftWall;

        // separated on both axes (came in at a corner) or neither (already overlapping)
        // -> can't tell, let the caller decide using the smallest overlap
        return ContactSide::kNone;
    }

    // finds the entity holding the camera, or kNullComponent if there is none
    template <typename EM>
    static EntityID FindCameraEntity(EM& em) {
        for (auto& entity : em) {
            if (em.template HasComponent<CameraComponent>(entity.GetID())) return entity.GetID();
        }
        return EM::kNullComponent;
    }

    // finds the entity holding the score, or kNullComponent if there is none
    template <typename EM>
    static EntityID FindScoreEntity(EM& em) {
        for (auto& entity : em) {
            if (em.template HasComponent<ScoreComponent>(entity.GetID())) return entity.GetID();
        }
        return EM::kNullComponent;
    }

    // pushes a moving entity out of a static tile
    // will return true if the entity was respawned
    template <typename EM>
    bool ResolveStaticDynamic(EM& em, EntityID staticId, EntityID dynamicId) {
        const auto& stat = em.template GetComponent<CollisionComponent>(staticId);
        auto& dyn = em.template GetComponent<CollisionComponent>(dynamicId);
        auto& dynLoc = em.template GetComponent<LocationComponent>(dynamicId);
        auto& vel = em.template GetComponent<MovementComponent>(dynamicId).velocity;
        bool isPlayer = em.template HasComponent<InputComponent>(dynamicId);

        // damaging tile (spikes) and the entity can respawn -> send it back to the start
        if (stat.doesDamage && em.template HasComponent<RespawnComponent>(dynamicId)) {
            dynLoc.loc = em.template GetComponent<RespawnComponent>(dynamicId).loc;
            vel = Point2D(0.f, 0.f);

            // cancel any dash or wall jump that was in progress
            if (isPlayer) {
                auto& input = em.template GetComponent<InputComponent>(dynamicId);
                input.dashFramesLeft = 0;
                input.wallJumpLock = 0;
            }

            // turn gravity back on in case we died mid-dash
            if (em.template HasComponent<GravityComponent>(dynamicId))
                em.template GetComponent<GravityComponent>(dynamicId).disabledTimer = 0;
            return true;
        }

        // world boxes for the tile (s) and the moving entity (d)
        Rect s = WorldBox(em, staticId);
        Rect d = WorldBox(em, dynamicId);

        // how far the entity would have to move in each direction to get out
        float overlapLeft = (s.topLeft.x + s.width) - d.topLeft.x;    // push right by this
        float overlapRight = (d.topLeft.x + d.width) - s.topLeft.x;   // push left by this
        float overlapTop = (s.topLeft.y + s.height) - d.topLeft.y;    // push down by this
        float overlapBottom = (d.topLeft.y + d.height) - s.topLeft.y; // push up by this

        // first try to tell the side from where the entity was before it moved
        ContactSide side = SideFromPrevious(dyn.previousCollisionBox, s);

        // if that was unclear, use the smallest push needed to get out
        if (side == ContactSide::kNone) {
            float m = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});
            if (m == overlapBottom)
                side = ContactSide::kGround;
            else if (m == overlapTop)
                side = ContactSide::kCeiling;
            else if (m == overlapRight)
                side = ContactSide::kRightWall;
            else
                side = ContactSide::kLeftWall;
        }

        // speed along the axis that was hit (used for the screen shake),
        // and which axis the shake should move along
        float impactSpeed = 0.f;
        Point2D shakeDir(0.f, 0.f);

        switch (side) {
            case ContactSide::kGround:
                impactSpeed = std::abs(vel.y);

                // only real landings play the sound, not standing still (gravity is ~1.6/frame)
                if (isPlayer && vel.y > kLandSoundSpeed) mSounds->GetSound("land").play();

                // push up onto the top of the tile
                dynLoc.loc.y -= overlapBottom;

                // player -> stop falling (keep any upward speed, remove downward speed)
                // star -> bounce upward (force negative y) and lose some speed
                vel.y = isPlayer ? std::min(vel.y, 0.f) : -std::abs(vel.y) * kWallRestitution;
                dyn.onGround = 1;
                shakeDir = Point2D(0.f, 1.f);
                break;
            case ContactSide::kCeiling:
                impactSpeed = std::abs(vel.y);

                // push down below the tile
                dynLoc.loc.y += overlapTop;

                // player -> stop moving up (keep any downward speed)
                // star -> bounce downward (force positive y) and lose some speed
                vel.y = isPlayer ? std::max(vel.y, 0.f) : std::abs(vel.y) * kWallRestitution;
                dyn.onCeiling = 1;
                shakeDir = Point2D(0.f, 1.f);
                break;
            case ContactSide::kRightWall:
                impactSpeed = std::abs(vel.x);

                // push left, out of the wall on our right
                dynLoc.loc.x -= overlapRight;

                // player -> stop moving right (keep any leftward speed)
                // star -> bounce left (force negative x) and lose some speed
                vel.x = isPlayer ? std::min(vel.x, 0.f) : -std::abs(vel.x) * kWallRestitution;
                dyn.onRightWall = 1;
                shakeDir = Point2D(1.f, 0.f);
                break;
            case ContactSide::kLeftWall:
                impactSpeed = std::abs(vel.x);

                // push right, out of the wall on our left
                dynLoc.loc.x += overlapLeft;

                // player -> stop moving left (keep any rightward speed)
                // star -> bounce right (force positive x) and lose some speed
                vel.x = isPlayer ? std::max(vel.x, 0.f) : std::abs(vel.x) * kWallRestitution;
                dyn.onLeftWall = 1;
                shakeDir = Point2D(1.f, 0.f);
                break;
            case ContactSide::kNone:
                break;
        }

        // shake the camera when the player hits something hard enough
        if (isPlayer && impactSpeed > kShakeImpactThreshold) {
            EntityID cameraEntity = FindCameraEntity(em);
            if (cameraEntity != EM::kNullComponent) {
                // faster hits shake more, but never more than kMaxShakeMagnitude
                float mag = std::min(impactSpeed * kShakeMagnitudeScale, kMaxShakeMagnitude);

                // shake along the axis of the hit (CameraSystem plays and fades it out)
                em.template AddComponent<CameraShake>(
                    cameraEntity,
                    CameraShake{kShakeFrameLimit, Point2D(shakeDir.x * mag, shakeDir.y * mag)});
            }
        }
        return false;
    }

    // player touched a star: add a point, play the sound and remove the star
    template <typename EM>
    void CollectStar(EM& em, EntityID starId) {
        EntityID scoreEntity = FindScoreEntity(em);
        if (scoreEntity != EM::kNullComponent)
            em.template GetComponent<ScoreComponent>(scoreEntity).score++;
        mSounds->GetSound("collect").play();
        em.RemoveEntity(starId);
    }

    // two stars hit: push them apart and bounce them off each other
    template <typename EM>
    void ResolveDynamicDynamic(EM& em, EntityID idA, EntityID idB) {
        Rect a = WorldBox(em, idA), b = WorldBox(em, idB);
        auto& locA = em.template GetComponent<LocationComponent>(idA).loc;
        auto& locB = em.template GetComponent<LocationComponent>(idB).loc;
        auto& velA = em.template GetComponent<MovementComponent>(idA).velocity;
        auto& velB = em.template GetComponent<MovementComponent>(idB).velocity;

        // how much the two boxes overlap on each axis
        float overlapX = std::min(a.topLeft.x + a.width, b.topLeft.x + b.width) -
                         std::max(a.topLeft.x, b.topLeft.x);
        float overlapY = std::min(a.topLeft.y + a.height, b.topLeft.y + b.height) -
                         std::max(a.topLeft.y, b.topLeft.y);

        // separate along whichever axis has the smaller overlap (the side they hit on)
        if (overlapX < overlapY) {
            // compare centers to see which star is on the left
            bool aIsLeft = a.topLeft.x + a.width / 2.f < b.topLeft.x + b.width / 2.f;

            // move each star half the overlap away from the other
            // A on the left -> A moves left (negative), B moves right
            // A on the right -> A moves right (positive), B moves left
            float push = (aIsLeft ? -overlapX : overlapX) / 2.f;
            locA.x += push;
            locB.x -= push;

            // only bounce if they are moving toward each other
            // A on the left -> approaching if A moves right faster than B
            // A on the right -> approaching if A moves left faster than B
            bool approaching = aIsLeft ? velA.x > velB.x : velA.x < velB.x;
            if (approaching) {
                // equal mass bounce: swap speeds on this axis, then lose some speed
                std::swap(velA.x, velB.x);
                velA.x *= kStarRestitution;
                velB.x *= kStarRestitution;
            }
        } else {
            // compare centers to see which star is on top
            bool aIsAbove = a.topLeft.y + a.height / 2.f < b.topLeft.y + b.height / 2.f;

            // A on top -> A moves up (negative), B moves down
            // A below -> A moves down (positive), B moves up
            float push = (aIsAbove ? -overlapY : overlapY) / 2.f;
            locA.y += push;
            locB.y -= push;

            // A on top -> approaching if A moves down faster than B
            // A below -> approaching if A moves up faster than B
            bool approaching = aIsAbove ? velA.y > velB.y : velA.y < velB.y;
            if (approaching) {
                // equal mass bounce: swap speeds on this axis, then lose some speed
                std::swap(velA.y, velB.y);
                velA.y *= kStarRestitution;
                velB.y *= kStarRestitution;
            }
        }
    }

    // sound manager for this frame, set from the engine at the start of every Run
    SoundManager* mSounds = nullptr;
};

}  // namespace ECSEngine
