#include "animationsolver.h"

#include "skeleton.h"
#include "pose.h"
#include "posepool.h"
#include "world/interactive.h"
#include "world/world.h"
#include "game/serialize.h"

#include "resources.h"

using namespace Tempest;

AnimationSolver::AnimationSolver() {
  }

void AnimationSolver::save(Serialize &fout) {
  visual.save(fout);

  fout.write(uint32_t(overlay.size()));
  for(auto& i:overlay){
    fout.write(i.sk->name(),i.time);
    }

  fout.write(sAnim);
  fout.write(uint16_t(current),uint16_t(prevAni),uint16_t(lastIdle));

  fout.write(uint8_t(animSq.cls));
  fout.write(animSq.l0 ? animSq.l0->name : "");
  fout.write(animSq.l1 ? animSq.l1->name : "");
  }

void AnimationSolver::load(Serialize &fin,Npc& npc) {
  uint32_t sz=0;
  std::string s;

  visual.load(fin,npc);

  fin.read(sz);
  overlay.resize(sz);
  for(auto& i:overlay){
    fin.read(s,i.time);
    i.sk = Resources::loadSkeleton(s);
    }
  for(size_t i=0;i<overlay.size();){
    if(overlay[i].sk){
      ++i;
      } else {
      overlay[i] = overlay.back();
      overlay.pop_back();
      }
    }
  fin.read(sAnim);
  fin.read(reinterpret_cast<uint16_t&>(current),reinterpret_cast<uint16_t&>(prevAni),reinterpret_cast<uint16_t&>(lastIdle));

  fin.read(reinterpret_cast<uint8_t&>(animSq.cls));
  fin.read(s);
  Sequence l0 = animSequence(s.c_str());
  animSq.l0 = l0.l1;

  fin.read(s);
  Sequence l1 = animSequence(s.c_str());
  animSq.l1 = l1.l1;
  invalidateAnim(animSq,visual.skeleton,npc.world(),sAnim);
  }

void AnimationSolver::setPos(const Matrix4x4 &m) {
  visual.setPos(m);
  }

void AnimationSolver::setVisual(const Skeleton *v,uint64_t tickCount,
                                WeaponState ws,WalkBit walk,Interactive* inter,World& owner) {
  visual.setVisual(v);
  current=NoAnim;
  setAnim(Idle,tickCount,tickCount,ws,walk,inter,owner);

  invalidateAnim(animSq,visual.skeleton,owner,tickCount);
  }

void AnimationSolver::setVisualBody(MeshObjects::Mesh&& h, MeshObjects::Mesh &&body) {
  visual.setVisualBody(std::move(h),std::move(body));
  }

bool AnimationSolver::setAnim(Anim a,uint64_t tickCount,uint64_t fghLastEventTime,
                              WeaponState weaponSt,
                              WalkBit walk,Interactive* inter,World& owner) {
  if(Npc::Anim::DeadB<a && a<Npc::Anim::IdleLoopLast && weaponSt!=WeaponState::NoWeapon)
    a = Npc::Anim::Idle;
  if(animSq!=nullptr){
    if(current==a && currentW==weaponSt && currentWlk==walk && animSq.cls==Animation::Loop)
      return true;
    if((animSq.cls==Animation::Transition &&
        current!=RotL && current!=RotR && current!=MoveL && current!=MoveR && // no idea why this animations maked as Transition
        !(current==Move && a==Jump)) && // allow to jump at any point of run animation
       !animSq.isFinished(fghLastEventTime-sAnim))
      return false;
    if(current==Atack && !animSq.isAtackFinished(fghLastEventTime-sAnim))
      return false;
    if(MagFirst<=current && current<=MagLast && !animSq.isFinished(tickCount-sAnim))
      return false;
    }
  auto ani = solveAnim(a,currentW,current,weaponSt,walk,inter);
  if(ani==nullptr) {
    a   = Idle;
    ani = solveAnim(Idle,WeaponState::NoWeapon,Idle,WeaponState::NoWeapon,WalkBit::WM_Run,nullptr);
    }

  if(ani==nullptr)
    return false;

  if(prevAni!=current)
    prevAni = current;

  current    = a;
  currentW   = weaponSt;
  currentWlk = walk;

  if(current<=IdleLoopLast && (weaponSt==WeaponState::NoWeapon || weaponSt==WeaponState::Fist))
    lastIdle=current;
  if(ani==animSq) {
    if(animSq.cls==Animation::Transition){
      invalidateAnim(ani,visual.skeleton,owner,tickCount); // restart anim
      }
    return true;
    }
  owner.takeSoundSlot(std::move(soundSlot));
  invalidateAnim(ani,visual.skeleton,owner,tickCount);
  return true;
  }

