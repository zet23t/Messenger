#include "game_common.h"
#include "lib_sound.h"


TinyScreen display = TinyScreen(TinyScreenPlus);
RenderBuffer<uint16_t,RENDER_COMMAND_COUNT> buffer;
TileMap::SceneRenderer<uint16_t,RENDER_COMMAND_COUNT> renderer;
TileMap::Scene<uint16_t> tilemap;


namespace Time {
    uint32_t tick;
    uint32_t prevMillis;
    uint32_t millis;
    uint16_t delta;
}

namespace Game {
    int16_t camX = 0;
    int16_t camY = 0;

    Texture<uint16_t> atlas;
    Texture<uint16_t> forest;
    Texture<uint16_t> ground1, ground2, ground0,ground3,uiPanelBackground;

    const uint8_t STATUS_INTRO = 0, STATUS_INTRO_OUTRO = 1, STATUS_PLAY = 3, STATUS_GAMEOVER = 4, STATUS_GAMEWON = 5;
    const int16_t INTRO_CAMY_START = -60;
    uint8_t gameStatus;
    uint32_t gameStatusUpdateMillis;

    void setStatus(uint8_t s) {
        if (gameStatus != s) {
            gameStatusUpdateMillis = Time::millis;
            gameStatus = s;
        }
    }

    RenderCommand<uint16_t>* drawCenteredSprite(int x,int y,SpriteSheetRect rect, bool mirrorh) {
        return buffer.drawRect(mirrorh ? x + rect.origWidth - rect.offsetX - rect.width - rect.origWidth / 2
                                : x + rect.offsetX - rect.origWidth / 2,
                               y + rect.offsetY - rect.origHeight / 2,
                               rect.width,rect.height)->sprite(&atlas, mirrorh ? rect.x + rect.width - 1: rect.x, rect.y, mirrorh, false);
    }
    RenderCommand<uint16_t>* drawSprite(int x,int y,SpriteSheetRect rect, bool mirrorh) {
        return buffer.drawRect(mirrorh ? x + rect.origWidth - rect.offsetX - rect.width
                                : x + rect.offsetX,
                               y + rect.offsetY,
                               rect.width,rect.height)->sprite(&atlas, mirrorh ? rect.x + rect.width - 1: rect.x, rect.y,mirrorh, false);
    }

    bool isHeld(int id) {
        return Joystick::getButton(id,Joystick::Phase::CURRENT);
    }
    bool wasHeld(int id) {
        return Joystick::getButton(id,Joystick::Phase::PREVIOUS);
    }
    bool isPressed(int id) {
        return Joystick::getButton(id,Joystick::Phase::CURRENT) && !Joystick::getButton(id,Joystick::Phase::PREVIOUS);
    }
    bool isReleased(int id) {
        return !Joystick::getButton(id,Joystick::Phase::CURRENT) && Joystick::getButton(id,Joystick::Phase::PREVIOUS);
    }
    static int isContinousPress(bool focused, bool continous) {
        static uint32_t inputBlockTime;
        if (!continous || !focused) return focused && isReleased(0);
        if (isReleased(0) || !isHeld(0)) return 0;
        if (isPressed(0)) {
            inputBlockTime = Time::millis;
            return 1;
        }
        if (Time::millis - inputBlockTime < 200) return 0;
        inputBlockTime += 250;
        return 1;
    }
    void drawText(const char *text, int8_t x, int8_t y, int8_t w, int8_t h, int8_t halign, int8_t valign, uint8_t depth) {
        buffer.drawText(text,x,y,w,h,halign,valign,true,FontAsset::font,depth,RenderCommandBlendMode::opaque);
    }
    void drawText(const char *text, int8_t x, int8_t y, int8_t w, int8_t h) {
        drawText(text,x,y,w,h,0,0,0);
    }
    struct SpriteAnimation {
        const SpriteSheet sheet;
        const uint8_t *frames;
        const uint16_t *durations;
        const uint8_t looping;

        bool draw(int16_t x, int16_t y, uint8_t dep, uint32_t time, bool mirrorh) const;
    };

    bool SpriteAnimation::draw(int16_t x, int16_t y, uint8_t dep, uint32_t time, bool mirrorh) const{
        uint32_t length =0;
        int i = 0;
        while (frames[i] != 0xff) length+= durations[i++];

        uint32_t t = looping ? time % length : time;
        uint32_t d = durations[0];
        i = 0;
        while (t > d && frames[i+1] != 0xff) {
            i+=1;
            d+= durations[i];
        }
        //printf("%d %d\n",i,d);
        drawCenteredSprite(x,y,sheet.sprites[frames[i]], mirrorh)->setDepth(dep);
        return !looping && frames[i+1] == 0xff && t > d;
    }

    namespace PlayerAnimation {
        const uint8_t idle_frames[] = {
            0,1,0,1,0,1,0xff
        };
        const uint16_t idle_durations[] = {
            700,900,650,940,1400,900
        };
        const uint8_t walk_frames[] = {0,1,0xff};
        const uint16_t walk_durations[] = {100,100};

        const uint8_t strike_frames[] = {0,1,2,3,4,5,6,0xff};
        const uint16_t strike_durations[] = {10,10,10,10,10,10,10};

        const uint8_t die_frames[] =     {0,  1,  2,  1,  2,  1,  2,  3,  4,  5,  6,  5,  7,0xff};
        const uint16_t die_durations[] = {100,200,600,700,400,200,100,100,300,900,800,200,1000,10};

        const SpriteAnimation idleDirLeft = {
            ImageAsset::TextureAtlas_atlas::char_player_idle_left,
            idle_frames, idle_durations,1
        };
        const SpriteAnimation walkDirLeft = {
            ImageAsset::TextureAtlas_atlas::char_player_walk_left,
            walk_frames, walk_durations,1
        };
        const SpriteAnimation strikeDirLeft = {
            ImageAsset::TextureAtlas_atlas::char_player_strike_left,
            strike_frames, strike_durations,0
        };
        const SpriteAnimation die = {
            ImageAsset::TextureAtlas_atlas::char_player_die,
            die_frames, die_durations,0
        };
    }

