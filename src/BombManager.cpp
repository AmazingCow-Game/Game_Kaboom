//Header
#include "BombManager.h"
//std
#include <algorithm>

//Usings
USING_NS_GAMEKABOOM;


////////////////////////////////////////////////////////////////////////////////
// Constants                                                                  //
////////////////////////////////////////////////////////////////////////////////
const char * const kSoundName_Dropping   = "bomb.wav";
const char * const kSoundName_ExplodeFmt = "bomb_explode%d.wav";
const char * const kSoundName_Caught     = "bomb_caught.wav";

constexpr int kSoundExplode_Count = 1;


////////////////////////////////////////////////////////////////////////////////
// CTOR / DTOR                                                                //
////////////////////////////////////////////////////////////////////////////////
BombManager::BombManager()
{
    initSounds();
}

BombManager::~BombManager()
{

}



// Update / Draw //
void BombManager::update(float dt)
{
    for(auto &bomb : m_bombsVec)
        bomb->update(dt);
}

void BombManager::draw()
{
    for(auto &bomb : m_bombsVec)
        bomb->draw();
}


////////////////////////////////////////////////////////////////////////////////
// Actions                                                                    //
////////////////////////////////////////////////////////////////////////////////
void BombManager::reset(const TurnInfo &turnInfo)
{
    m_turnInfo         = turnInfo;
    m_aliveBombs       = 0;
    m_bombsCaught      = 0;
    m_isExplodingBombs = false;

    //Create the needed bombs for this level.
    for(int i = 0; i < m_bombsVec.size() - m_turnInfo.bombsCount; ++i)
        createBombHelper();

    //Reset them.
    resetAllBombs();
}

void BombManager::dropBombAt(const Lore::Vector2 &pos)
{
    playDroppingEffect();

    for(auto &bomb : m_bombsVec)
    {
        if(bomb->getState() == Bomb::State::Dead)
        {
            bomb->setPosition(pos);
            bomb->startDropping();
            m_aliveBombs++;

            break;
        }
    }
}

void BombManager::checkCollision(Paddle &paddle)
{
    if(m_isExplodingBombs)
        return;

    for(auto &bomb : m_bombsVec)
    {
        //Dead bomb.
        if(bomb->getState() != Bomb::State::Alive)
            continue;

        //Did not collide with paddle.
        if(!paddle.checkCollision(bomb->getHitBox()))
            continue;


        //Did collide with paddle.
        //Kill the bomb, update the housekeep vars
        //and inform the listeners
        bomb->kill();

        ++m_bombsCaught;
        --m_aliveBombs;

        m_onBombCaughtCallback();

        //Play Sound.
        playCaughtEffect();

        //Check if Player won this level and inform the listeners.
        if(m_bombsCaught == m_turnInfo.bombsCount)
        {
            stopDroppingEffect();
            m_onAllBombsCaughtCallback();
        }
    }
}


////////////////////////////////////////////////////////////////////////////////
// Setters                                                                    //
////////////////////////////////////////////////////////////////////////////////
//Movement
void BombManager::setBombMovementBounds(int maxY)
{
    m_maxBombY = maxY;
}

 //Callbacks
void BombManager::setOnBombCaughtCallback(const BombCaughtCallback &callback)
{
    m_onBombCaughtCallback = callback;
}

void BombManager::setOnAllBombsCaughtCallback(const AllBombsCaughtCallback &callback)
{
    m_onAllBombsCaughtCallback = callback;
}


void BombManager::setOnBombReachTargetCallback(const BombReachTargetCallback &callback)
{
    m_onBombReachTargetCallback = callback;
}

void BombManager::setOnBombExplodeCallback(const BombExplodeCallback &callback)
{
    m_onBombExplodeCallback = callback;
}

void BombManager::setOnAllBombsExplodedCallback(const AllBombsExplodedCallback &callback)
{
    m_onAllBombsExplodedCallback = callback;
}


