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

template <typename... Components>
class EntityManager {
public:
    class iterator {
    public:
        iterator(std::vector<Entity>* entities, std::vector<bool>* valid, size_t index)
            : mEntities(entities), mValid(valid), mIndex(index) {
            SkipInvalid();
        }
        iterator& operator++() {
            ++mIndex;
            SkipInvalid();
            return *this;
        }

        Entity& operator*() const { return (*mEntities)[mIndex]; }
        bool operator!=(const iterator& other) const { return mIndex != other.mIndex; }

    private:
        void SkipInvalid() {
            while (mIndex < mValid->size() && !(*mValid)[mIndex]) ++mIndex;
        }

        std::vector<Entity>* mEntities;
        std::vector<bool>* mValid;
        size_t mIndex;
    };

    iterator begin() { return iterator(&mEntities, &mValid, 0); }
    iterator end() { return iterator(&mEntities, &mValid, mEntities.size()); }

    static constexpr size_t kNullComponent = std::numeric_limits<size_t>::max();
    [[nodiscard]] EntityID CreateEntity(const std::string& name) {
        EntityID id;
        if (!mFreeList.empty()) {
            id = mFreeList.back();
            mFreeList.pop_back();
            mEntities[id] = Entity(id, name);
        } else {
            id = mEntities.size();
            mEntities.push_back(Entity(id, name));
            mValid.push_back(false);
        }
        mValid[id] = true;
        return id;
    }

    void RemoveEntity(EntityID entity) {
        if (entity >= mValid.size() || !mValid[entity]) {
            return;
        }

        mValid[entity] = false;
        mFreeList.push_back(entity);
        (InvalidateIfPresent<Components>(entity), ...);
    }

    template <typename T>
    void InvalidateIfPresent(EntityID entity) {
        auto& storage = GetComponentStorage<T>();
        if (entity < storage.mValid.size()) storage.mValid[entity] = false;
    }

    template <typename T>
    void AddComponent(EntityID entity, T component) {
        auto& storage = GetComponentStorage<T>();
        if (entity >= storage.mStorage.size()) {
            storage.mStorage.resize(entity + 1, component);
            storage.mValid.resize(entity + 1, false);
        }
        storage.mStorage[entity] = component;
        storage.mValid[entity] = true;
    }

    template <typename T>
    void RemoveComponent(EntityID entity) {
        auto& storage = GetComponentStorage<T>();
        if (entity < storage.mValid.size()) {
            storage.mValid[entity] = false;
        }
    }

    template <typename T>
    bool HasComponent(EntityID entity) const {
        const auto& storage = GetComponentStorage<T>();
        return entity < storage.mValid.size() && storage.mValid[entity];
    }

    template <typename T>
    T& GetComponent(EntityID entity) {
        assert(HasComponent<T>(entity));
        return GetComponentStorage<T>().mStorage[entity];
    }

    template <typename T>
    ComponentStorage<T>& GetComponentStorage() {
        return std::get<ComponentStorage<T>>(mComponentStorages);
    }

    template <typename T>
    const ComponentStorage<T>& GetComponentStorage() const {
        return std::get<ComponentStorage<T>>(mComponentStorages);
    }

private:
    std::vector<Entity> mEntities;
    std::vector<bool> mValid;
    std::vector<EntityID> mFreeList;
    std::tuple<ComponentStorage<Components>...> mComponentStorages;
};

}  // namespace ECSEngine
