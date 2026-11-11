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

const float kShakeImpactThreshold = 20.f;
const int kShakeFrameLimit = 10;
const float kShakeMagnitudeScale = 0.25f;
const float kStarRestitution = 0.6f;
const float kWallRestitution = 0.8f;
const float kLandSoundSpeed = 4.f;
const float kSideEpsilon = 0.01f;

enum class ContactSide { kNone, kGround, kCeiling, kLeftWall, kRightWall };

template <typename... Components>
class CollisionSystem : public System<Components...> {
public:
    const std::string& GetName() const override {
        static std::string name("CollisionSystem");
        return name;
    }

    tSystemReturn Run(EngineView<Components...>* engine) override {
        auto& em = engine->GetEntityManager();
        std::vector<EntityID> collidables;
        mSounds = &engine->GetSoundManager();

        // Split collidable objects into static and dynamic
        std::vector<EntityID> statics, dynamics;
        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (!em.template HasComponent<CollisionComponent>(id) ||
                !em.template HasComponent<LocationComponent>(id))
                continue;
            if (em.template HasComponent<SpawnComponent>(id)) continue;
            auto& c = em.template GetComponent<CollisionComponent>(id);
            if (!c.staticEntity && !em.template HasComponent<MovementComponent>(id)) continue;
            c.onGround = c.onCeiling = c.onLeftWall = c.onRightWall = 0;
            if (c.staticEntity)
                statics.push_back(id);
            else
                dynamics.push_back(id);
        }

        // Dynamic vs Static
        for (EntityID dynId : dynamics) {
            Rect dynBox = WorldBox(em, dynId);
            std::vector<std::pair<float, EntityID>> hits;
            for (EntityID staticId : statics) {
                float area = OverlapArea(dynBox, WorldBox(em, staticId));
                if (area > 0.f) hits.emplace_back(area, staticId);
            }
            std::sort(hits.begin(), hits.end(),
                      [](const auto& l, const auto& r) { return l.first > r.first; });
            for (const auto& hit : hits) {
                EntityID staticId = hit.second;
                if (OverlapArea(WorldBox(em, dynId), WorldBox(em, staticId)) <= 0.f) continue;
                if (ResolveStaticDynamic(em, staticId, dynId)) break;
            }
        }

        // Dynamic vs Dynamic
        for (size_t i = 0; i < dynamics.size(); ++i) {
            for (size_t j = i + 1; j < dynamics.size(); ++j) {
                EntityID idA = dynamics[i], idB = dynamics[j];
                // a star may already have been collected earlier this frame
                if (!em.template HasComponent<CollisionComponent>(idA) ||
                    !em.template HasComponent<CollisionComponent>(idB))
                    continue;
                if (OverlapArea(WorldBox(em, idA), WorldBox(em, idB)) <= 0.f) continue;

                bool aIsPlayer = em.template HasComponent<InputComponent>(idA);
                bool bIsPlayer = em.template HasComponent<InputComponent>(idB);
                if (aIsPlayer != bIsPlayer) {
                    CollectStar(em, aIsPlayer ? idB : idA);
                } else if (!aIsPlayer) {
                    ResolveDynamicDynamic(em, idA, idB);
                }
            }
        }
        return kContinue;
    }