    namespace AllyAnimation {
        const uint8_t idle_frames[] = {
            0,1,0,1,0,1,0xff
        };
        const uint16_t idle_durations[] = {
            700,900,650,940,1400,900
        };
        const uint8_t walk_frames[] = {0,1,0xff};
        const uint16_t walk_durations[] = {100,100};

        const uint8_t strike_frames[] = {0,1,2,3,4,5,6,0xff};
        const uint16_t strike_durations[] = {30,30,30,30,30,30,30};

        const uint8_t die_frames[] =     {0,  1,  2,  3,  4,  5,  6,  7,0xff};
        const uint16_t die_durations[] = {100,200,400,100,300,300,300,1000,10};

        const SpriteAnimation idle = {
            ImageAsset::TextureAtlas_atlas::ally_player_idle_left,
            idle_frames, idle_durations,1
        };
        const SpriteAnimation walk = {
            ImageAsset::TextureAtlas_atlas::ally_player_walk_left,
            walk_frames, walk_durations,1
        };
        const SpriteAnimation strike = {
            ImageAsset::TextureAtlas_atlas::ally_player_strike_left,
            strike_frames, strike_durations,0
        };
        const SpriteAnimation die = {
            ImageAsset::TextureAtlas_atlas::ally_player_die,
            die_frames, die_durations,0
        };
    }


    namespace EnemySwordsAnimation {
        const uint8_t idle_frames[] = {
            0,1,0,1,0,1,0xff
        };
        const uint16_t idle_durations[] = {
            510,440,720,800,1122,500
        };
        const uint8_t walk_frames[] = {0,1,0xff};
        const uint16_t walk_durations[] = {100,100};

        const uint8_t strike_frames[] = {0,1,2,0xff};
        const uint16_t strike_durations[] = {30,30,30};

        const uint8_t die_frames[] = {0,1,2,4,5,0xff};
        const uint16_t die_durations[] = {30,30,30,30,30,30,30};

        const SpriteAnimation idleDirLeft = {
            ImageAsset::TextureAtlas_atlas::enemy_swords_idle,
            idle_frames, idle_durations,1
        };
        const SpriteAnimation walkDirLeft = {
            ImageAsset::TextureAtlas_atlas::enemy_swords_walk,
            walk_frames, walk_durations,1
        };
        const SpriteAnimation strikeDirLeft = {
            ImageAsset::TextureAtlas_atlas::enemy_swords_strike,
            strike_frames, strike_durations,0
        };
        const SpriteAnimation die  = {
            ImageAsset::TextureAtlas_atlas::enemy_swords_die,
            die_frames, die_durations,0
        };
    }

    const uint8_t CHAR_TYPE_ENEMY_SWORDSFIGHTER = 1, CHAR_TYPE_ALLY = 2;

    struct Character {
        bool active;
        uint8_t health;
        const SpriteAnimation* idle;
        const SpriteAnimation* walk;
        const SpriteAnimation* strike;
        const SpriteAnimation* die;
        uint16_t animMillis;
        uint8_t isStriking;
        uint32_t dieTime;
        Fixed2D4 pos;
        Fixed2D4 speed;
        int8_t dir;
        uint8_t tookDamage;
        uint32_t tookDamageTime;
        uint8_t type;

        uint8_t strikeCooldown, strikeCooldownDuration;
        void init (const SpriteAnimation* idleLeft,
                   const SpriteAnimation* walkLeft,
                   const SpriteAnimation* strikeLeft,
                   const SpriteAnimation* die,
                   Fixed2D4 p, uint8_t type) {
            this->type = type;
            active = true;
            pos = (p);
            speed = Fixed2D4(0,0);
            idle = idleLeft;
            this->die = die;
            walk = walkLeft;
            strike = strikeLeft;
            strikeCooldownDuration = 15;
            health = 3;
            dieTime = 0;
        }
        bool doStrike() {
            if (isStriking || strikeCooldown > 0) return false;
            isStriking = true;
            strikeCooldown = strikeCooldownDuration;
            onStrike();
            return true;
        }
        void takeDamage(Fixed2D4 push) {
            pos += push;
            if (health > 0 && (Time::millis - tookDamageTime) > 500) {
                health -=1;
                tookDamage = 1;
                tookDamageTime = Time::millis;
                if (health == 0) {
                    active = 0;
                    dieTime = Time::millis;
                    animMillis = 1;
                }
            }
        }
        virtual void onStrike() {}

        void draw() {
            if (!active && animMillis == 0) return;
            uint8_t dep = (uint8_t) pos.y.getIntegerPart();
            int16_t y = pos.y.getIntegerPart();
            if (y<45) {
                y = 45 - (45-y) / 2;
            }
            else if (y<50) {
                y = 50 - (50-y) * 3 / 4;
            }
            if (health == 0) {
                if (!die->draw(pos.x.getIntegerPart(),y,dep,animMillis,dir == 1)) {
                    animMillis += Time::delta;
                }
                return;
            }
            if (isStriking) {
                animMillis += Time::delta;
                if (strike->draw(pos.x.getIntegerPart(),y,dep,animMillis, dir == 1)) {
                    isStriking = 0;
                    animMillis = 0;
                }
            } else {
                if (speed.calcSqrLen() < Fix4(0,3)) {
                    idle->draw(pos.x.getIntegerPart(),y,dep,Time::millis, dir == 1);

                } else {
                    walk->draw(pos.x.getIntegerPart(),y,dep,Time::millis, dir == 1);
                }
            }
        }
        virtual void tick() {
            if (!active) return;
            if (speed.x > 0) dir = 1;
            if (speed.x < 0) dir = 0;
            if (pos.y < Fix4(42,0)) pos.y= Fix4(42,0);
            if (pos.y > Fix4(58,0)) pos.y= Fix4(58,0);
            pos += speed;
            speed = speed * Fix4(0,12);
            if (strikeCooldown > 0) strikeCooldown --;
            //tookDamage = 0;
        }
        void finishTick() {
            tookDamage = 0;
        }
    };

