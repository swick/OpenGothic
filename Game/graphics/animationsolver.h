#pragma once

#include <Tempest/Matrix4x4>
#include <vector>

#include "world/gsoundeffect.h"
#include "game/inventory.h"
#include "game/constants.h"
#include "meshobjects.h"
#include "pfxobjects.h"
#include "animation.h"
#include "mdlvisual.h"

class Skeleton;
class Overlay;
class Pose;
class Interactive;
class World;

class AnimationSolver final {
  public:
    AnimationSolver();

    enum Anim : uint16_t {
      NoAnim,
      Idle,
      DeadA,
      DeadB,
      GuardL,
      GuardH,
      Sit,
      Sleep,
      GuardSleep,
      MagicSleep,
      IdleLoopLast=MagicSleep,

      Pray,
      PrayRand,
      Talk,
      GuardLChLeg,
      GuardLScratch,
      GuardLStrectch,
      Lookaround,
      Perception,
      Chair1,
      Chair2,
      Chair3,
      Chair4,
      Roam1,
      Roam2,
      Roam3,
      Food1,
      Food2,
      FoodHuge1,
      Potition1,
      Potition2,
      Potition3,
      PotitionFast,
      Joint1,
      Map1,
      MapSeal1,
      Firespit1,
      Meat1,
      Rice1,
      Rice2,
      Dance1,
      Dance2,
      Dance3,
      Dance4,
      Dance5,
      Dance6,
      Dance7,
      Dance8,
      Dance9,
      Dialog1,
      Dialog2,
      Dialog3,
      Dialog4,
      Dialog5,
      Dialog6,
      Dialog7,
      Dialog8,
      Dialog9,
      Dialog10,
      Dialog11,
      Dialog12,
      Dialog13,
      Dialog14,
      Dialog15,
      Dialog16,
      Dialog17,
      Dialog18,
      Dialog19,
      Dialog20,
      Dialog21,

      DialogFirst =Dialog1,
      DialogLastG2=Dialog11,
      DialogLastG1=Dialog21,

      Fear1,
      Fear2,
      Fear3,
      Plunder,
      Pee,
      Eat,
      IdleLast=Eat,
      Warn,
      UnconsciousA,
      UnconsciousB,

      Move,
      MoveBack,
      MoveL,
      MoveR,
      RotL,
      RotR,
      Jump,
      JumpUpLow,
      JumpUpMid,
      JumpUp,
      Fall,
      FallDeep,
      SlideA,
      SlideB,
      Training,
      Interact,

      Atack,
      AtackL,
      AtackR,
      AtackBlock,
      AtackFinish,
      StumbleA,
      StumbleB,
      GotHit,
      AimBow,

      MagNoMana,
      MagFib,
      MagWnd,
      MagHea,
      MagPyr,
      MagFea,
      MagTrf,
      MagSum,
      MagMsd,
      MagStm,
      MagFrz,
      MagSle,
      MagWhi,
      MagSck,
      MagFbt,

      MagFirst=MagFib,
      MagLast =MagFbt
      };

    struct Overlay final {
      const Skeleton* sk  =nullptr;
      uint64_t        time=0;
      };

    struct Sequence final {
      Sequence()=default;
      Sequence(const Animation::Sequence* s):Sequence(nullptr,s){}
      Sequence(const Animation::Sequence* s,const Animation::Sequence* s1)
        :cls(s1 ? s1->animCls : Animation::UnknownAnim),l0(s),l1(s1){}

      Animation::AnimClass       cls=Animation::UnknownAnim;
      const Animation::Sequence* l0=nullptr;
      const Animation::Sequence* l1=nullptr;

      const char* name() const { return l1->name.c_str(); }

      bool  isFinished(uint64_t t) const { return l1->isFinished(t); }
      bool  isAtackFinished(uint64_t t) const { return l1->isAtackFinished(t); }
      bool  isParWindow(uint64_t t) const { return l1->isParWindow(t); }
      void  processEvents(uint64_t& barrier,uint64_t sTime,uint64_t now, Animation::EvCount& ev) const {
        if(l1)
          l1->processEvents(barrier,sTime,now,ev);
        if(l0)
          l0->processEvents(barrier,sTime,now,ev);
        barrier = now;
        }
      float totalTime() const { return l1->totalTime(); }
      bool  isFly() const { return l1->isFly(); }

      operator bool () const { return l1!=nullptr; }

      bool operator == (std::nullptr_t) const { return l1==nullptr; }
      bool operator != (std::nullptr_t) const { return l1!=nullptr; }

      bool operator == (const Sequence& s) const { return l0==s.l0 && l1==s.l1; }
      bool operator != (const Sequence& s) const { return l0!=s.l0 || l1!=s.l1; }
      };

    void                           save(Serialize& fout);
    void                           load(Serialize& fin, Npc &owner);

    void                           setPos   (const Tempest::Matrix4x4 &m);
    void                           setVisual(const Skeleton *visual, uint64_t tickCount, WeaponState ws, WalkBit walk, Interactive* inter, World &owner);
    void                           setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body);
    ZMath::float3                  animMoveSpeed(Anim a, uint64_t tickCount, uint64_t dt, WeaponState weaponSt) const;

    void                           emitSfx(Npc &npc, uint64_t tickCount);
    void                           updateAnimation(uint64_t tickCount);
    bool                           stopAnim(const std::string& ani);
    void                           resetAni();

    void                           addOverlay(const Skeleton *sk, uint64_t time, uint64_t tickCount, WalkBit wlk, Interactive *inter, World &owner);
    void                           delOverlay(const char *sk);
    void                           delOverlay(const Skeleton *sk);

    bool                           setAnim(Anim a, uint64_t tickCount, uint64_t fghLastEventTime,
                                           WeaponState weaponSt,
                                           WalkBit walk, Interactive *inter, World &owner);

    bool                           isFlyAnim(uint64_t tickCount) const;
    void                           invalidateAnim(const Sequence ani, const Skeleton *sk, World &owner, uint64_t tickCount);

    Sequence                       solveAnim(const char *format, WeaponState st) const;
    Sequence                       solveAnim(Anim a, WeaponState st0, Anim cur, WeaponState st, WalkBit wlk, Interactive *inter) const;

    AnimationSolver::Anim          animByName  (const std::string &name) const;
    Sequence                       animSequence(const char *name) const;
    Sequence                       layredSequence(const char *name, const char *base) const;
    Sequence                       layredSequence(const char *name, const char *base, WeaponState st) const;

    void                           processEvents(uint64_t& barrier, uint64_t now, Animation::EvCount &ev) const;

    MdlVisual                      visual;

    std::shared_ptr<Pose>          skInst;
    Sequence                       animSq;
    uint64_t                       sAnim    =0;

    Anim                           current    = NoAnim;
    WeaponState                    currentW   = WeaponState::NoWeapon;
    WalkBit                        currentWlk = WalkBit::WM_Walk;
    Anim                           prevAni    = NoAnim;
    Anim                           lastIdle   = Idle;

    GSoundEffect                   soundSlot;

  private:
    Sequence                       solveMag    (const char *format,Anim spell) const;
    Sequence                       solveDead   (const char *format1,const char *format2) const;
    Sequence                       solveItemUse(const char *format,const char* scheme) const;

    std::vector<Overlay>           overlay;
  };