private:
    template <typename EM>
    static Rect WorldBox(EM& em, EntityID id) {
        Rect box = em.template GetComponent<CollisionComponent>(id).collisionBox;
        const auto& loc = em.template GetComponent<LocationComponent>(id).loc;
        box.topLeft.x += loc.x;
        box.topLeft.y += loc.y;
        return box;
    }

    // Area of intersection between two rectangles
    static float OverlapArea(const Rect& a, const Rect& b) {
        float w = std::min(a.topLeft.x + a.width, b.topLeft.x + b.width) -
                  std::max(a.topLeft.x, b.topLeft.x);
        float h = std::min(a.topLeft.y + a.height, b.topLeft.y + b.height) -
                  std::max(a.topLeft.y, b.topLeft.y);
        return (w > 0.f && h > 0.f) ? w * h : 0.f;
    }

    static ContactSide SideFromPrevious(const Rect& prev, const Rect& s) {
        bool wasAbove = prev.topLeft.y + prev.height <= s.topLeft.y + kSideEpsilon;
        bool wasBelow = prev.topLeft.y >= s.topLeft.y + s.height - kSideEpsilon;
        bool wasLeftOf = prev.topLeft.x + prev.width <= s.topLeft.x + kSideEpsilon;
        bool wasRightOf = prev.topLeft.x >= s.topLeft.x + s.width - kSideEpsilon;
        bool vertical = wasAbove || wasBelow;
        bool horizontal = wasLeftOf || wasRightOf;
        if (vertical && !horizontal) return wasAbove ? ContactSide::kGround : ContactSide::kCeiling;
        if (horizontal && !vertical)
            return wasLeftOf ? ContactSide::kRightWall : ContactSide::kLeftWall;
        return ContactSide::kNone;
    }

    template <typename EM>
    static EntityID FindCameraEntity(EM& em) {
        for (auto& entity : em) {
            if (em.template HasComponent<CameraComponent>(entity.GetID())) return entity.GetID();
        }
        return EM::kNullComponent;
    }

    template <typename EM>
    static EntityID FindScoreEntity(EM& em) {
        for (auto& entity : em) {
            if (em.template HasComponent<ScoreComponent>(entity.GetID())) return entity.GetID();
        }
        return EM::kNullComponent;
    }

    // Will return true if the entity was respawned
    template <typename EM>
    bool ResolveStaticDynamic(EM& em, EntityID staticId, EntityID dynamicId) {
        const auto& stat = em.template GetComponent<CollisionComponent>(staticId);
        auto& dyn = em.template GetComponent<CollisionComponent>(dynamicId);
        auto& dynLoc = em.template GetComponent<LocationComponent>(dynamicId);
        auto& vel = em.template GetComponent<MovementComponent>(dynamicId).velocity;
        bool isPlayer = em.template HasComponent<InputComponent>(dynamicId);

        if (stat.doesDamage && em.template HasComponent<RespawnComponent>(dynamicId)) {
            dynLoc.loc = em.template GetComponent<RespawnComponent>(dynamicId).loc;
            vel = Point2D(0.f, 0.f);
            return true;
        }

        Rect s = WorldBox(em, staticId);
        Rect d = WorldBox(em, dynamicId);
        float overlapLeft = (s.topLeft.x + s.width) - d.topLeft.x;
        float overlapRight = (d.topLeft.x + d.width) - s.topLeft.x;
        float overlapTop = (s.topLeft.y + s.height) - d.topLeft.y;
        float overlapBottom = (d.topLeft.y + d.height) - s.topLeft.y;

        ContactSide side = SideFromPrevious(dyn.previousCollisionBox, s);
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

        float impactSpeed = 0.f;
        Point2D shakeDir(0.f, 0.f);
        switch (side) {
            case ContactSide::kGround:
                impactSpeed = std::abs(vel.y);
                if (isPlayer && vel.y > kLandSoundSpeed) mSounds->GetSound("land").play();
                dynLoc.loc.y -= overlapBottom;
                vel.y = isPlayer ? std::min(vel.y, 0.f) : -std::abs(vel.y) * kWallRestitution;
                dyn.onGround = 1;
                shakeDir = Point2D(0.f, 1.f);
                break;
            case ContactSide::kCeiling:
                impactSpeed = std::abs(vel.y);
                dynLoc.loc.y += overlapTop;
                vel.y = isPlayer ? std::max(vel.y, 0.f) : std::abs(vel.y) * kWallRestitution;
                dyn.onCeiling = 1;
                shakeDir = Point2D(0.f, 1.f);
                break;
            case ContactSide::kRightWall:
                impactSpeed = std::abs(vel.x);
                dynLoc.loc.x -= overlapRight;
                vel.x = isPlayer ? std::min(vel.x, 0.f) : -std::abs(vel.x) * kWallRestitution;
                dyn.onRightWall = 1;
                shakeDir = Point2D(1.f, 0.f);
                break;
            case ContactSide::kLeftWall:
                impactSpeed = std::abs(vel.x);
                dynLoc.loc.x += overlapLeft;
                vel.x = isPlayer ? std::max(vel.x, 0.f) : std::abs(vel.x) * kWallRestitution;
                dyn.onLeftWall = 1;
                shakeDir = Point2D(1.f, 0.f);
                break;
            case ContactSide::kNone:
                break;
        }

        if (isPlayer && impactSpeed > kShakeImpactThreshold) {
            EntityID cameraEntity = FindCameraEntity(em);
            if (cameraEntity != EM::kNullComponent) {
                float mag = impactSpeed * kShakeMagnitudeScale;
                em.template AddComponent<CameraShake>(
                    cameraEntity,
                    CameraShake{kShakeFrameLimit, Point2D(shakeDir.x * mag, shakeDir.y * mag)});
            }
        }
        return false;
    }

    template <typename EM>
    void CollectStar(EM& em, EntityID starId) {
        EntityID scoreEntity = FindScoreEntity(em);
        if (scoreEntity != EM::kNullComponent)
            em.template GetComponent<ScoreComponent>(scoreEntity).score++;
        mSounds->GetSound("collect").play();
        em.RemoveEntity(starId);
    }

    template <typename EM>
    void ResolveDynamicDynamic(EM& em, EntityID idA, EntityID idB) {
        Rect a = WorldBox(em, idA), b = WorldBox(em, idB);
        auto& locA = em.template GetComponent<LocationComponent>(idA).loc;
        auto& locB = em.template GetComponent<LocationComponent>(idB).loc;
        auto& velA = em.template GetComponent<MovementComponent>(idA).velocity;
        auto& velB = em.template GetComponent<MovementComponent>(idB).velocity;

        float overlapX = std::min(a.topLeft.x + a.width, b.topLeft.x + b.width) -
                         std::max(a.topLeft.x, b.topLeft.x);
        float overlapY = std::min(a.topLeft.y + a.height, b.topLeft.y + b.height) -
                         std::max(a.topLeft.y, b.topLeft.y);

        if (overlapX < overlapY) {
            bool aIsLeft = a.topLeft.x + a.width / 2.f < b.topLeft.x + b.width / 2.f;
            float push = (aIsLeft ? -overlapX : overlapX) / 2.f;
            locA.x += push;
            locB.x -= push;
            bool approaching = aIsLeft ? velA.x > velB.x : velA.x < velB.x;
            if (approaching) {
                std::swap(velA.x, velB.x);
                velA.x *= kStarRestitution;
                velB.x *= kStarRestitution;
            }
        } else {
            bool aIsAbove = a.topLeft.y + a.height / 2.f < b.topLeft.y + b.height / 2.f;
            float push = (aIsAbove ? -overlapY : overlapY) / 2.f;
            locA.y += push;
            locB.y -= push;
            bool approaching = aIsAbove ? velA.y > velB.y : velA.y < velB.y;
            if (approaching) {
                std::swap(velA.y, velB.y);
                velA.y *= kStarRestitution;
                velB.y *= kStarRestitution;
            }
        }
    }

    SoundManager* mSounds = nullptr;
};

}  // namespace ECSEngine
