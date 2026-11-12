#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#include "CameraSystem.h"
#include "CollisionComponent.h"
#include "CollisionLateSystem.h"
#include "CollisionSystem.h"
#include "ECSEngine.h"
#include "EntityManager.h"
#include "GravitySystem.h"
#include "InputComponent.h"
#include "InputSystem.h"
#include "LocationComponent.h"
#include "MathUtil.h"
#include "MovementSystem.h"
#include "ScoreComponent.h"
#include "ScoreSystem.h"
#include "SpawnComponent.h"
#include "SpawnSystem.h"
#include "SpriteComponent.h"
#include "SpriteManager.h"
#include "SpriteSystem.h"

std::string gResourcePath = "../assets/";
#define COMPONENTS                                                                             \
    ECSEngine::LocationComponent, ECSEngine::MovementComponent, ECSEngine::CollisionComponent, \
        ECSEngine::SpriteComponent, ECSEngine::SpawnComponent, ECSEngine::CameraComponent,     \
        ECSEngine::CameraFollower, ECSEngine::InputComponent, ECSEngine::GravityComponent,     \
        ECSEngine::CameraShake, ECSEngine::ScoreComponent, ECSEngine::RespawnComponent
using Engine = ECSEngine::ECSEngine<COMPONENTS>;

void LoadMap(Engine& engine, const std::string& path, const std::string& file) {
    std::ifstream mapFile(path + file);

    if (!mapFile.is_open()) {
        throw std::runtime_error("Failed to open map: " + path + file);
    }

    auto& entities = engine.GetEntityManager();
    auto& sprites = engine.GetSpriteManager();

    // store the sprite and collision data for each tile type
    struct TileInfo {
        ECSEngine::SpriteID spriteID;
        ECSEngine::Rect collisionBounds;
        bool hasCollision;
    };

    std::unordered_map<char, TileInfo> dictionary;

    std::string line;
    std::string word;
    int dictionaryType;

    std::getline(mapFile, line);
    std::istringstream firstLine(line);

    // read the first line of the map file to determine the dictionary type
    firstLine >> word >> dictionaryType;

    // only the two supported map formats are valid
    if (word != "dictionary" || (dictionaryType != 1 && dictionaryType != 2)) {
        throw std::runtime_error("Invalid map dictionary");
    }

    // read tile dictionary until the map header is reached
    while (std::getline(mapFile, line)) {
        if (line.empty()) {
            continue;
        }

        // map header marks the end of the tile dictionary
        if (line.find("map origin") == 0) {
            break;
        }

        // parse title dictionary line
        std::istringstream tileLine(line);

        char symbol;
        std::string textureFile;
        float x, y, width, height;

        // read tile symbol, texture file and source rectangle from dictionay line
        tileLine >> symbol >> textureFile >> x >> y >> width >> height;

        ECSEngine::Rect sourceRect = {{x, y}, width, height};

        // register tile texture with the sprite manager and get the ID
        auto spriteID = sprites.RegisterTexture(path + textureFile, sourceRect);

        TileInfo tile{};
        tile.spriteID = spriteID;
        tile.hasCollision = (dictionaryType == 2);

        // only gameplay tiles include collision bounds
        if (tile.hasCollision) {
            float boxX, boxY, boxWidth, boxHeight;

            tileLine >> boxX >> boxY >> boxWidth >> boxHeight;

            tile.collisionBounds = {{boxX, boxY}, boxWidth, boxHeight};
        }

        dictionary[symbol] = tile;
    }

    std::istringstream mapSettings(line);

    std::string mapWord, originWord, tileWord, sizeWord;
    float originX, originY, tileWidth, tileHeight;
    int gridWidth, gridHeight;

    // read map settings from the map header line
    mapSettings >> mapWord >> originWord >> originX >> originY >> tileWord >> tileWidth >>
        tileHeight >> sizeWord >> gridWidth >> gridHeight;

    // validate map settings
    // check whether the map settings from world and sky are valid before creating the map entities
    if (mapWord != "map" || originWord != "origin" || tileWord != "tile" || sizeWord != "size" ||
        tileWidth <= 0 || tileHeight <= 0 || gridWidth <= 0 || gridHeight <= 0) {
        throw std::runtime_error("Invalid map settings");
    }

    // loop responsible for converting the map file into entities with components
    for (int row = 0; row < gridHeight; row++) {
        // read each row of the map and check its length
        if (!std::getline(mapFile, line) || line.size() != static_cast<size_t>(gridWidth)) {
            throw std::runtime_error("Invalid map row");
        }

        // loop through each column of the map
        // processes each char in the current row
        for (int col = 0; col < gridWidth; col++) {
            char symbol = line[col];

            // empty tiles do not need entities
            if (symbol == '.') {
                continue;
            }

            // find the tile info for the current symbol
            auto tile = dictionary.find(symbol);
            if (tile == dictionary.end()) {
                throw std::runtime_error("Unknown map tile");
            }

            // convert grid coordinates to world coordinates
            float tileX = originX + col * tileWidth;
            float tileY = originY + row * tileHeight;

            // create an ECS for the tile
            ECSEngine::EntityID id = entities.CreateEntity(std::string(1, symbol));

            // give the tile a location component
            ECSEngine::LocationComponent location;
            location.loc = {tileX, tileY};
            entities.AddComponent(id, location);

            // give the tile a sprite component
            ECSEngine::SpriteComponent sprite;
            sprite.textureID = tile->second.spriteID;
            sprite.bounds = {{0, 0}, tileWidth, tileHeight};
            sprite.worldSpace = true;

            // add the sprite component to the entity
            entities.AddComponent(id, sprite);

            // background tiles have no collision components
            // add collision component to the entity only if the tile has collision bounds
            if (tile->second.hasCollision) {
                ECSEngine::CollisionComponent collision{};

                collision.staticEntity = true;
                collision.doesDamage = (symbol == 'x');
                collision.collisionBox = tile->second.collisionBounds;

                // initialize the previous collision box in world coordinates for collision
                // detection
                collision.previousCollisionBox = {{tileX + collision.collisionBox.topLeft.x,
                                                   tileY + collision.collisionBox.topLeft.y},
                                                  collision.collisionBox.width,
                                                  collision.collisionBox.height};

                entities.AddComponent(id, collision);
            }

            // spawner behavior is added if the tile symbole is S
            if (symbol == 'S') {
                ECSEngine::SpawnComponent spawn{};

                spawn.entityID = id;
                spawn.spawnType = "star";
                spawn.totalSpawns = 0;

                entities.AddComponent(id, spawn);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc >= 3 && argv[1] == std::string("-path")) {
        gResourcePath = argv[2];
    }
    std::cout << "Usage: " << argv[0] << " [-path resource_path]\n";
    std::cout << "Using resource path: " << gResourcePath << "\n";

    // Engine test code
    Engine engine(1024, 768, "Test Engine");

    // Load sounds

    // get sound manager
    auto& sounds = engine.GetSoundManager();

    // register sounds for player actions
    sounds.RegisterSound(gResourcePath + "sfx_jump.ogg", "jump");
    sounds.RegisterSound(gResourcePath + "footstep_snow_001.ogg", "wall");
    sounds.RegisterSound(gResourcePath + "footstep_grass_003.ogg", "land");
    sounds.RegisterSound(gResourcePath + "sfx_gem.ogg", "collect");

    // Load maps
    // get sprite and entity managers
    auto& sprites = engine.GetSpriteManager();
    auto& entities = engine.GetEntityManager();

    // register player, star and digit sprites
    ECSEngine::SpriteID playerSprite = sprites.RegisterTexture(
        gResourcePath + "spritesheet-characters-default.png", {{0, 0}, 128, 128});
    ECSEngine::SpriteID starSprite = sprites.RegisterTexture(
        gResourcePath + "spritesheet-tiles-default.png", {{640, 320}, 64, 64});
    ECSEngine::SpriteID digitSprites[10];

    // load the digit sprites
    for (int i = 0; i < 10; i++) {
        digitSprites[i] = sprites.RegisterTexture(gResourcePath + "spritesheet-tiles-default.png",
                                                  {{832.0f, 832.0f - i * 64.0f}, 64.0f, 64.0f});
    }

    // load background first
    LoadMap(engine, gResourcePath, "sky.map");

    // load gameplay world
    LoadMap(engine, gResourcePath, "world.map");

    // TODO: test spawner setup once EntityManager iteration is implemented by other group member
    // find the spawner created by the map
    for (const auto& entity : entities) {
        ECSEngine::EntityID id = entity.GetID();

        // only process entities with a spawn component
        if (!entities.HasComponent<ECSEngine::SpawnComponent>(id)) {
            continue;
        }

        // spawner settings
        auto& spawn = entities.GetComponent<ECSEngine::SpawnComponent>(id);

        // set star sprite and spawn timer
        spawn.spawnTextureID = starSprite;
        spawn.timeToNextSpawn = 120;
        spawn.timeBetweenSpawns = 120;
        spawn.totalSpawns = 10;
    }

    // Place player
    ECSEngine::EntityID playerID = entities.CreateEntity("player");

    // set players initial world position
    ECSEngine::LocationComponent playerLocation{};
    playerLocation.loc = {736.0f, 480.0f};
    entities.AddComponent(playerID, playerLocation);

    // set players sprite and bounds
    ECSEngine::SpriteComponent playerSpriteComponent{};
    playerSpriteComponent.textureID = playerSprite;
    playerSpriteComponent.bounds = {{-24, -48}, 96, 96};
    playerSpriteComponent.worldSpace = true;
    entities.AddComponent(playerID, playerSpriteComponent);

    // store players velocity so movement system can update the player
    ECSEngine::MovementComponent playerMovement{};
    playerMovement.velocity = {0, 0};
    entities.AddComponent(playerID, playerMovement);

    // apply downward gravity to the player
    ECSEngine::GravityComponent playerGravity{ECSEngine::Point2D(0.0f, 1.0f)};
    entities.AddComponent(playerID, playerGravity);

    ECSEngine::InputComponent playerInput{playerID};
    entities.AddComponent(playerID, playerInput);

    // collision bounds define players physical area in the world
    // players collision behavior
    ECSEngine::CollisionComponent playerCollision{};
    playerCollision.staticEntity = false;
    playerCollision.doesDamage = false;
    playerCollision.collisionBox = {{0, 0}, 48, 48};

    // previous bounds help detect collisions between movement updates
    playerCollision.previousCollisionBox = {playerLocation.loc, 48, 48};

    entities.AddComponent(playerID, playerCollision);

    // save starting location so player can respawn there
    ECSEngine::RespawnComponent playerRespawn{};
    playerRespawn.loc = playerLocation.loc;
    entities.AddComponent(playerID, playerRespawn);

    // Setup score component
    // store score separately from entities displaying its digits
    ECSEngine::EntityID scoreID = entities.CreateEntity("score");
    ECSEngine::ScoreComponent score{};
    score.score = 0;

    // give ScoreSystem access to the sprite for each number
    for (int i = 0; i < 10; i++) {
        score.digitTextureID[i] = digitSprites[i];
    }

    // use three separate entities so each digit can update independently
    for (int i = 0; i < 3; i++) {
        ECSEngine::EntityID digitID = entities.CreateEntity("digit");

        // offset each digit horizontally to prevent overlapping
        ECSEngine::LocationComponent digitLocation{};
        digitLocation.loc = {20.0f + i * 36.0f, 20.0f};
        entities.AddComponent(digitID, digitLocation);

        // all digits initially display zero
        ECSEngine::SpriteComponent digitSprite{};
        digitSprite.textureID = digitSprites[0];
        digitSprite.bounds = {{0, 0}, 32, 32};

        // screen space keeps score in place when camera moves
        digitSprite.worldSpace = false;
        entities.AddComponent(digitID, digitSprite);

        // ScoreSystem uses these ids to update the correct digit entities
        score.digitID[i] = digitID;
    }

    entities.AddComponent(scoreID, score);

    // Setup Camera
    // keep camera settings in a separate entity
    ECSEngine::EntityID cameraID = entities.CreateEntity("camera");
    ECSEngine::CameraComponent camera{};

    // initialize camera at the players starting position
    camera.position = playerLocation.loc;
    camera.worldUnitsPerPixel = 1.0f;

    // allow player to move within the center region before following
    camera.softExclusion = {{307.2f, 0.0f}, 409.6f, 768.0f};

    // keep player within the outer screen boundaries
    camera.hardExclusion = {{102.4f, 0.0f}, 819.2f, 768.0f};

    // smooth camera movement
    camera.speed = 0.1f;

    entities.AddComponent(cameraID, camera);

    // identify which entity the camera should follow
    ECSEngine::CameraFollower follower{};
    follower.entityID = playerID;
    entities.AddComponent(cameraID, follower);

    // disable camera shake until a gameplay event activates it
    ECSEngine::CameraShake shake{};
    shake.frameLimit = 0;
    shake.offset = {0, 0};
    entities.AddComponent(cameraID, shake);

    // synchronize the initial window view with the camera
    engine.GetWindowManager().SetWorldScale(camera.worldUnitsPerPixel);
    engine.GetWindowManager().SetWorldCenter(camera.position);

    // Place test star
    // separate star so wall jumping can be tested
    ECSEngine::EntityID starID = entities.CreateEntity("star");

    ECSEngine::LocationComponent starLocation{};

    // place the test star outside the first wall
    starLocation.loc = {576.0f, 256.0f};

    entities.AddComponent(starID, starLocation);

    // the star graphic registered earlier
    ECSEngine::SpriteComponent starSpriteComponent{};
    starSpriteComponent.textureID = starSprite;

    starSpriteComponent.bounds = {{0, 0}, 32, 32};
    starSpriteComponent.worldSpace = true;

    entities.AddComponent(starID, starSpriteComponent);

    // allow the player to collide with the test star
    ECSEngine::CollisionComponent starCollision{};
    starCollision.staticEntity = false;
    starCollision.doesDamage = false;

    // keep collision bounds consistent with the sprite
    starCollision.collisionBox = {{0, 0}, 32, 32};
    starCollision.previousCollisionBox = {starLocation.loc, 32, 32};

    entities.AddComponent(starID, starCollision);

    // allow the movement system to update the star
    ECSEngine::MovementComponent starMovement{};
    entities.AddComponent(starID, starMovement);

    // Add all systems
    auto& sm = engine.GetSystemManager();
    sm.AddSystem(std::make_unique<ECSEngine::CollisionLateSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::InputSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::GravitySystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::MovementSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::CollisionSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::ScoreSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::CameraSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::SpriteSystem<COMPONENTS>>());
    sm.AddSystem(std::make_unique<ECSEngine::SpawnSystem<COMPONENTS>>());

    engine.Run();
    return 0;
}
