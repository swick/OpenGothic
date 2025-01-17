#pragma once

#include <Tempest/Matrix4x4>
#include "physics/dynamicworld.h"
#include "graphics/animationsolver.h"
#include "graphics/meshobjects.h"
#include "graphics/protomesh.h"
#include "game/inventory.h"

class Npc;
class World;
class Trigger;

class Interactive final {
  public:
    enum Anim : int8_t {
      In    = 1,
      Active= 0,
      Out   =-1
      };

    Interactive(World& owner,const ZenLoad::zCVobData &vob);
    Interactive(Interactive&&)=default;

    const std::string&  tag() const;
    const std::string&  focusName() const;
    bool                checkMobName(const std::string& dest) const;
    const std::string&  ownerName() const;

    std::array<float,3> position() const;
    std::array<float,3> displayPosition() const;
    const char*         displayName() const;

    std::string         stateFunc() const;
    int32_t             stateId() const { return state; }
    void                emitTriggerEvent() const;
    const char*         schemeName() const;

    bool                isContainer() const;
    Inventory&          inventory();

    uint32_t            stateMask(uint32_t orig) const;

    bool                canSeeNpc(const Npc &npc, bool freeLos) const;

    bool                isAvailable() const;
    bool                isLoopState() const { return loopState; }
    bool                attach (Npc& npc);
    bool                dettach(Npc& npc);

    void                nextState();
    void                prevState();
    auto                anim(const AnimationSolver &solver, Anim t) -> AnimationSolver::Sequence;
    void                marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    Tempest::Matrix4x4     objMat;

  private:
    struct Pos final {
      Npc*               user=nullptr;
      std::string        name;
      size_t             node=0;
      Tempest::Matrix4x4 pos;

      const char*        posTag() const;
      bool               isAttachPoint() const;
      };

    void setPos(Npc& npc,std::array<float,3> pos);
    void setDir(Npc& npc,const Tempest::Matrix4x4& mt);
    bool attach(Npc& npc,Pos& to);
    void implAddItem(char *name);
    void autoDettachNpc();

    const Pos* findFreePos() const;
    Pos*       findFreePos();
    auto       worldPos(const Pos &to) const -> std::array<float,3>;
    float      qDistanceTo(const Npc &npc, const Pos &to);

    World*             world = nullptr;
    ZenLoad::zCVobData data;
    Inventory          invent;
    int                state=0;
    bool               loopState=false;

    std::vector<Pos>         pos;
    const ProtoMesh*         mesh = nullptr;
    MeshObjects::Mesh        view;
    DynamicWorld::StaticItem physic;
  };