    struct AICharacter;
    struct AIStateMachineNode;

    struct AIStateMachineTransition {
        AIStateMachineNode *to;
        AIStateMachineTransition *next;

        AIStateMachineTransition(AIStateMachineNode& to) : to(&to),next(0) {
        }

        virtual bool check(AICharacter *ai) {
            return true;
        };

        virtual AIStateMachineNode* testActivation(AICharacter* ai) {
            if (check(ai)) return to;
            if (next) return next->testActivation(ai);
            return 0;
        }
    };

    struct AIStateMachineTransitionDelayed : AIStateMachineTransition{
        uint32_t delay;
        AIStateMachineTransitionDelayed(AIStateMachineNode& to, uint32_t delay) : AIStateMachineTransition(to), delay(delay) {
        }
        bool check(AICharacter *a);
    };

    struct AIStateMachineTransitionWhenPlayerInRange : AIStateMachineTransition{
        Fix4 range;
        bool inrange;
        AIStateMachineTransitionWhenPlayerInRange(AIStateMachineNode& to, Fix4 range, bool inrange) : AIStateMachineTransition(to), range(range), inrange(inrange) {
        }
        bool check (AICharacter *a);
    };

    struct AIStateMachineTransitionWhenEnemyPresent : AIStateMachineTransition{
        bool present;
        Fix4 range;
        AIStateMachineTransitionWhenEnemyPresent(AIStateMachineNode& to, bool present, Fix4 range) : AIStateMachineTransition(to),present(present), range(range) {
        }
        bool check (AICharacter *a);
    };

    struct AIStateMachineTransitionAll : AIStateMachineTransition {
        AIStateMachineTransition* first;
        AIStateMachineTransitionAll(AIStateMachineNode &to) : AIStateMachineTransition(to) {}
        void addTransition(AIStateMachineTransition &t) {
            t.next = first;
            first = &t;
        }
        bool check (AICharacter *a) {
            AIStateMachineTransition* browse = first;
            while (browse) {
                if (!browse->check(a)) return false;
                browse = browse->next;
            }
            return true;
        }
    };
    struct AIStateMachineTransitionWhenTookDamage : AIStateMachineTransition {
        AIStateMachineTransitionWhenTookDamage(AIStateMachineNode &to) : AIStateMachineTransition(to) {}
        bool check(AICharacter *a);
    };

    struct AIStateMachineNode {
        AIStateMachineTransition *transition;
        bool lookAtPlayer;

        AIStateMachineNode* tick(AICharacter *ai);
        virtual void onEnter(AICharacter* ai) {}
        virtual void onExit(AICharacter* ai) {}
        virtual void onStay(AICharacter* ai) {}

        void addTransition(AIStateMachineTransition &t) {
            t.next = transition;
            transition = &t;
        }
    };

    struct AIStateMachineNodeMoveTo : AIStateMachineNode {
        Fix4 minRange, maxRange, speed;
        AIStateMachineNodeMoveTo(Fix4 minRange, Fix4 maxRange, Fix4 speed) : minRange(minRange), maxRange(maxRange), speed(speed) {}
        void onStay(AICharacter*a);
        virtual Fixed2D4 getTarget(AICharacter *a);
    };
    struct AIStateMachineNodeFollowPlayer : AIStateMachineNodeMoveTo {
        AIStateMachineNodeFollowPlayer(Fix4 minRange, Fix4 maxRange, Fix4 speed) : AIStateMachineNodeMoveTo(minRange, maxRange, speed) {}
        Fixed2D4 getTarget(AICharacter *a);
    };
    struct AIStateMachineNodeMoveToPlayer : AIStateMachineNodeMoveTo {
        AIStateMachineNodeMoveToPlayer(Fix4 minRange, Fix4 maxRange, Fix4 speed) : AIStateMachineNodeMoveTo(minRange, maxRange, speed) {}
        Fixed2D4 getTarget(AICharacter *a);
    };
    struct AIStateMachineNodeMoveToIdleSpot : AIStateMachineNodeMoveTo {
        AIStateMachineNodeMoveToIdleSpot(Fix4 minRange, Fix4 maxRange, Fix4 speed) : AIStateMachineNodeMoveTo(minRange, maxRange, speed) {}
        Fixed2D4 getTarget(AICharacter *a);
    };
    struct AIStateMachineNodeMoveToNearestNonAlly : AIStateMachineNodeMoveTo {
        AIStateMachineNodeMoveToNearestNonAlly(Fix4 minRange, Fix4 maxRange, Fix4 speed) : AIStateMachineNodeMoveTo(minRange, maxRange, speed) {}
        Fixed2D4 getTarget(AICharacter *a);
    };
    struct AIStateMachineNodeAttackOnExit : AIStateMachineNode {
        void onExit(AICharacter *a);
    };

    namespace MeleeFighterFSM {
        AIStateMachineNodeMoveToIdleSpot idle(Fix4(0,0),Fix4(6,0),Fix4(0,5));
        AIStateMachineNodeMoveToPlayer engage(Fix4(5,0),Fix4(8,0), Fix4(0,3));
        AIStateMachineNodeAttackOnExit attack;
        AIStateMachineNodeMoveToPlayer retreat(Fix4(22,0),Fix4(32,0), Fix4(0,6));

        AIStateMachineTransitionWhenPlayerInRange idleToEngage(engage, Fix4(18,0), true);
        AIStateMachineTransitionWhenPlayerInRange engageToIdle(idle, Fix4(22,0), false);
        AIStateMachineTransitionWhenPlayerInRange engageToAttack(attack,Fix4(9,0), true);
        AIStateMachineTransitionDelayed attackToEngage(engage, 500);