bool AnimationSolver::isFlyAnim(uint64_t tickCount) const {
  if(animSq==nullptr)
    return false;
  return animSq.isFly() && !animSq.isFinished(tickCount-sAnim) &&
         current!=Fall && current!=FallDeep;
  }

void AnimationSolver::invalidateAnim(const Sequence ani,const Skeleton* sk,World& owner,uint64_t tickCount) {
  animSq = ani;
  sAnim  = tickCount;
  if(ani.l0)
    skInst = owner.view()->get(sk,ani.l0,ani.l1,sAnim,skInst); else
    skInst = owner.view()->get(sk,ani.l1,sAnim,skInst);
  }

void AnimationSolver::addOverlay(const Skeleton* sk,uint64_t time,uint64_t tickCount,
                                 WalkBit wlk,Interactive* inter,World& owner) {
  if(sk==nullptr)
    return;
  if(time!=0)
    time+=tickCount;

  Overlay ov = {sk,time};
  overlay.push_back(ov);
  if(animSq!=nullptr) {
    auto ani=animSequence(animSq.name());
    invalidateAnim(ani,visual.skeleton,owner,tickCount);
    } else {
    // fallback
    setAnim(Idle,tickCount,tickCount,WeaponState::NoWeapon,wlk,inter,owner);
    }
  }

void AnimationSolver::delOverlay(const char *sk) {
  if(overlay.size()==0)
    return;
  auto skelet = Resources::loadSkeleton(sk);
  delOverlay(skelet);
  }

void AnimationSolver::delOverlay(const Skeleton *sk) {
  for(size_t i=0;i<overlay.size();++i)
    if(overlay[i].sk==sk){
      overlay.erase(overlay.begin()+int(i));
      return;
      }
  }

void AnimationSolver::updateAnimation(uint64_t tickCount) {
  for(size_t i=0;i<overlay.size();){
    auto& ov = overlay[i];
    if(ov.time!=0 && ov.time<tickCount)
      overlay.erase(overlay.begin()+int(i)); else
      ++i;
    }

  if(skInst!=nullptr){
    uint64_t dt = tickCount - sAnim;
    skInst->update(dt);
    visual.updateAnimation(*skInst);
    }
  }

void AnimationSolver::emitSfx(Npc& npc,uint64_t tickCount) {
  if(skInst!=nullptr){
    uint64_t dt = tickCount - sAnim;
    skInst->emitSfx(npc,dt);
    }
  }

bool AnimationSolver::stopAnim(const std::string &ani) {
  if(animSq!=nullptr && (animSq.l1->name==ani || ani.empty())){
    resetAni();
    return true;
    }
  return false;
  }

void AnimationSolver::resetAni() {
  if(current<=Anim::IdleLoopLast)
    return;
  current = Anim::NoAnim;
  animSq  = Sequence();
  }

