#pragma once
#include "SceneNode.hpp"
#include "Aircraft.hpp"

class World 
{
public:
	explicit							World(Game* window);
	void								update(const GameTimer& gt);
	void								draw();

	//void								loadTextures();
	void								buildScene();


private:
	enum Layer
	{
		Background,
		Air,
		LayerCount
	};


private:
	Game*								mGame;

	std::unique_ptr<SceneNode> mSceneGraph;
	std::array<SceneNode*, LayerCount>	mSceneLayers;

	float								mScrollSpeed;
	Aircraft*							mPlayerAircraft;
	Aircraft*							mEnemy;
};