        AIStateMachineTransitionWhenTookDamage engageToRetreat(retreat);
        AIStateMachineTransitionWhenTookDamage idleToRetreat(retreat);
        AIStateMachineTransitionWhenTookDamage attackToRetreat(retreat);
        AIStateMachineTransitionDelayed retreatToIdle(idle, 2000);

        void init() {
            idle.addTransition(idleToEngage);
            engage.addTransition(engageToIdle);
            engage.addTransition(engageToAttack);
            attack.addTransition(attackToEngage);
            attack.addTransition(attackToRetreat);
            engage.addTransition(engageToRetreat);
            idle.addTransition(idleToRetreat);
            retreat.addTransition(retreatToIdle);
            idle.lookAtPlayer = 1;
            engage.lookAtPlayer = 1;
            attack.lookAtPlayer = 1;
            retreat.lookAtPlayer = 0;
        }
    }


    namespace AllyFighterFSM {
        AIStateMachineNodeFollowPlayer idle(Fix4(0,0),Fix4(6,0),Fix4(0,7));
        AIStateMachineNodeMoveToNearestNonAlly engage(Fix4(3,0),Fix4(6,0), Fix4(0,3));
        AIStateMachineNodeAttackOnExit attack;
        //AIStateMachineNodeMoveToPlayer retreat(Fix4(22,0),Fix4(32,0), Fix4(0,6));

        AIStateMachineTransitionWhenEnemyPresent idleToEngage(engage, true, Fix4(34,0));
        AIStateMachineTransitionWhenEnemyPresent engageToIdle(idle, false, Fix4(50,0));
        AIStateMachineTransitionWhenPlayerInRange engageToAttack(attack,Fix4(7,0), true);

        AIStateMachineTransitionDelayed attackToEngage(engage, 100);

        //AIStateMachineTransitionWhenTookDamage engageToRetreat(retreat);
        //AIStateMachineTransitionWhenTookDamage idleToRetreat(retreat);
        //AIStateMachineTransitionWhenTookDamage attackToRetreat(retreat);
        //AIStateMachineTransitionDelayed retreatToIdle(idle, 2000);

        void init() {
            idle.addTransition(idleToEngage);
            engage.addTransition(engageToIdle);
            attack.addTransition(attackToEngage);
            engage.addTransition(engageToAttack);
            /*
            attack.addTransition(attackToRetreat);
            engage.addTransition(engageToRetreat);
            idle.addTransition(idleToRetreat);
            retreat.addTransition(retreatToIdle);
            idle.lookAtPlayer = 1;
            engage.lookAtPlayer = 1;
            attack.lookAtPlayer = 1;
            retreat.lookAtPlayer = 0;*/
        }
    }

    struct AICharacter : Character {
        AIStateMachineNode *activeNode;
        uint32_t nodeActiveSince;
        uint8_t lookAtPlayer;
        Fixed2D4 idleSpot;
        void tick();
        void onStrike();
    };

    struct PlayerCharacter : Character {

        void tick() {
            if (!active) return;
            Character::tick();
            if (!isStriking && gameStatus == STATUS_PLAY && health > 0) {
                Fixed2D4 stick = Joystick::getJoystick();
                if(stick.x > Fix4(0,8)) stick.x = Fix4(0,8);
                if(stick.x < Fix4(-1,8)) stick.x = Fix4(-1,8);
                if(stick.x < 0 && pos.x.getIntegerPart() - 10 < camX) stick.x = 0;
                if (stick.x > 0 && pos.x.getIntegerPart() + 10 > camX + 96) stick.x = 0;
                if(stick.y > Fix4(0,8)) stick.y = Fix4(0,8);
                if(stick.y < Fix4(-1,8)) stick.y = Fix4(-1,8);
                speed += stick * Fix4(0,12);
                if (Joystick::getButton(0)) doStrike();
                //if (isPressed(1)) takeDamage(Fixed2D4(1,0));

            }
            if (gameStatus == STATUS_GAMEWON) {
                speed = Fixed2D4(Fix4(0,12),Fix4(0,0));
            }
        }
        void onStrike();
    };
    PlayerCharacter playerChar;

    const int MAX_AI_CHARS = 32;
    AICharacter aiChars[MAX_AI_CHARS];