AnimationSolver::Sequence AnimationSolver::solveAnim( Anim a,   WeaponState st0,
                                                      Anim cur, WeaponState st,
                                                      WalkBit wlkMode,
                                                      Interactive* inter) const {
  if(visual.skeleton==nullptr)
    return nullptr;

  if(st0==WeaponState::NoWeapon && st!=WeaponState::NoWeapon){
    if(a==Anim::Move && cur==a)
      return layredSequence("S_%sRUNL","T_MOVE_2_%sMOVE",st);
    return solveAnim("T_%s_2_%sRUN",st);
    }

  if(st==WeaponState::NoWeapon && st0!=WeaponState::NoWeapon &&
     a!=Anim::UnconsciousA && a!=Anim::UnconsciousB &&
     a!=Anim::DeadA        && a!=Anim::DeadB){
    if(a==Anim::Move && cur==a)
      return layredSequence("S_%sRUNL","T_%sMOVE_2_MOVE",st0);
    return solveAnim("T_%sRUN_2_%s",st0);
    }

  if((cur==Anim::UnconsciousA || cur==Anim::UnconsciousB) && a==Anim::DeadA)
    return solveDead("T_WOUNDED_2_DEAD","T_WOUNDEDB_2_DEADB");
  if(cur!=Anim::DeadA && a==Anim::DeadA)
    return solveDead("T_DEAD","T_DEADB");
  if(a==Anim::DeadA)
    return solveDead("S_DEAD","S_DEADB");

  if((cur==Anim::UnconsciousA || cur==Anim::UnconsciousB) && a==Anim::DeadB)
    return solveDead("T_WOUNDEDB_2_DEADB","T_WOUNDED_2_DEAD");
  if(cur!=Anim::DeadB && a==Anim::DeadB)
    return solveDead("T_DEADB","T_DEAD");
  if(a==Anim::DeadB)
    return solveDead("S_DEADB","S_DEAD");

  if(cur!=Anim::UnconsciousA && a==Anim::UnconsciousA)
    return animSequence("T_STAND_2_WOUNDED");
  if(cur==Anim::UnconsciousA && a==Anim::Idle)
    return animSequence("T_WOUNDED_2_STAND");
  if(a==Anim::UnconsciousA)
    return animSequence("S_WOUNDED");

  if(inter!=nullptr) {
    if(cur!=Interact && a==Interact){
      auto ret = inter->anim(*this,Interactive::In);
      inter->nextState();
      return ret;
      }
    if(cur==Interact && a==Interact){
      auto ret = inter->anim(*this,Interactive::In);
      inter->nextState();
      return ret;
      }
    if(inter->stateId()==-1){
      auto ret = inter->anim(*this,Interactive::Out);
      return ret;
      }
    }
  /*
  // TODO: interactives dettach
  if(inter==nullptr && cur==Interact){
    auto ret = inter->anim(*this,Interactive::Out);
    inter->prevState();
    return ret;
    }*/

  if(bool(wlkMode & WalkBit::WM_Swim)) {
    if(a==Anim::Move)
      return solveAnim("S_SWIMF",st);
    if(a==Anim::MoveL)
      return solveAnim("T_SWIMTURNL",st);
    if(a==Anim::MoveR)
      return solveAnim("T_SWIMTURNR",st);
    return solveAnim("S_SWIM",st);
    }

  if(st==WeaponState::Fist) {
    if(a==Anim::Atack && cur==Move) {
      if(auto a=animSequence("T_FISTATTACKMOVE"))
        return layredSequence("S_%sRUNL","T_%sATTACKMOVE",st);
      }
    if(a==Anim::Atack)
      return animSequence("S_FISTATTACK");
    if(a==Anim::AtackBlock)
      return animSequence("T_FISTPARADE_0");
    }
  else if(st==WeaponState::W1H || st==WeaponState::W2H) {
    if(a==Anim::Atack && cur==Move)
      return layredSequence("S_%sRUNL","T_%sATTACKMOVE",st);
    if(a==Anim::AtackL)
      return solveAnim("T_%sATTACKL",st);
    if(a==Anim::AtackR)
      return solveAnim("T_%sATTACKR",st);
    if(a==Anim::Atack || a==Anim::AtackL || a==Anim::AtackR)
      return solveAnim("S_%sATTACK",st);
    if(a==Anim::AtackBlock) {
      AnimationSolver::Sequence s;
      switch(std::rand()%3){
        case 0: s = solveAnim("T_%sPARADE_0",   st); break;
        case 1: s = solveAnim("T_%sPARADE_0_A2",st); break;
        case 2: s = solveAnim("T_%sPARADE_0_A3",st); break;
        }
      if(s==nullptr)
        s = solveAnim("T_%sPARADE_0",st);
      return s;
      }
    if(a==Anim::AtackFinish)
      return solveAnim("T_1HSFINISH",st);
    }
  else if(st==WeaponState::Bow || st==WeaponState::CBow){
    if(a==Anim::AimBow && cur!=Anim::AimBow && cur!=Anim::Atack)
      return solveAnim("T_%sRUN_2_%sAIM",st);
    if(a==Anim::Atack && cur==Anim::AimBow)
      return solveAnim("T_%sRELOAD",st);
    if(a!=Anim::AimBow && cur==Anim::AimBow)
      return solveAnim("T_BOWAIM_2_BOWRUN",st);
    if(a==Anim::AimBow)
      return solveAnim("S_%sSHOOT",st);

    if(a==Anim::Atack)
      return solveAnim("T_%sRELOAD",st);
    }

  if((cur==Anim::Idle || cur==Anim::NoAnim) && a==Anim::Idle) {
    if(bool(wlkMode&WalkBit::WM_Walk) && st==WeaponState::NoWeapon) {
      if(auto sq=solveAnim("S_%sWALK",st))
        return sq;
      }
    if(auto sq=solveAnim("S_%sRUN",st)) // there are no RUN for 'Waran'
      return sq;
    return solveAnim("S_%sWALK", st);
    }
  if(cur!=Anim::Move && a==Anim::Move) {
    Sequence sq;
    if(bool(wlkMode&WalkBit::WM_Water))
      sq = solveAnim("T_%sWALK_2_%sWALKWL",st); else
    if(bool(wlkMode&WalkBit::WM_Walk))
      sq = solveAnim("T_%sWALK_2_%sWALKL",st); else
      sq = solveAnim("T_%sRUN_2_%sRUNL",  st);
    if(sq)
      return sq;
    }
  if(cur==Anim::Move && a==cur){
    if(bool(wlkMode&WalkBit::WM_Water))
      return solveAnim("S_%sWALKWL",st); else
    if(bool(wlkMode&WalkBit::WM_Walk))
      return solveAnim("S_%sWALKL",st); else
      return solveAnim("S_%sRUNL", st);
    }
  if(cur==Anim::Move && a==Anim::Idle) {
    if(bool(wlkMode&WalkBit::WM_Water))
      return solveAnim("T_%sWALKWL_2_%sWALK",st); else
    if(bool(wlkMode&WalkBit::WM_Walk))
      return solveAnim("T_%sWALKL_2_%sWALK",st); else
      return solveAnim("T_%sRUNL_2_%sRUN",st);
    }

  if(a==Anim::RotL){
    if(bool(wlkMode&WalkBit::WM_Water)){
      if(auto ani=solveAnim("T_%sWALKWTURNL",st))
        return ani;
      }
    if(bool(wlkMode&WalkBit::WM_Walk)){
      if(auto ani=solveAnim("T_%sWALKTURNL",st))
        return ani;
      }
    return solveAnim("T_%sRUNTURNL",st);
    }
  if(a==Anim::RotR){
    if(bool(wlkMode&WalkBit::WM_Water)){
      if(auto ani=solveAnim("T_%sWALKWTURNR",st))
        return ani;
      }
    if(bool(wlkMode&WalkBit::WM_Walk)){
      if(auto ani=solveAnim("T_%sWALKTURNR",st))
        return ani;
      }
    return solveAnim("T_%sRUNTURNR",st);
    }
  if(a==Anim::MoveL) {
    if(bool(wlkMode&WalkBit::WM_Water)){
      if(auto ani=solveAnim("T_%sWALKWSTRAFEL",st))
        return ani;
      }
    return solveAnim("T_%sRUNSTRAFEL",st);
    }
  if(a==Anim::MoveR) {
    if(bool(wlkMode&WalkBit::WM_Water)){
      if(auto ani=solveAnim("T_%sWALKWSTRAFER",st))
        return ani;
      }
    return solveAnim("T_%sRUNSTRAFER",st);
    }
  if(a==Anim::MoveBack)
    return solveAnim("T_%sJUMPB",st);

  if(cur==Anim::Move && a==Jump)
    return animSequence("T_RUNL_2_JUMP");
  if(cur==Anim::Idle && a==Anim::Jump)
    return animSequence("T_STAND_2_JUMP");
  if(cur==Anim::Jump && a==Anim::Idle)
    return animSequence("T_JUMP_2_STAND");
  if(a==Anim::Jump)
    return animSequence("S_JUMP");
  if(cur==Anim::Fall && a==Move)
    return animSequence("T_RUN_2_RUNL");

  if(cur==Anim::Idle && a==Anim::JumpUpLow)
    return animSequence("T_STAND_2_JUMPUPLOW");
  if(cur==Anim::JumpUpLow && a==Anim::Idle)
    return animSequence("T_JUMPUPLOW_2_STAND");
  if(a==Anim::JumpUpLow)
    return animSequence("S_JUMPUPLOW");

  if(cur==Anim::Idle && a==Anim::JumpUpMid)
    return animSequence("T_STAND_2_JUMPUPMID");
  if(cur==Anim::JumpUpMid && a==Anim::Idle)
    return animSequence("T_JUMPUPMID_2_STAND");
  if(a==Anim::JumpUpMid)
    return animSequence("S_JUMPUPMID");

  if(cur==Anim::Idle && a==Anim::JumpUp)
    return animSequence("T_STAND_2_JUMPUP");
  if(a==Anim::JumpUp)
    return animSequence("S_JUMPUP");

  if(cur==Anim::Idle && a==Anim::GuardL)
    return animSequence("T_STAND_2_LGUARD");
  if(a==Anim::GuardL)
    return animSequence("S_LGUARD");

  if(cur==Anim::Idle && a==Anim::GuardH)
    return animSequence("T_STAND_2_HGUARD");
  if(a==Anim::GuardH)
    return animSequence("S_HGUARD");

  if(cur==Anim::Idle && a==Anim::Talk)
    return animSequence("T_STAND_2_TALK");
  if(a==Anim::Talk)
    return animSequence("S_TALK");

  if(cur==Anim::Idle && a==Anim::Eat){
    if(auto ani=animSequence("T_STAND_2_EAT"))
      return ani;
    }
  if(cur==Anim::Eat && a==Anim::Idle)
    return animSequence("T_EAT_2_STAND");
  if(a==Anim::Eat){
    if(auto ani=animSequence("S_EAT"))
      return ani;
    return animSequence("S_FOOD_S0");
    }

  if(cur<=Anim::IdleLast && a==Anim::Sleep)
    return animSequence("T_STAND_2_SLEEP");
  if(cur==Anim::Sleep && a<=Anim::IdleLast)
    return animSequence("T_SLEEP_2_STAND");
  if(a==Anim::Sleep)
    return animSequence("S_SLEEP");

  if(cur!=Anim::UnconsciousB && a==Anim::UnconsciousB)
    return animSequence("T_STAND_2_WOUNDEDB");
  if(cur==Anim::UnconsciousB && a==Anim::Idle)
    return animSequence("T_WOUNDEDB_2_STAND");
  if(a==Anim::UnconsciousB)
    return animSequence("S_WOUNDEDB");

  if(cur!=Anim::GuardSleep && a==Anim::GuardSleep)
    return animSequence("T_STAND_2_GUARDSLEEP");
  if(cur==Anim::GuardSleep && a<=Anim::IdleLast && a!=Anim::GuardSleep)
    return animSequence("T_GUARDSLEEP_2_STAND");
  if(a==Anim::GuardSleep)
    return animSequence("S_GUARDSLEEP");

  if(cur==Anim::Idle && a==Anim::Pee)
    return animSequence("T_STAND_2_PEE");
  if(cur==Anim::Pee && a==Anim::Idle)
    return animSequence("T_PEE_2_STAND");
  if(a==Anim::Pee)
    return animSequence("S_PEE");

  if(cur==Anim::Idle && (Anim::MagFirst<=a && a<=Anim::MagLast))
    return solveMag("T_MAGRUN_2_%sSHOOT",a);
  if((Anim::MagFirst<=cur && cur<=Anim::MagLast) && a==Anim::Idle)
    return solveMag("T_%sSHOOT_2_STAND",cur);
  if(Anim::MagFirst<=a && a<=Anim::MagLast)
    return solveMag("S_%sSHOOT",a);
  if(a==Anim::MagNoMana)
    return animSequence("T_CASTFAIL");

  if(cur==Anim::Idle && a==Anim::Sit)
    return animSequence("T_STAND_2_SIT");
  if(cur==Anim::Sit && a==Anim::Idle)
    return animSequence("T_SIT_2_STAND");
  if(a==Anim::Sit)
    return animSequence("S_SIT");

  if(a==Anim::GuardLChLeg)
    return animSequence("T_LGUARD_CHANGELEG");
  if(a==Anim::GuardLScratch)
    return animSequence("T_LGUARD_SCRATCH");
  if(a==Anim::GuardLStrectch)
    return animSequence("T_LGUARD_STRETCH");
  if(a==Anim::Perception)
    return animSequence("T_PERCEPTION");
  if(a==Anim::Lookaround)
    return animSequence("T_HGUARD_LOOKAROUND");
  if(a==Anim::Training)
    return animSequence("T_1HSFREE");
  if(a==Anim::Warn)
    return animSequence("T_WARN");

  if(a==Anim::Fall)
    return animSequence("S_FALLDN");
  //if(cur==Fall && a==Anim::FallDeep)
  //  return animSequence("T_FALL_2_FALLEN");
  if(a==Anim::FallDeep)
    return animSequence("S_FALL");
  if(a==Anim::SlideA)
    return animSequence("S_SLIDE");
  if(a==Anim::SlideB)
    return animSequence("S_SLIDEB");

  if(a==Anim::StumbleA && current==Anim::Move)
    return layredSequence("S_RUNL","T_STUMBLE");
  if(a==Anim::StumbleA)
    return animSequence("T_STUMBLE");

  if(a==Anim::StumbleB && current==Anim::Move)
    return layredSequence("S_RUNL","T_STUMBLEB");
  if(a==Anim::StumbleB)
    return animSequence("T_STUMBLEB");

  if(a==Anim::GotHit && current==Anim::Move)
    return layredSequence("S_RUNL","T_GOTHIT");
  if(a==Anim::GotHit)
    return animSequence("T_GOTHIT");

  if(a==Anim::Chair1)
    return animSequence("R_CHAIR_RANDOM_1");
  if(a==Anim::Chair2)
    return animSequence("R_CHAIR_RANDOM_2");
  if(a==Anim::Chair3)
    return animSequence("R_CHAIR_RANDOM_3");
  if(a==Anim::Chair4)
    return animSequence("R_CHAIR_RANDOM_4");

  if(a==Anim::Roam1)
    return animSequence("R_ROAM1");
  if(a==Anim::Roam2)
    return animSequence("R_ROAM2");
  if(a==Anim::Roam3)
    return animSequence("R_ROAM3");

  if(a==Anim::Pray && cur<=Anim::IdleLast && cur!=Anim::Pray && cur!=Anim::PrayRand)
    return animSequence("T_STAND_2_PRAY");
  if(a==Anim::Pray)
    return animSequence("S_PRAY");
  if(a==Anim::PrayRand)
    return animSequence("T_PRAY_RANDOM");

  static const std::pair<const char*,Npc::Anim> schemes[]={
    {"FOOD",       Npc::Anim::Food1},
    {"FOODHUGE",   Npc::Anim::FoodHuge1},
    {"POTION",     Npc::Anim::Potition1},
    {"POTIONFAST", Npc::Anim::PotitionFast},
    {"RICE",       Npc::Anim::Rice1},
    {"MEAT",       Npc::Anim::Meat1},
    {"JOINT",      Npc::Anim::Joint1},
    {"MAP",        Npc::Anim::Map1},
    {"MAPSEALED",  Npc::Anim::MapSeal1},
    {"FIRESPIT",   Npc::Anim::Firespit1}
    };
  for(auto& i:schemes){
    if(cur<Anim::IdleLast && a==i.second)
      return solveItemUse("T_%s_STAND_2_S0",i.first);
    if(cur==i.second && a<Anim::IdleLast)
      return solveItemUse("T_%s_S0_2_STAND",i.first);
    if(a==i.second)
      return solveItemUse("S_%s_S0",i.first);
    }

  if(a==Anim::Plunder)
    return animSequence("T_PLUNDER");
  if(a==Anim::Food1)
    return animSequence("T_FOOD_RANDOM_1");
  if(a==Anim::Food2)
    return animSequence("T_FOOD_RANDOM_2");
  if(a==Anim::FoodHuge1)
    return animSequence("T_FOODHUGE_RANDOM_1");
  if(a==Anim::Potition1)
    return animSequence("T_POTION_RANDOM_1");
  if(a==Anim::Potition2)
    return animSequence("T_POTION_RANDOM_2");
  if(a==Anim::Potition3)
    return animSequence("T_POTION_RANDOM_3");

  if(Anim::Dance1<=a && a<=Anim::Dance9){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DANCE_%02d",a-Anim::Dance1+1);
    return animSequence(buf);
    }

  if(Anim::Dialog1<=a && a<=Anim::Dialog21){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",int(a-Anim::Dialog1+1));
    return animSequence(buf);
    }

  if(Anim::Fear1<=a && a<=Anim::Fear3){
    char buf[32]={};
    if(Anim::Fear1<=cur && cur<=Anim::Fear3)
      std::snprintf(buf,sizeof(buf),"T_STAND_2_FEAR_VICTIM%d",int(a-Anim::Fear1+1)); else
      std::snprintf(buf,sizeof(buf),"S_FEAR_VICTIM%d",int(a-Anim::Fear1+1));
    return animSequence(buf);
    }

  if(Anim::MagicSleep==a && cur!=Anim::MagicSleep)
    return animSequence("T_STAND_2_VICTIM_SLE");
  if(cur==Anim::MagicSleep && a!=Anim::MagicSleep)
    return animSequence("T_VICTIM_SLE_2_STAND");
  if(a==Anim::MagicSleep)
    return animSequence("S_VICTIM_SLE");

  // FALLBACK
  if(a==Anim::Move)
    return solveAnim("S_%sRUNL",st);
  if(a==Anim::Idle)  {
    if(auto idle=solveAnim("S_%sRUN",st))
      return idle;
    return solveAnim("S_%sWALK",st);
    }
  return nullptr;
  }

