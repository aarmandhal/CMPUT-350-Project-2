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

        for (auto& entity : em) {
            EntityID id = entity.GetID();
            if (!em.template HasComponent<CollisionComponent>(id) ||
                !em.template HasComponent<LocationComponent>(id))
                continue;
            if (em.template HasComponent<SpawnComponent>(id)) continue;
            auto& c = em.template GetComponent<CollisionComponent>(id);
            if (!c.staticEntity && !em.template HasComponent<MovementComponent>(id)) continue;
            c.onGround = c.onCeiling = c.onLeftWall = c.onRightWall = 0;
            collidables.push_back(id);
        }

        for (size_t i = 0; i < collidables.size(); ++i) {
            for (size_t j = i + 1; j < collidables.size(); ++j) {
                EntityID idA = collidables[i], idB = collidables[j];
                if (!em.template HasComponent<CollisionComponent>(idA) ||
                    !em.template HasComponent<CollisionComponent>(idB))
                    continue;
                auto& a = em.template GetComponent<CollisionComponent>(idA);
                auto& b = em.template GetComponent<CollisionComponent>(idB);
                if (a.staticEntity && b.staticEntity) continue;
                if (!Overlaps(WorldBox(em, idA), WorldBox(em, idB))) continue;

                if (a.staticEntity != b.staticEntity) {
                    EntityID staticId = a.staticEntity ? idA : idB;
                    EntityID dynamicId = a.staticEntity ? idB : idA;
                    ResolveStaticDynamic(em, staticId, dynamicId);
                } else {
                    bool aIsPlayer = em.template HasComponent<InputComponent>(idA);
                    bool bIsPlayer = em.template HasComponent<InputComponent>(idB);
                    if (aIsPlayer != bIsPlayer) {
                        CollectStar(em, aIsPlayer ? idB : idA);
                    } else {
                        ResolveDynamicDynamic(em, idA, idB);
                    }
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

    static bool Overlaps(const Rect& a, const Rect& b) {
        return a.topLeft.x < b.topLeft.x + b.width && a.topLeft.x + a.width > b.topLeft.x &&
               a.topLeft.y < b.topLeft.y + b.height && a.topLeft.y + a.height > b.topLeft.y;
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

    template <typename EM>
    void ResolveStaticDynamic(EM& em, EntityID staticId, EntityID dynamicId) {
        const auto& stat = em.template GetComponent<CollisionComponent>(staticId);
        auto& dyn = em.template GetComponent<CollisionComponent>(dynamicId);
        auto& dynLoc = em.template GetComponent<LocationComponent>(dynamicId);
        auto& dynMove = em.template GetComponent<MovementComponent>(dynamicId);

        if (stat.doesDamage && em.template HasComponent<RespawnComponent>(dynamicId)) {
            dynLoc.loc = em.template GetComponent<RespawnComponent>(dynamicId).loc;
            dynMove.velocity = Point2D(0.f, 0.f);
            return;
        }

        Rect staticBox = WorldBox(em, staticId);
        Rect dynBox = WorldBox(em, dynamicId);

        float overlapLeft = (staticBox.topLeft.x + staticBox.width) - dynBox.topLeft.x;
        float overlapRight = (dynBox.topLeft.x + dynBox.width) - staticBox.topLeft.x;
        float overlapTop = (staticBox.topLeft.y + staticBox.height) - dynBox.topLeft.y;
        float overlapBottom = (dynBox.topLeft.y + dynBox.height) - staticBox.topLeft.y;
        float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});
        float impactSpeed = std::max(std::abs(dynMove.velocity.x), std::abs(dynMove.velocity.y));

        if (minOverlap == overlapBottom) {
            dynLoc.loc.y -= overlapBottom;
            dynMove.velocity.y = 0;
            dyn.onGround = 1;
        } else if (minOverlap == overlapTop) {
            dynLoc.loc.y += overlapTop;
            dynMove.velocity.y = 0;
            dyn.onCeiling = 1;
        } else if (minOverlap == overlapRight) {
            dynLoc.loc.x -= overlapRight;
            dynMove.velocity.x = 0;
            dyn.onRightWall = 1;
        } else {
            dynLoc.loc.x += overlapLeft;
            dynMove.velocity.x = 0;
            dyn.onLeftWall = 1;
        }

        if (impactSpeed > kShakeImpactThreshold &&
            em.template HasComponent<InputComponent>(dynamicId)) {
            EntityID cameraEntity = FindCameraEntity(em);
            if (cameraEntity != EM::kNullComponent) {
                float magnitude = impactSpeed * kShakeMagnitudeScale;
                em.template AddComponent<CameraShake>(
                    cameraEntity, CameraShake{kShakeFrameLimit, Point2D(magnitude, magnitude)});
            }
        }
    }

    template <typename EM>
    void CollectStar(EM& em, EntityID starId) {
        EntityID scoreEntity = FindScoreEntity(em);
        if (scoreEntity != EM::kNullComponent)
            em.template GetComponent<ScoreComponent>(scoreEntity).score++;
        em.RemoveEntity(starId);
    }

    template <typename EM>
    void ResolveDynamicDynamic(EM& em, EntityID idA, EntityID idB) {
        auto& moveA = em.template GetComponent<MovementComponent>(idA);
        auto& moveB = em.template GetComponent<MovementComponent>(idB);
        std::swap(moveA.velocity, moveB.velocity);
        moveA.velocity.x *= kStarRestitution;
        moveA.velocity.y *= kStarRestitution;
        moveB.velocity.x *= kStarRestitution;
        moveB.velocity.y *= kStarRestitution;
    }
};

}  // namespace ECSEngine