    int countAI(int16_t minx, int16_t maxx) {
        int n = 0;
        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            int16_t x = aiChars[i].pos.x.getIntegerPart();
            if (aiChars[i].active && aiChars[i].type != CHAR_TYPE_ALLY && x >= minx && x <= maxx) n+=1;
        }
        return n;
    }

    int countAI() {
        int n = 0;
        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            if (aiChars[i].active && aiChars[i].type != CHAR_TYPE_ALLY) n+=1;
        }
        return n;
    }

    void damageClosestAIs(bool isPlayer, Fixed2D4 pos) {
        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            AICharacter* ai = &aiChars[i];
            if (!ai->active || (ai->type == CHAR_TYPE_ALLY) == isPlayer || ai->health == 0) continue;
            Fixed2D4 diff = ai->pos  - pos;
            //printf("%d\n",diff.x.getIntegerPart());
            if (diff.x.absolute().getIntegerPart() < 8 && diff.y.absolute().getIntegerPart() < 7) {
                ai->takeDamage(diff.normalize() * Fix4(3,0));
            }
        }
    }

    void AICharacter::onStrike() {
        damageClosestAIs(type == CHAR_TYPE_ALLY, pos);
        if (type != CHAR_TYPE_ALLY) {
            Character* ai = &playerChar;
            if (!ai->active) return;
            Fixed2D4 diff = ai->pos  - pos;
            //printf("%d\n",diff.x.getIntegerPart());
            if (diff.x.absolute().getIntegerPart() < 8 && diff.y.absolute().getIntegerPart() < 7) {
                ai->takeDamage(diff.normalize() * Fix4(3,0));
            }
        }
    }

    void PlayerCharacter::onStrike() {
        //printf("strike\n");
        damageClosestAIs(true, pos);
    }

    bool AIStateMachineTransitionDelayed::check(AICharacter *a)  {
        return Time::millis - a->nodeActiveSince > delay;
    }
    bool AIStateMachineTransitionWhenEnemyPresent::check(AICharacter *ai) {
        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            AICharacter* otherai = &aiChars[i];
            if (!otherai->active || (otherai->type == CHAR_TYPE_ALLY) || otherai->health == 0) continue;
            Fix4 dist = (otherai->pos - ai->pos).length();
            //printf("%d %d\n", dist.getIntegerPart(), range.getIntegerPart());
            if (dist < range) {
                return present == true;
            }
        }
        return present == false;
    }
    bool AIStateMachineTransitionWhenPlayerInRange::check(AICharacter *ai) {
        bool isPlayer = false;
        if (ai->type == CHAR_TYPE_ALLY) {
            isPlayer = true;
        } else {
            if (playerChar.health > 0) {
                Fixed2D4 diff = (ai->pos - playerChar.pos);
                if ((diff.length() < range) == inrange) return true;
            }
        }

        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            AICharacter* otherai = &aiChars[i];
            if (!otherai->active || (otherai->type == CHAR_TYPE_ALLY) == isPlayer || otherai->health == 0) continue;
            Fixed2D4 diff = (ai->pos - otherai->pos);
            if ((diff.length() < range) == inrange) return true;

        }

        return false;
        //printf("%d %d\n",range.getIntegerPart(),diff.length().getIntegerPart());
    }

    Fixed2D4 AIStateMachineNodeMoveTo::getTarget(AICharacter *a) {
        return a->pos;
    }
    Fixed2D4 AIStateMachineNodeMoveToPlayer::getTarget(AICharacter *a) {
        return playerChar.pos;
    }
    Fixed2D4 AIStateMachineNodeFollowPlayer::getTarget(AICharacter *a) {
        return playerChar.pos + a->idleSpot;
    }
    Fixed2D4 AIStateMachineNodeMoveToIdleSpot::getTarget(AICharacter *a) {
        return a->idleSpot;
    }
    Fixed2D4 AIStateMachineNodeMoveToNearestNonAlly::getTarget(AICharacter *a) {
        Fix4 closestDist = -1;
        Fixed2D4 closest = a->pos;
        for (int i=0;i<MAX_AI_CHARS;i+=1) {
            if (!aiChars[i].active || aiChars[i].type == CHAR_TYPE_ALLY) continue;
            Fixed2D4 to = aiChars[i].pos;
            Fix4 dist = (to - a->pos).length();
            if (closestDist < 0 || dist < closestDist) {
                closestDist = dist;
                closest = to;
            }
        }
        return closest;
    }

    void AIStateMachineNodeMoveTo::onStay(AICharacter *ai) {
        Fixed2D4 to = getTarget(ai);
        Fixed2D4 diff = (ai->pos - to);
        Fix4 dist = diff.length();
        if (dist < minRange)
            ai->speed += diff.normalize() * speed;
        if (dist > maxRange)
            ai->speed -= diff.normalize() * speed;
    }

    void AIStateMachineNodeAttackOnExit::onExit(AICharacter *ai) {
        ai->doStrike();
    }

    bool AIStateMachineTransitionWhenTookDamage::check(AICharacter *a) {
        return a->tookDamage != 0;
    }

    AIStateMachineNode* AIStateMachineNode::tick(AICharacter *ai) {
        if (transition) {
            AIStateMachineNode *next = transition->testActivation(ai);
            if (next) {
                onExit(ai);
                next->onEnter(ai);
                ai->lookAtPlayer = next->lookAtPlayer;
                //printf("")
                return next;
            }
        }
        onStay(ai);
        return 0;
    }

    void AICharacter::tick() {
        if (!active) return;
        Character::tick();
        if (lookAtPlayer) {
            Fixed2D4 diff = playerChar.pos - pos;
            dir = (diff.x < 0) ? 0 : 1;
        }
        if (activeNode) {
            AIStateMachineNode* next =activeNode->tick(this);
            if (next != activeNode && next) {
                activeNode=next;
                nodeActiveSince = Time::millis;
                //printf("Active node change: %p\n",activeNode);
            }
        }
        //Fixed2D4 diff = (pos - playerChar.pos);
        //if (!isStriking && diff.length() < Fix4(8,0))
        //    doStrike();
    }

    void drawMessage(const char* message) {
        const int ox = 10, width = 84;
        const int oy = 2, height = 24;
        auto tl = ImageAsset::TextureAtlas_atlas::ui_msg_panel_topleft.sprites[0];
        auto bl = ImageAsset::TextureAtlas_atlas::ui_msg_panel_bottomleft.sprites[0];
        buffer.drawRect(ox+4,oy+3,width-8,height-8)->sprite(&uiPanelBackground)->setDepth(200);
        drawSprite(ox,oy,tl,false)->setDepth(200);
        drawSprite(ox,oy+height - bl.height,bl,false)->setDepth(200);
        drawSprite(ox+width-tl.width,oy,tl,true)->setDepth(200);
        drawSprite(ox+width-bl.width,oy+height-bl.height,bl,true)->setDepth(200);
        buffer.setClipping(oy,ox+width-tl.width,64,0);
        for (int x=tl.width+ox,n = 9;x<96;x+=8,n+=1) {
            drawSprite(x,oy,ImageAsset::TextureAtlas_atlas::ui_msg_panel_top.sprites[
                       n * 17 % ImageAsset::TextureAtlas_atlas::ui_msg_panel_top.spriteCount],false)->setDepth(200);
            drawSprite(x,oy+height-5,ImageAsset::TextureAtlas_atlas::ui_msg_panel_bottom.sprites[
                       n * 19 % ImageAsset::TextureAtlas_atlas::ui_msg_panel_bottom.spriteCount],false)->setDepth(200);
        }
        buffer.setClipping(oy,ox+width,oy+height-bl.height,0);
        for (int y=tl.height+oy,n = 19;y<height+oy;y+=7,n+=1) {
            drawSprite(ox,y,ImageAsset::TextureAtlas_atlas::ui_msg_panel_left.sprites[
                       n * 17 % ImageAsset::TextureAtlas_atlas::ui_msg_panel_left.spriteCount],false)->setDepth(200);
            drawSprite(ox+width-4,y,ImageAsset::TextureAtlas_atlas::ui_msg_panel_left.sprites[
                       (n+7) * 11 % ImageAsset::TextureAtlas_atlas::ui_msg_panel_left.spriteCount],true)->setDepth(200);
        }
        buffer.setClipping(0,96,64,0);
        drawText(message,ox+4,oy+3,width-8,height-8,0,0,200);
        if (playerChar.health == 0) {
            int index=(Time::millis - playerChar.dieTime) / 300;
            //printf("%d %d %d\n", index,playerChar.dieTime,Time::millis);
            if (index > 3) index = 3;
            drawSprite(ox + 50,oy + 2, ImageAsset::TextureAtlas_atlas::ui_blood_spread.sprites[index],false)->blend(RenderCommandBlendMode::average)->setDepth(201);
        }
    }

    struct ScriptPoint;

    typedef bool (*ScriptCallback)(ScriptPoint&);

    const uint8_t MAX_ACTIVE_SCRIPT_CALLBACKS = 16;
    struct AciveScriptCallbacks {
        ScriptPoint* scriptPoints[MAX_ACTIVE_SCRIPT_CALLBACKS];

        void addScriptCallback(ScriptPoint &p) {
            for (int i=0;i<MAX_ACTIVE_SCRIPT_CALLBACKS;i+=1) {
                if (scriptPoints[i] == 0) {
                    scriptPoints[i] = &p;
                    return;
                }
            }
        }

        void init() {
            for (int i=0;i<MAX_ACTIVE_SCRIPT_CALLBACKS;i+=1) {
                scriptPoints[i] = 0;
            }
        }

        void callActiveCallbacks();
    };
    AciveScriptCallbacks activeScriptCallbacks;

    struct ScriptPoint {
        const char *msg;
        int16_t startX, endX;
        ScriptCallback onDrawCallback;
        uint8_t spawnEnemySwordFighterCount;
        uint8_t spawnEnemySwordFighterCountBehind;

        uint32_t enterTime;
        int16_t offsetX;

        void onDraw() {
        }

        void onEnter(int16_t posX) {
            offsetX = posX;
            enterTime = Time::millis;
            for (int i=0;i<spawnEnemySwordFighterCount;i+=1) {
                spawnEnemySwordFighter(posX + 60 + Math::randInt() % 16 - 8, 48 + Math::randInt() % 16 - 8, 60);
            }

            for (int i=0;i<spawnEnemySwordFighterCountBehind;i+=1) {
                spawnEnemySwordFighter(posX + 20 + Math::randInt() % 16 - 8, 48 + Math::randInt() % 16 - 8, -30);
            }
            if (onDrawCallback) activeScriptCallbacks.addScriptCallback(*this);
        }
    };

    void AciveScriptCallbacks::callActiveCallbacks() {
        for (int i=0;i<MAX_ACTIVE_SCRIPT_CALLBACKS;i+=1) {
            if (scriptPoints[i] != 0) {
                if (!scriptPoints[i]->onDrawCallback(*scriptPoints[i])) {
                    scriptPoints[i] = 0;
                }
            }
        }

    }

    bool SC_drawFortress(ScriptPoint& sc) {
        int16_t camXp4 = -camX / 2;
        int16_t camXp3 = -camX * 3 / 4;
        int16_t camXp2 = -camX * 7 / 8;
        int16_t camXp0 = -camX * 4 / 3;

        for (int i = 0;i<4;i+=1)
            drawCenteredSprite(camXp4 + i * 12,38,ImageAsset::TextureAtlas_atlas::bg_wall_forest.sprites[0],i%3);
        if (camXp3 > -20)
            drawCenteredSprite(camXp4 + 46, 31, ImageAsset::TextureAtlas_atlas::bg_tower.sprites[0],false);
        else
            drawCenteredSprite(camXp4 + 46, 31, ImageAsset::TextureAtlas_atlas::bg_tower.sprites[1],false);
        if (camXp3 > -22)
            drawCenteredSprite(camXp3 + 54, 41, ImageAsset::TextureAtlas_atlas::bg_wall_side.sprites[0],false);
        else {
            if (camXp3 < -28)
                drawCenteredSprite(camXp4 + 42, 41, ImageAsset::TextureAtlas_atlas::bg_wall_side.sprites[0],true);
            drawCenteredSprite(camXp3 + 58, 41, ImageAsset::TextureAtlas_atlas::bg_wall_side.sprites[0],true);

        }
        drawCenteredSprite(camXp2 + 58, 42, ImageAsset::TextureAtlas_atlas::bg_gate_pillar_back.sprites[0],false);
        drawCenteredSprite(camXp2 + 62, 42, ImageAsset::TextureAtlas_atlas::bg_gate_pillar_back.sprites[1],false);
        drawCenteredSprite(camXp0 + 75, 45, ImageAsset::TextureAtlas_atlas::bg_gate_pillar_front.sprites[0],false)->setDepth(100);
        drawCenteredSprite(camXp0 + 81, 45, ImageAsset::TextureAtlas_atlas::bg_gate_pillar_front.sprites[1],false)->setDepth(100);
        drawCenteredSprite(-camX + 30, 47, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[0],true);
        drawCenteredSprite(-camX + 36, 53, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[0],true);
        drawCenteredSprite(-camX + 29, 59, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[0],false);
        drawCenteredSprite(-camX + 15, 53, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[1],false);
        drawCenteredSprite(-camX + 9, 58, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[1],false);
        drawCenteredSprite(-camX + 78, 53, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[0],false);
        drawCenteredSprite(camXp0 + 12, 62, ImageAsset::TextureAtlas_atlas::ground_dirt_patch.sprites[1],false);
        drawCenteredSprite(camXp2 + 10, 39, ImageAsset::TextureAtlas_atlas::bg_tent_back.sprites[0],false);
        drawCenteredSprite(camXp2 + 30, 39, ImageAsset::TextureAtlas_atlas::bg_tent_back.sprites[0],false);
        drawCenteredSprite(camXp0 + 2, 47, ImageAsset::TextureAtlas_atlas::bg_tent_front.sprites[0],false)->setDepth(100);

        return camX < 150;
    }
    bool SC_drawFrontTrees(ScriptPoint& sc) {
        int16_t offset = camX - sc.offsetX;
        int16_t camXp4 = -offset / 2;
        int16_t camXp3 = -offset * 3 / 4;
        int16_t camXp2 = -offset * 7 / 8;
        int16_t camXp0 = -offset * 4 / 3;

        drawSprite(camXp4 + 100,33, ImageAsset::TextureAtlas_atlas::bg_dark_bushes.sprites[0],false);
        drawSprite(camXp0 + 100,44, ImageAsset::TextureAtlas_atlas::fg_tree.sprites[0],false)->setDepth(100);
        drawSprite(camXp0 + 120,48, ImageAsset::TextureAtlas_atlas::fg_tree.sprites[0],true)->setDepth(100);
        return offset < 180;
    }


    ScriptPoint script[] = {
        {"Beloved almighty EMPEROR",0,100,SC_drawFortress},
        {"I, CENTURIO Claudius, have selected",0,100, SC_drawFrontTrees},
        {"four of my finest men to send you",0,50},
        {"... grave news. ",0,80,0,5,2},
        {"I hope they will reach you timely -",0,50},
        {"shall the gods help them",0,50, 0,4,2},
        {"to reach you at all.",0,50},
        {"I fear they have no chance of success",0,50, 0, 5, 1},
        {"and if they fail, we all will perish.",0,50},
        {"The enemy has crossed the river",0,50, 0, 4,3},
        {"in such great numbers that all",0,50, 0, 6},
        {"fortresses but ours have fallen.",0,50},
        {"We alone won't be able to hold it.",0,50, 0, 2,4},
        {"You have to send aid.",0,50},
        {"If you don't, I fear ...",0,50},
        {"that the borders will collapse.",0,50, 0, 4, 4},
        {"But I deeply trust my messenger",0,50},
        {"to reach you soon.",0,50},

    };
    uint16_t currentScriptIndex;
    int16_t currentScriptPosX;

    void tick() {
        Time::tick += 1;
        Time::delta = Time::millis - Time::prevMillis;

        const int scriptCount = sizeof(script) / sizeof(ScriptPoint);
        for (uint16_t i=currentScriptIndex;i<scriptCount;i+=1) {
            if (currentScriptPosX <= camX && script[i].endX + currentScriptPosX > camX) {
                if (script[i].msg && gameStatus == STATUS_PLAY && playerChar.health > 0) drawMessage(script[i].msg);
                if (currentScriptIndex != i) {
                    currentScriptIndex = i;
                    script[i].onEnter(currentScriptPosX);
                }

                break;
            }
            if (i+1 < scriptCount)
                currentScriptPosX += script[i].endX;
        }
        if (playerChar.health == 0 && gameStatus == STATUS_PLAY) drawMessage("... but I fear, my messenger will die.");
        if (countAI(camX-50,camX + 48) == 0 && currentScriptIndex + 1 < scriptCount) {
            int16_t x = playerChar.pos.x.getIntegerPart();
            if (x - 10 > camX) {
                camX = (camX * 15 + x - 10) / 16;
            }
        }

        Sound::tick();
        playerChar.tick();
        for (int i=0;i<MAX_AI_CHARS && camY == 0;i+=1) {
            aiChars[i].tick();
        }

        int16_t camXp4 = -camX / 2;
        int16_t camXp3 = -camX * 3 / 4;
        int16_t camXp2 = -camX * 7 / 8;
        int16_t camXp0 = -camX * 4 / 3;

        buffer.setOffset(0,camY);

        buffer.drawRect(camXp4,28,4096,4)->sprite(&forest);
        buffer.drawRect(camXp4,32,4096,12)->filledRect(0x6312);
        buffer.drawRect(camXp3,38,4096,4)->sprite(&ground3);
        buffer.drawRect(camXp3,42,4096,8)->filledRect(0xc30a);
        buffer.drawRect(camXp2,43,4096,8)->sprite(&ground2);
        buffer.drawRect(camXp2,43+8,4094,3)->filledRect(0x8724);
        buffer.drawRect(-camX,47,4096,8)->sprite(&ground1);
        buffer.drawRect(-camX,47+8,4096,16)->sprite(&ground0);

        script[currentScriptIndex].onDraw();
        activeScriptCallbacks.callActiveCallbacks();

        buffer.setOffset(camX,camY);

        playerChar.draw();
        for (int i=0;i<MAX_AI_CHARS && camY == 0;i+=1) {
            aiChars[i].draw();
        }
        for (int i=0;i<MAX_AI_CHARS && camY == 0;i+=1) {
            aiChars[i].finishTick();
        }

        // intro logo and anim
        if (gameStatus == STATUS_INTRO || gameStatus == STATUS_INTRO_OUTRO) {
            drawCenteredSprite(48,-30, ImageAsset::TextureAtlas_atlas::logo.sprites[0],false);

            if (gameStatus == STATUS_INTRO) {
                if (Time::tick %3 == 0) {
                    if (camY < INTRO_CAMY_START) camY +=1;
                    if (camY > INTRO_CAMY_START) camY -=1;
                }
                if (Time::tick % 40 > 20)
                    drawText("Press button to start", 0,-10,96,10,0,0,0);
                if (isPressed(0) || isPressed(1)) gameStatus = STATUS_INTRO_OUTRO;
            }
            if (gameStatus == STATUS_INTRO_OUTRO) {
                if (Time::tick %2 == 0) {
                    if (camY < 0) camY +=1;
                    if (camY > 0) camY -=1;
                }
                if (camY == 0) gameStatus = STATUS_PLAY;
            }
        }


        buffer.setOffset(0,0);
        for (int i=0;i<playerChar.health && gameStatus == STATUS_PLAY;i+=1) {
            drawSprite(1,5+i*4,ImageAsset::TextureAtlas_atlas::heart.sprites[0], false)->setDepth(201);
        }

        if (gameStatus == STATUS_PLAY && camX < 30) {
            for (int i = 0; i < Time::tick % 40 / 10; i +=1)
                drawSprite(70 + i * 8,50,ImageAsset::TextureAtlas_atlas::ui_arrow_right.sprites[0],false)->setDepth(201);

        }
        if (gameStatus == STATUS_PLAY && playerChar.health == 0 && Time::millis - playerChar.dieTime > 4000) {

            if (Time::tick % 40 > 20) {
                buffer.drawRect(0,40,96,20)->filledRect(0xffff)->blend(RenderCommandBlendMode::average)->setDepth(202);
                drawText("GAME OVER\nPress button to restart.", 0,40,96,20,0,0,202);
            }
            if (isPressed(0) || isPressed(1)) restart();
        }

        if (gameStatus == STATUS_PLAY && playerChar.health > 0 && currentScriptIndex + 1 == scriptCount) {
            gameStatus = STATUS_GAMEWON;
        }

        if (gameStatus == STATUS_GAMEWON && playerChar.pos.x.getIntegerPart() > camX + 100) {

            if (Time::tick % 40 > 20) {
                buffer.drawRect(0,40,96,24)->filledRect(0xffff)->blend(RenderCommandBlendMode::average)->setDepth(202);
                drawText("CONGRATULATIONS\nYOU WON!\nPress button to restart.", 0,40,96,24,0,0,202);
            }
            if (isPressed(0) || isPressed(1)) restart();
        }

        Time::prevMillis = Time::millis;
    }

    void clearAI() {
        for(int i = 0; i < MAX_AI_CHARS; i++) {
            aiChars[i].active = false;
            aiChars[i].animMillis = false;
        }
    }


    int getFreeAISlot() {
        int use = -1;
        for(int i = 0; i < MAX_AI_CHARS; i++) {
            if (!aiChars[i].active & (use < 0 || aiChars[use].dieTime > aiChars[i].dieTime)) {
                use = i;
            }
        }
        return use;
    }

    void spawnEnemySwordFighter(int x, int y, int xoffset) {
        int use = getFreeAISlot();
        if (use < 0) return;
        aiChars[use].activeNode = &MeleeFighterFSM::idle;
        aiChars[use].idleSpot = Fixed2D4(Fix4(x,0),Fix4(y,0));
        aiChars[use].init(
                       &EnemySwordsAnimation::idleDirLeft,
                       &EnemySwordsAnimation::walkDirLeft,
                       &EnemySwordsAnimation::strikeDirLeft,
                       &EnemySwordsAnimation::die,
                       Fixed2D4(Fix4(x + xoffset,0),Fix4(y + Math::randInt()%16-8,0)),
                        CHAR_TYPE_ENEMY_SWORDSFIGHTER);
    }

    void spawnAllySwordFighter(int x, int y) {
        int use = getFreeAISlot();
        if (use < 0) return;
        aiChars[use].activeNode = &AllyFighterFSM::idle;
        aiChars[use].idleSpot = Fixed2D4(Fix4(x,0),Fix4(y,0));
        aiChars[use].init(
                       &AllyAnimation::idle,
                       &AllyAnimation::walk,
                       &AllyAnimation::strike,
                       &AllyAnimation::die,
                       Fixed2D4(Fix4(x - 36,0),Fix4(y + Math::randInt()%16-8,0)),
                        CHAR_TYPE_ALLY);
    }

    void restart() {
        clearAI();
        activeScriptCallbacks.init();
        currentScriptIndex = 0;
        currentScriptPosX = 0;
        playerChar.init(
                       &PlayerAnimation::idleDirLeft,
                       &PlayerAnimation::walkDirLeft,
                       &PlayerAnimation::strikeDirLeft,
                       &PlayerAnimation::die,
                       Fixed2D4(Fix4(20,0),Fix4(50,0)),0);
        spawnAllySwordFighter(-8,4);
        spawnAllySwordFighter(-8,-4);
        spawnAllySwordFighter(-16,0);
        setStatus(STATUS_INTRO);
        script[0].onEnter(0);
        camX = 0;
        camY = INTRO_CAMY_START - 60;
    }

    void initialize() {
        MeleeFighterFSM::init();
        AllyFighterFSM::init();
        //spawnEnemySwordFighter(40,50);
        //spawnEnemySwordFighter(44,48);
        //spawnEnemySwordFighter(50,55);
        Time::prevMillis = Time::millis;
        buffer.setClearBackground(true,RGB565(0xA4,0xBB,0xF4));
        Sound::init();
        atlas = Texture<uint16_t>(ImageAsset::atlas);
        forest = Texture<uint16_t>(ImageAsset::forest_background);
        ground0 = Texture<uint16_t>(ImageAsset::ground_foreground0);
        ground1 = Texture<uint16_t>(ImageAsset::ground_foreground1);
        ground2 = Texture<uint16_t>(ImageAsset::ground_foreground2);
        ground3 = Texture<uint16_t>(ImageAsset::ground_foreground3);
        uiPanelBackground = Texture<uint16_t>(ImageAsset::ui_panel_background);

        restart();
    }
}