AnimationSolver::Sequence AnimationSolver::solveAnim(const char *format, WeaponState st) const {
  static const char* weapon[] = {
    "",
    "FIST",
    "1H",
    "2H",
    "BOW",
    "CBOW",
    "MAG"
    };
  char name[128]={};
  std::snprintf(name,sizeof(name),format,weapon[int(st)],weapon[int(st)]);
  if(auto ret=animSequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"");
  if(auto ret=animSequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"FIST");
  return animSequence(name);
  }

AnimationSolver::Sequence AnimationSolver::solveMag(const char *format,Anim spell) const {
  static const char* mg[]={"FIB", "WND", "HEA", "PYR", "FEA", "TRF", "SUM", "MSD", "STM", "FRZ", "SLE", "WHI", "SCK", "FBT"};

  char name[128]={};
  std::snprintf(name,sizeof(name),format,mg[spell-Anim::MagFirst]);
  if(auto a=animSequence(name))
    return a;
  std::snprintf(name,sizeof(name),format,mg[0]);
  return animSequence(name);
  }

AnimationSolver::Sequence AnimationSolver::solveDead(const char *format1, const char *format2) const {
  if(auto a=animSequence(format1))
    return a;
  return animSequence(format2);
  }

AnimationSolver::Sequence AnimationSolver::solveItemUse(const char *format, const char *scheme) const {
  char buf[128]={};
  std::snprintf(buf,sizeof(buf),format,scheme);
  return animSequence(buf);
  }

