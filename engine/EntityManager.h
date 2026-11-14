#pragma once

#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "Component.h"
#include "Entity.h"

namespace ECSEngine {

// owns all entities and their components
// - an EntityID is an index into the internal vectors; removed IDs are reused by CreateEntity
// - each component type has its own storage, indexed by EntityID
// - references from GetComponent<T> stay valid until AddComponent<T> is called for any
//   entity (the storage may grow and move) or until RemoveEntity is called on that entity
template <typename... Components>
class EntityManager {
public:
    // iterates over valid entities only, skipping slots left by removed entities
    class iterator {
    public:
        iterator(std::vector<Entity>* entities, std::vector<bool>* valid, size_t index)
            : mEntities(entities), mValid(valid), mIndex(index) {
            // the starting slot may be a removed entity, so move to the first valid one
            SkipInvalid();
        }

        // go to the next slot, then skip over any removed entities
        iterator& operator++() {
            ++mIndex;
            SkipInvalid();
            return *this;
        }

        Entity& operator*() const { return (*mEntities)[mIndex]; }

        // two iterators are equal when they point at the same slot
        bool operator!=(const iterator& other) const { return mIndex != other.mIndex; }

    private:
        // move forward until we reach a valid entity or the end of the vector
        void SkipInvalid() {
            while (mIndex < mValid->size() && !(*mValid)[mIndex]) ++mIndex;
        }

        std::vector<Entity>* mEntities;
        std::vector<bool>* mValid;
        size_t mIndex;
    };

    // allows for (auto& entity : entityManager)
    iterator begin() { return iterator(&mEntities, &mValid, 0); }
    iterator end() { return iterator(&mEntities, &mValid, mEntities.size()); }

    // returned when something can't be found (largest possible size_t)
    static constexpr size_t kNullComponent = std::numeric_limits<size_t>::max();

    // creates a new entity and returns its id
    [[nodiscard]] EntityID CreateEntity(const std::string& name) {
        EntityID id;
        if (!mFreeList.empty()) {
            // reuse the slot of a removed entity, so the vectors don't keep growing
            id = mFreeList.back();
            mFreeList.pop_back();
            mEntities[id] = Entity(id, name);
        } else {
            // no free slots: add a new one at the end
            id = mEntities.size();
            mEntities.push_back(Entity(id, name));
            mValid.push_back(false);
        }
        mValid[id] = true;
        return id;
    }

    // removes an entity and all of its components
    void RemoveEntity(EntityID entity) {
        // ignore ids that don't exist or were already removed
        if (entity >= mValid.size() || !mValid[entity]) {
            return;
        }

        // mark the slot as free so CreateEntity can reuse it
        mValid[entity] = false;
        mFreeList.push_back(entity);

        // fold expression: calls InvalidateIfPresent once for every component type
        (InvalidateIfPresent<Components>(entity), ...);
    }

    // marks this entity's T component as removed, if its storage is big enough to hold it
    template <typename T>
    void InvalidateIfPresent(EntityID entity) {
        auto& storage = GetComponentStorage<T>();
        if (entity < storage.mValid.size()) storage.mValid[entity] = false;
    }

    // adds (or replaces) a component of type T on an entity
    template <typename T>
    void AddComponent(EntityID entity, T component) {
        auto& storage = GetComponentStorage<T>();

        // grow the storage so this entity's index fits
        // new slots are filled with a copy of the component but marked invalid
        // (copying is needed because some components have no default constructor)
        if (entity >= storage.mStorage.size()) {
            storage.mStorage.resize(entity + 1, component);
            storage.mValid.resize(entity + 1, false);
        }
        storage.mStorage[entity] = component;
        storage.mValid[entity] = true;
    }

    // removes a component of type T from an entity (ignores ids outside the storage)
    template <typename T>
    void RemoveComponent(EntityID entity) {
        auto& storage = GetComponentStorage<T>();
        if (entity < storage.mValid.size()) {
            storage.mValid[entity] = false;
        }
    }

    // true if the entity currently has a component of type T
    template <typename T>
    bool HasComponent(EntityID entity) const {
        const auto& storage = GetComponentStorage<T>();
        // check the size first so we never read past the end of the vector
        return entity < storage.mValid.size() && storage.mValid[entity];
    }

    // returns the entity's T component
    // call HasComponent first: this only checks with assert (removed in release builds)
    template <typename T>
    T& GetComponent(EntityID entity) {
        assert(HasComponent<T>(entity));
        return GetComponentStorage<T>().mStorage[entity];
    }

    // returns the storage for component type T (looked up by type in the tuple)
    template <typename T>
    ComponentStorage<T>& GetComponentStorage() {
        return std::get<ComponentStorage<T>>(mComponentStorages);
    }

    // const version, used by HasComponent
    template <typename T>
    const ComponentStorage<T>& GetComponentStorage() const {
        return std::get<ComponentStorage<T>>(mComponentStorages);
    }

private:
    // every entity ever created, indexed by EntityID
    std::vector<Entity> mEntities;

    // true if the entity at that index is alive
    std::vector<bool> mValid;

    // ids of removed entities waiting to be reused
    std::vector<EntityID> mFreeList;

    // one ComponentStorage for every component type the engine was created with
    std::tuple<ComponentStorage<Components>...> mComponentStorages;
};

}  // namespace ECSEngine
