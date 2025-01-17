#include "dialogmenu.h"

#include <Tempest/Painter>
#include <algorithm>

#include "utils/cp1251.h"
#include "gothic.h"
#include "inventorymenu.h"
#include "resources.h"

using namespace Tempest;

bool DialogMenu::Pipe::output(Npc &npc, const std::string &text) {
  return owner.aiOutput(npc,text.c_str());
  }

bool DialogMenu::Pipe::outputSvm(Npc &npc, const std::string &text, int voice) {
  auto& svm = owner.gothic.messageFromSvm(text,voice);
  return owner.aiOutput(npc,svm.c_str());
  }

bool DialogMenu::Pipe::outputOv(Npc &npc, const std::string &text, int voice) {
  return outputSvm(npc,text,voice);
  }

bool DialogMenu::Pipe::close() {
  return owner.aiClose();
  }

bool DialogMenu::Pipe::isFinished() {
  bool ret=false;
  owner.aiIsClose(ret);
  return ret;
  }

DialogMenu::DialogMenu(Gothic &gothic, InventoryMenu &trade)
  :gothic(gothic), trade(trade), pipe(*this) , camera(gothic) {
  tex     = Resources::loadTexture("DLG_CHOICE.TGA");
  ambient = Resources::loadTexture("DLG_AMBIENT.TGA");
  setFocusPolicy(NoFocus);
  camera.setMode(Camera::Dialog);
  }

void DialogMenu::tick(uint64_t dt) {
  if(state==State::PreStart){
    except.clear();
    dlgTrade=false;
    trade.close();
    onStart(*this->pl,*this->other);
    return;
    }

  if(remPrint<dt){
    for(size_t i=1;i<MAX_PRINT;++i)
      printMsg[i-1u]=printMsg[i];
    printMsg[MAX_PRINT-1]=PScreen();
    remPrint=1500;
    } else {
    remPrint-=dt;
    }

  if(current.time<=dt){
    current.time = 0;
    if(dlgTrade && !haveToWaitOutput())
      startTrade();
    } else {
    current.time-=dt;
    }

  for(size_t i=0;i<pscreen.size();){
    auto& p=pscreen[i];
    if(p.time<dt){
      pscreen.erase(pscreen.begin()+int(i));
      } else {
      p.time-=dt;
      ++i;
      }
    }

  update();
  }

void DialogMenu::drawTextMultiline(Painter &p, int x, int y, int w, int h, const std::string &txt,bool isPl) {
  const int pdd=10;
  y+=int(p.font().pixelSize());

  p.setFont(Resources::dialogFont());
  p.setBrush(Color(1,1,1));
  if(isPl){
    p.drawText(x+pdd, y+pdd,
               w-2*pdd, h-2*pdd, txt);
    } else {
    if(other!=nullptr){
      auto txt = cp1251::toUtf8(other->displayName());
      auto sz  = p.font().textSize(txt);
      p.drawText(x+(w-sz.w)/2,y,txt);
      y+=int(sz.h);
      h-=int(sz.h);
      }
    p.setBrush(Color(0.81f,0.78f,0.01f));
    p.drawText(x+pdd, y+pdd,
               w-2*pdd, h-2*pdd, txt);
    }
  p.setBrush(Color(1,1,1));
  }

void DialogMenu::clear() {
  for(auto& i:printMsg)
    i=PScreen();
  pscreen.clear();
  }

void DialogMenu::onWorldChanged() {
  assert(state==State::Idle);
  close();
  }

const Camera &DialogMenu::dialogCamera() {
  if(pl && other){
    camera.reset();
    auto p0 = pl->position();
    auto p1 = other->position();
    camera.setPosition(0.5f*(p0[0]+p1[0]),
                       0.5f*(p0[1]+p1[1])+1.5f*pl->translateY(),
                       0.5f*(p0[2]+p1[2]));
    p0[0]-=p1[0];
    p0[1]-=p1[1];
    p0[2]-=p1[2];

    float l = std::sqrt(p0[0]*p0[0]+p0[1]*p0[1]+p0[2]*p0[2]);
    float a = (std::atan2(p0[2],p0[0])/float(M_PI))*180.f;
    if(curentIsPl)
      a+=45; else
      a-=45;

    camera.setDistance(l);
    camera.setSpin(PointF(a,0));
    }
  return camera;
  }