AnimationSolver::Sequence AnimationSolver::animSequence(const char *name) const {
  if(name==nullptr || name[0]=='\0')
    return Sequence();
  for(size_t i=overlay.size();i>0;){
    --i;
    if(auto s = overlay[i].sk->sequence(name))
      return s;
    }
  return visual.skeleton ? visual.skeleton->sequence(name) : nullptr;
  }

AnimationSolver::Sequence AnimationSolver::layredSequence(const char *name,const char* base) const {
  auto a = animSequence(name);
  auto b = animSequence(base);
  return Sequence(a.l1,b.l1);
  }

AnimationSolver::Sequence AnimationSolver::layredSequence(const char *name, const char *base, WeaponState st) const {
  auto a = solveAnim(name,st);
  auto b = solveAnim(base,st);
  return Sequence(a.l1,b.l1);
  }

void AnimationSolver::processEvents(uint64_t &barrier, uint64_t now, Animation::EvCount& ev) const {
  animSq.processEvents(barrier,sAnim,now,ev);
  }

AnimationSolver::Anim AnimationSolver::animByName(const std::string &name) const {
  if(name=="T_HEASHOOT_2_STAND" || name=="T_LGUARD_2_STAND" || name=="T_HGUARD_2_STAND" ||
     name=="T_EAT_2_STAND"      || name=="T_SLEEP_2_STAND"  || name=="T_GUARDSLEEP_2_STAND" ||
     name=="T_SIT_2_STAND"      || name=="T_PEE_2_STAND"    || name=="T_VICTIM_SLE_2_STAND" ||
     name=="T_TALK_2_STAND")
    return Anim::Idle;

  if(name=="T_STAND_2_LGUARD" || name=="S_LGUARD")
    return Anim::GuardL;
  if(name=="T_STAND_2_HGUARD")
    return Anim::GuardH;
  if(name=="T_STAND_2_TALK" || name=="S_TALK")
    return Anim::Talk;
  if(name=="T_PERCEPTION")
    return Anim::Perception;
  if(name=="T_HGUARD_LOOKAROUND")
    return Anim::Lookaround;
  if(name=="T_STAND_2_EAT" || name=="S_EAT")
    return Anim::Eat;
  if(name=="T_STAND_2_SLEEP" || name=="S_SLEEP")
    return Anim::Sleep;
  if(name=="T_STAND_2_GUARDSLEEP" || name=="S_GUARDSLEEP")
    return Anim::GuardSleep;
  if(name=="T_STAND_2_SIT" || name=="S_SIT")
    return Anim::Sit;
  if(name=="T_LGUARD_CHANGELEG")
    return Anim::GuardLChLeg;
  if(name=="T_LGUARD_STRETCH")
    return Anim::GuardLStrectch;
  if(name=="T_LGUARD_SCRATCH")
    return Anim::GuardLScratch;
  if(name=="T_1HSFREE")
    return Anim::Training;
  if(name=="T_MAGRUN_2_HEASHOOT" || name=="S_HEASHOOT")
    return Anim::MagHea;
  if(name=="T_WARN")
    return Anim::Warn;
  if(name=="R_CHAIR_RANDOM_1")
    return Anim::Chair1;
  if(name=="R_CHAIR_RANDOM_2")
    return Anim::Chair2;
  if(name=="R_CHAIR_RANDOM_3")
    return Anim::Chair3;
  if(name=="R_CHAIR_RANDOM_4")
    return Anim::Chair4;
  if(name=="R_ROAM1")
    return Anim::Roam1;
  if(name=="R_ROAM2")
    return Anim::Roam2;
  if(name=="R_ROAM3")
    return Anim::Roam3;
  if(name=="T_FOOD_RANDOM_1")
    return Anim::Food1;
  if(name=="T_FOOD_RANDOM_2")
    return Anim::Food2;
  if(name=="T_FOODHUGE_RANDOM_1")
    return Anim::FoodHuge1;
  if(name=="T_POTION_RANDOM_1")
    return Anim::Potition1;
  if(name=="T_POTION_RANDOM_2")
    return Anim::Potition2;
  if(name=="T_POTION_RANDOM_3")
    return Anim::Potition3;
  if(name=="T_JOINT_RANDOM_1")
    return Anim::Joint1;
  if(name=="T_MEAT_RANDOM_1")
    return Anim::Meat1;
  if(name=="T_STAND_2_PEE" || name=="S_PEE")
    return Anim::Pee;
  if(name=="T_DANCE_01")
    return Anim::Dance1;
  if(name=="T_DANCE_02")
    return Anim::Dance2;
  if(name=="T_DANCE_03")
    return Anim::Dance3;
  if(name=="T_DANCE_04")
    return Anim::Dance4;
  if(name=="T_DANCE_05")
    return Anim::Dance5;
  if(name=="T_DANCE_06")
    return Anim::Dance6;
  if(name=="T_DANCE_07")
    return Anim::Dance7;
  if(name=="T_DANCE_08")
    return Anim::Dance8;
  if(name=="T_DANCE_09")
    return Anim::Dance9;
  if(name=="T_STAND_2_FEAR_VICTIM1" || name=="S_FEAR_VICTIM1")
    return Anim::Fear1;
  if(name=="T_STAND_2_FEAR_VICTIM2" || name=="S_FEAR_VICTIM2")
    return Anim::Fear2;
  if(name=="T_STAND_2_FEAR_VICTIM3" || name=="S_FEAR_VICTIM3")
    return Anim::Fear3;
  if(name=="T_STAND_2_VICTIM_SLE" || name=="S_VICTIM_SLE")
    return Anim::MagicSleep;
  if(name=="T_STAND_2_PRAY")
    return Anim::Pray;
  if(name=="T_PRAY_RANDOM")
    return Anim::PrayRand;
  if(name=="T_PLUNDER")
    return Anim::Plunder;
  if(name=="S_FOOD_S0")
    return Anim::Food1;
  if(name=="S_FOODHUGE_S0")
    return Anim::FoodHuge1;
  if(name=="S_POTIONFAST_S0")
    return Anim::PotitionFast;
  if(name=="S_POTION_S0")
    return Anim::Potition1;
  if(name.find("T_DIALOGGESTURE_")==0){
    for(int i=Anim::Dialog1;i<=DialogLastG1;++i){
      char buf[32]={};
      std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",int(i-Anim::Dialog1+1));
      if(name==buf)
        return Anim(i);
      }
    }
  return Anim::NoAnim;
  }
