#include "World.hpp"
#include "Game.hpp"

World::World(Game* game)
	: mGame(game)
	, mSceneGraph(nullptr)
	, mSceneLayers()
	, mPlayerAircraft(nullptr)
{
}

void World::update(const GameTimer& gt)
{
	mSceneGraph->update(gt);
}

void World::draw()
{
	mSceneGraph->draw();
}

void World::buildScene()
{
	// Create scene graph root
	mSceneGraph = std::make_unique<SceneNode>(mGame);

	// Create player aircraft
	std::unique_ptr<Aircraft> player(new Aircraft(Aircraft::Eagle, mGame));
	mPlayerAircraft = player.get();
	mPlayerAircraft->setPosition(0, 0, 0);
	mPlayerAircraft->setScale(1.0, 1.0, 1.0);
	mSceneGraph->attachChild(std::move(player));

	// Create enemy aircraft 1
	std::unique_ptr<Aircraft> enemy1(new Aircraft(Aircraft::Raptor, mGame));
	enemy1->setPosition(-5.0, 0, 10);
	enemy1->setScale(1.0, 1.0, 1.0);
	enemy1->setWorldRotation(0, XM_PI, 0);
	mSceneGraph->attachChild(std::move(enemy1));

	// Create enemy aircraft 2
	std::unique_ptr<Aircraft> enemy2(new Aircraft(Aircraft::Raptor, mGame));
	enemy2->setPosition(5.0, 0, 10);
	enemy2->setScale(1.0, 1.0, 1.0);
	enemy2->setWorldRotation(0, XM_PI, 0);
	mSceneGraph->attachChild(std::move(enemy2));

	// Build render items
	mSceneGraph->build();
}