void DialogMenu::openPipe(Npc &player, Npc &npc, AiOuputPipe *&out) {
  out    = &pipe;
  pl     = &player;
  other  = &npc;
  state  = State::PreStart;
  }

bool DialogMenu::aiOutput(Npc &npc, const char *msg) {
  if(&npc!=pl && &npc!=other){
    Log::e("unexpected aiOutput call: ",msg);
    return true;
    }

  if(current.time>0)
    return false;

  if(pl==&npc) {
    if(other!=nullptr)
      other->stopDlgAnim();
    } else {
    if(pl!=nullptr)
      pl->stopDlgAnim();
    }

  current.txt  = gothic.messageByName(msg);
  current.time = gothic.messageTime(msg);
  currentSnd   = soundDevice.load(Resources::loadSoundBuffer(std::string(msg)+".wav"));
  curentIsPl   = (pl==&npc);

  currentSnd.play();
  update();
  return true;
  }

bool DialogMenu::aiClose() {
  if(current.time>0){
    return false;
    }

  choise.clear();
  close();
  state=State::Idle;
  update();
  return true;
  }

void DialogMenu::aiIsClose(bool &ret) {
  ret = (state==State::Idle);
  }

bool DialogMenu::isActive() const {
  return (state!=State::Idle) || current.time>0;
  }

bool DialogMenu::onStart(Npc &p, Npc &ot) {
  choise   = ot.dialogChoises(p,except,state==State::PreStart);
  state    = State::Active;
  depth    = 0;

  if(choise.size()==0){
    close();
    return false;
    }

  if(choise.size()>0 && choise[0].title.size()==0){
    // important dialog
    onEntry(choise[0]);
    }

  dlgSel=0;
  update();
  return true;
  }

void DialogMenu::printScreen(const char *msg, int x, int y, int time, const Tempest::Font &font) {
  PScreen e;
  e.txt  = msg;
  e.font = font;
  e.time = uint32_t(time*1000)+1000;
  e.x    = x;
  e.y    = y;
  pscreen.emplace(pscreen.begin(),std::move(e));
  update();
  }

void DialogMenu::print(const char *msg) {
  if(msg==nullptr || msg[0]=='\0')
    return;

  for(size_t i=1;i<MAX_PRINT;++i)
    printMsg[i-1u]=printMsg[i];

  remPrint=1500;
  PScreen& e=printMsg[MAX_PRINT-1];
  e.txt  = msg;
  e.font = Resources::font();
  e.time = remPrint;
  e.x    = -1;
  e.y    = -1;
  update();
  }

void DialogMenu::onDoneText() {
  choise = gothic.updateDialog(selected,*pl,*other);
  dlgSel = 0;
  if(choise.size()==0){
    if(depth>0) {
      onStart(*pl,*other);
      } else {
      close();
      }
    }
  }

void DialogMenu::close() {
  auto prevPl   = pl;
  auto proveNpc = other;

  pl=nullptr;
  other=nullptr;
  depth=0;
  dlgTrade=false;
  current.time=0;
  choise.clear();
  state=State::Idle;
  currentSnd = SoundEffect();
  update();

  if(prevPl){
    prevPl->setInteraction(nullptr);
    prevPl->stopDlgAnim();
    }
  if(proveNpc && proveNpc!=prevPl){
    proveNpc->setInteraction(nullptr);
    proveNpc->stopDlgAnim();
    }
  }

bool DialogMenu::haveToWaitOutput() const {
  if(pl && pl->haveOutput()){
    return true;
    }
  if(other && other->haveOutput()){
    return true;
    }
  return false;
  }

void DialogMenu::startTrade() {
  if(pl!=nullptr && other!=nullptr)
    trade.trade(*pl,*other);
  dlgTrade=false;
  }

void DialogMenu::onEntry(const GameScript::DlgChoise &e) {
  if(pl && other) {
    selected = e;
    depth    = 1;
    dlgTrade = e.isTrade;
    except.push_back(e.scriptFn);
    gothic.dialogExec(e,*pl,*other);

    onDoneText();
    }
  }

