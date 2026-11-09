#pragma once

namespace ECSEngine {

const float kShakeImpactThreshold = 20.f;
const int kShakeFrameLimit = 10;
const float kStarRestitution = 0.6f;

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
            if (em.template HasComponent<CollisionComponent>(id) &&
                em.template HasComponent<LocationComponent>(id)) {
                collidables.push_back(id);
                auto& c = em.template GetComponent<CollisionComponent>(id);
                c.onGround = c.onCeiling = c.onLeftWall = c.onRightWall = 0;
            }
        }

        for (size_t i = 0; i < collidables.size(); ++i) {
            for (size_t j = i + 1; j < collidables.size(); ++j) {
                EntityID idA = collidables[i], idB = collidables[j];
                auto& a = em.template GetComponent<CollisionComponent>(idA);
                auto& b = em.template GetComponent<CollisionComponent>(idB);
                if (!Overlaps(a.collisionBox, b.collisionBox)) continue;
                if (a.staticEntity && b.staticEntity) continue;

                if (a.staticEntity != b.staticEntity) {
                    EntityID staticId = a.staticEntity ? idA : idB;
                    EntityID dynamicId = a.staticEntity ? idB : idA;
                    ResolveStaticDynamic(engine, em, staticId, dynamicId);
                } else {
                    ResolveDynamicDynamic(em, idA, idB);
                }
            }
        }
        return kContinue;
    }

private:
    bool Overlaps(const Rect& a, const Rect& b) {
        return a.topLeft.x < b.topLeft.x + b.width && a.topLeft.x + a.width > b.topLeft.x &&
               a.topLeft.y < b.topLeft.y + b.height && a.topLeft.y + a.height > b.topLeft.y;
    }

    template <typename Engine, typename EM>
    void ResolveStaticDynamic(Engine* engine, EM& em, EntityID staticId, EntityID dynamicId) {
        auto& staticBox = em.template GetComponent<CollisionComponent>(staticId).collisionBox;
        auto& dyn = em.template GetComponent<CollisionComponent>(dynamicId);
        auto& dynLoc = em.template GetComponent<LocationComponent>(dynamicId);
        auto& dynMove = em.template GetComponent<MovementComponent>(dynamicId);

        float overlapLeft = (staticBox.topLeft.x + staticBox.width) - dyn.collisionBox.topLeft.x;
        float overlapRight =
            (dyn.collisionBox.topLeft.x + dyn.collisionBox.width) - staticBox.topLeft.x;
        float overlapTop = (staticBox.topLeft.y + staticBox.height) - dyn.collisionBox.topLeft.y;
        float overlapBottom =
            (dyn.collisionBox.topLeft.y + dyn.collisionBox.height) - staticBox.topLeft.y;
        float minOverlap = std::min({overlapLeft, overlapRight, overlapTop, overlapBottom});
        float impactSpeed = std::max(std::abs(dynMove.velocity.x), std::abs(dynMove.velocity.y));

        if (minOverlap == overlapTop) {
            dynLoc.loc.y -= overlapTop;
            dynMove.velocity.y = 0;
            dyn.onGround = 1;
        } else if (minOverlap == overlapBottom) {
            dynLoc.loc.y += overlapBottom;
            dynMove.velocity.y = 0;
            dyn.onCeiling = 1;
        } else if (minOverlap == overlapLeft) {
            dynLoc.loc.x -= overlapLeft;
            dynMove.velocity.x = 0;
            dyn.onRightWall = 1;
        } else {
            dynLoc.loc.x += overlapRight;
            dynMove.velocity.x = 0;
            dyn.onLeftWall = 1;
        }

        if (impactSpeed > kShakeImpactThreshold) {
            EntityID cameraEntity = FindCameraEntity(em);  // see integration note below
            if (cameraEntity != EntityManagerType::kNullComponent)
                em.template AddComponent<CameraShake>(
                    cameraEntity, CameraShake{kShakeFrameLimit, /* magnitude from impactSpeed */});
        }
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