////////////////////////////////////////////////////////////////////////////////
// Private Methods                                                            //
////////////////////////////////////////////////////////////////////////////////
void BombManager::createBombHelper()
{
    auto bomb = std::unique_ptr<Bomb>(new Bomb());
    bomb->setMovementBounds(m_maxBombY);

    auto explode = COREGAME_CALLBACK_0(BombManager::onBombExplodeFinished, this);
    auto reach   = COREGAME_CALLBACK_0(BombManager::onBombReachTarget,     this);

    bomb->setOnReachTargetCallback    (reach);
    bomb->setOnExplodeFinishedCallback(explode);

    m_bombsVec.push_back(std::move(bomb));
}

void BombManager::explodeNextBomb()
{
    Bomb *lowerBomb = nullptr;

    for(auto &bomb : m_bombsVec)
    {
        //Bomb isn't alive... - Do not interest us.
        if(bomb->getState() != Bomb::State::Alive)
            continue;

        //Edge case - There is not lower bomb yet...
        if(!lowerBomb)
        {
            lowerBomb = bomb.get();
            continue;
        }

        //Current bomb is at bottom of the "current" lower bomb.
        if(bomb->getPosition().y > lowerBomb->getPosition().y)
            lowerBomb = bomb.get();
    }

    //There is any bomb to explode?
    if(lowerBomb)
    {
        lowerBomb->explode();
        playExplodeEffect();
        m_onBombExplodeCallback();
    }
}

void BombManager::stopAllBombs()
{
    stopDroppingEffect();

    for(auto &bomb : m_bombsVec)
        bomb->stopDropping();
}

void BombManager::resetAllBombs()
{
    for(auto &bomb : m_bombsVec)
        bomb->reset(m_turnInfo);
}


////////////////////////////////////////////////////////////////////////////////
// Sound Effects                                                              //
////////////////////////////////////////////////////////////////////////////////
void BombManager::initSounds()
{
    //Init the sounds.
    auto soundMgr = Lore::SoundManager::instance();

    //Dropping
    soundMgr->loadEffect(kSoundName_Dropping);

    //Caught
    soundMgr->loadEffect(kSoundName_Caught);

    //Exploded
    for(int i = 0; i < kSoundExplode_Count; ++i)
    {
        auto name = CoreGame::StringUtils::format(kSoundName_ExplodeFmt, i);
        soundMgr->loadEffect(name);
    }
}

void BombManager::playDroppingEffect()
{
    if(!m_isPlayingDroppingEffect)
    {
        KABOOM_DLOG("BombManager::playDroppingEffect - Start Playing");

        m_isPlayingDroppingEffect = true;

        Lore::SoundManager::instance()->playEffect(
            kSoundName_Dropping,
            Lore::SoundManager::kPlayForever
        );
    }
}

void BombManager::stopDroppingEffect()
{
    m_isPlayingDroppingEffect = false;
    Lore::SoundManager::instance()->stopEffect(kSoundName_Dropping);
}


void BombManager::playCaughtEffect()
{
    Lore::SoundManager::instance()->playEffect(kSoundName_Caught);
}

void BombManager::playExplodeEffect()
{
    auto name = CoreGame::StringUtils::format(kSoundName_ExplodeFmt, 0);
    Lore::SoundManager::instance()->playEffect(name);
}

////////////////////////////////////////////////////////////////////////////////
// Bombs Callbacks                                                            //
////////////////////////////////////////////////////////////////////////////////
void BombManager::onBombReachTarget()
{
    KABOOM_DLOG("BombManager::onBombReachTarget - Bomb reach target");

    m_onBombReachTargetCallback();

    m_isExplodingBombs = true;
    stopAllBombs();

    //Start exploding the bombs.
    explodeNextBomb();
}

void BombManager::onBombExplodeFinished()
{
    KABOOM_DLOG("BombManager::onBombExplodeFinished - Bomb Explode Finished");

    --m_aliveBombs;
    if(m_aliveBombs == 0)
        m_onAllBombsExplodedCallback();
    else
        explodeNextBomb();
}