void DialogMenu::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);

  const int dw = std::min(w(),600);
  if(current.time>0 && trade.isOpen()==InventoryMenu::State::Closed){
    if(ambient) {
      p.setBrush(*ambient);
      p.drawRect((w()-dw)/2,20,dw,100,
                 0,0,ambient->w(),ambient->h());
      }

    drawTextMultiline(p,(w()-dw)/2,20,dw,100,current.txt,curentIsPl);
    }

  paintChoise(e);

  for(size_t i=0;i<MAX_PRINT;++i){
    auto& sc = printMsg[i];
    p.setFont(sc.font);
    auto  sz = p.font().textSize(sc.txt);
    int x = (w()-sz.w)/2;
    p.drawText(x, int(i*2+2)*sz.h, sc.txt);
    }

  for(size_t i=0;i<pscreen.size();++i){
    auto& sc = pscreen[i];
    p.setFont(sc.font);
    auto  sz = p.font().textSize(sc.txt);
    int x = sc.x;
    int y = sc.y;
    if(x<0){
      x = (w()-sz.w)/2;
      } else {
      x = int(w()*x/100.f);
      }
    if(y<0){
      y = (h()-sz.h)/2;
      } else {
      y = int(h()*y/100.f);
      }
    p.drawText(x,y,sc.txt);
    }
  }

void DialogMenu::paintChoise(PaintEvent &e) {
  if(choise.size()==0 || current.time>0 || haveToWaitOutput() || trade.isOpen()!=InventoryMenu::State::Closed)
    return;

  Painter p(e);
  p.setFont(Resources::dialogFont());
  const int padd = 20;
  const int dw   = std::min(w(),600);
  const int dh   = int(choise.size()*p.font().pixelSize())+2*padd;
  const int y    = h()-dh-20;

  if(tex) {
    p.setBrush(*tex);
    p.drawRect((w()-dw)/2,y,dw,dh,
               0,0,tex->w(),tex->h());
    }

  for(size_t i=0;i<choise.size();++i){
    int x = (w()-dw)/2;
    if(i==dlgSel)
      p.setBrush(Color(1.f)); else
      p.setBrush(Color(0.6f,0.6f,0.6f,1.f));
    p.drawText(x+padd,y+padd+int((i+1)*p.font().pixelSize()),choise[i].title);
    }
  }

void DialogMenu::onSelect() {
  if(current.time>0 || haveToWaitOutput()){
    return;
    }

  if(dlgSel<choise.size())
    onEntry(choise[dlgSel]);
  }

void DialogMenu::mouseDownEvent(MouseEvent &event) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
    event.ignore();
    return;
    }

  onSelect();
  }

void DialogMenu::mouseWheelEvent(MouseEvent &e) {
  if(state==State::Idle || trade.isOpen()!=InventoryMenu::State::Closed){
    e.ignore();
    return;
    }

  if(e.delta>0)
    dlgSel--;
  if(e.delta<0)
    dlgSel++;
  dlgSel = (dlgSel+choise.size())%std::max<size_t>(choise.size(),1);
  update();
  }

void DialogMenu::keyDownEvent(KeyEvent &e) {
  if(state==State::Idle){
    e.ignore();
    return;
    }

  if(e.key!=Event::K_ESCAPE && trade.isOpen()!=InventoryMenu::State::Closed){
    e.ignore();
    return;
    }

  if(e.key==Event::K_Return){
    onSelect();
    return;
    }
  if(e.key==Event::K_W || e.key==Event::K_S || e.key==Event::K_ESCAPE){
    if(e.key==Event::K_W){
      dlgSel--;
      }
    if(e.key==Event::K_S){
      dlgSel++;
      }
    dlgSel = (dlgSel+choise.size())%std::max<size_t>(choise.size(),1);
    update();
    return;
    }
  e.ignore();
  }

void DialogMenu::keyUpEvent(KeyEvent &event) {
  if(state==State::Idle && current.time>0){
    event.ignore();
    return;
    }

  if(event.key==Event::K_ESCAPE) {
    if(trade.isOpen()!=InventoryMenu::State::Closed) {
      trade.close();
      } else {
      if(current.time>0) {
        currentSnd = SoundEffect();
        current.time=1;
        }
      }
    update();
    }
  }


