#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>
#include <Tempest/Application>
#include <Tempest/Sound>
#include <Tempest/SoundEffect>
#include <Tempest/Log>

#include <zenload/modelScriptParser.h>
#include <zenload/zCModelMeshLib.h>
#include <zenload/zCMorphMesh.h>
#include <zenload/zCProgMeshProto.h>
#include <zenload/zenParser.h>
#include <zenload/ztex2dds.h>

#include <fstream>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"
#include "graphics/skeleton.h"
#include "graphics/protomesh.h"
#include "graphics/animation.h"
#include "graphics/attachbinder.h"
#include "physics/physicmeshshape.h"
#include "dmusic/riff.h"

#include "gothic.h"

using namespace Tempest;

Resources* Resources::inst=nullptr;

static void emplaceTag(char* buf, char tag){
  for(size_t i=1;buf[i];++i){
    if(buf[i]==tag && buf[i-1]=='_' && buf[i+1]=='0'){
      buf[i  ]='%';
      buf[i+1]='s';
      ++i;
      return;
      }
    }
  }

Resources::Resources(Gothic &gothic, Tempest::Device &device)
  : device(device), asset("data",device), gothic(gothic) {
  inst=this;

  const char* menu = "data/font/menu.ttf";
  const char* main = "data/font/main.ttf";

  if(/* DISABLES CODE */ (true)) { // international chasters
    menu = "data/font/Kelvinch-Roman.otf";
    main = "data/font/Kelvinch-Roman.otf";
    }

  static std::array<VertexFsq,6> fsqBuf =
   {{
      {-1,-1},{ 1,1},{1,-1},
      {-1,-1},{-1,1},{1, 1}
   }};
  fsq = Resources::loadVbo(fsqBuf.data(),fsqBuf.size());

  const float mult=0.75f;

  fBuff .reserve(8*1024*1024);
  ddsBuf.reserve(8*1024*1024);

  menuFnt = Font(menu);
  menuFnt.setPixelSize(44*mult);

  mainFnt = Font(main);
  mainFnt.setPixelSize(24*mult);

  dlgFnt  = Font(main);
  dlgFnt.setPixelSize(32*mult);

  fallback = device.loadTexture("data/fallback.png");

  Pixmap pm(1,1,Pixmap::Format::RGBA);
  fbZero = device.loadTexture(pm);

  // TODO: priority for *.mod files
  std::vector<std::u16string> archives;
  detectVdf(archives,gothic.path()+u"Data/");

  // addon archives first!
  std::sort(archives.begin(),archives.end(),[](const std::u16string& a,const std::u16string& b){
    int aIsMod = (a.rfind(u".mod")==a.size()-4) ? 1 : 0;
    int bIsMod = (b.rfind(u".mod")==b.size()-4) ? 1 : 0;
    return std::make_tuple(aIsMod,VDFS::FileIndex::getLastModTime(a)) >
           std::make_tuple(bIsMod,VDFS::FileIndex::getLastModTime(b));
    });

  for(auto& i:archives)
    gothicAssets.loadVDF(i);
  gothicAssets.finalizeLoad();

  // auto v = getFileData("BSANVIL_OC_USE.asc");
  // Tempest::WFile f("../internal/BSANVIL_OC_USE.asc");
  // f.write(v.data(),v.size());
  }

Resources::~Resources() {
  inst=nullptr;
  }

void Resources::detectVdf(std::vector<std::u16string> &ret, const std::u16string &root) {
  Dir::scan(root,[this,&root,&ret](const std::u16string& vdf,Dir::FileType t){
    if(t==Dir::FT_File) {
      auto file = root + vdf;
      if(VDFS::FileIndex::getLastModTime(file)>0 || vdf.rfind(u".mod")==vdf.size()-4)
        ret.emplace_back(std::move(file));
      return;
      }

    if(t==Dir::FT_Dir && vdf!=u".." && vdf!=u".") {
      auto dir = root + vdf + u"/";
      detectVdf(ret,dir);
      }
    });
  }

Font Resources::fontByName(const std::string &fontName) {
  if(fontName=="FONT_OLD_10_WHITE.TGA" || fontName=="font_old_10_white.tga"){
    return Resources::font();
    }
  else if(fontName=="FONT_OLD_10_WHITE_HI.TGA"){
    return Resources::font();
    }
  else if(fontName=="FONT_OLD_20_WHITE.TGA" || fontName=="font_old_20_white.tga"){
    return Resources::menuFont();
    } else {
    return Resources::menuFont();
    }
  }

const Texture2d& Resources::fallbackTexture() {
  return inst->fallback;
  }

const Texture2d &Resources::fallbackBlack() {
  return inst->fbZero;
  }

VDFS::FileIndex& Resources::vdfsIndex() {
  return inst->gothicAssets;
  }

const Tempest::VertexBuffer<Resources::VertexFsq> &Resources::fsqVbo() {
  return inst->fsq;
  }

Tempest::Texture2d* Resources::implLoadTexture(std::string name) {
  if(name.size()==0)
    return nullptr;

  auto it=texCache.find(name);
  if(it!=texCache.end())
    return it->second.get();

  if(getFileData(name.c_str(),fBuff))
    return implLoadTexture(std::move(name),fBuff);

  if(name.rfind(".TGA")==name.size()-4){
    auto n = name;
    n.resize(n.size()+2);
    std::memcpy(&n[0]+n.size()-6,"-C.TEX",6);
    if(!getFileData(n.c_str(),fBuff))
      return nullptr;
    ddsBuf.clear();
    ZenLoad::convertZTEX2DDS(fBuff,ddsBuf);
    return implLoadTexture(std::move(name),ddsBuf);
    }

  return nullptr;
  }

Texture2d *Resources::implLoadTexture(std::string&& name,const std::vector<uint8_t> &data) {
  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    std::unique_ptr<Texture2d> t{new Texture2d(device.loadTexture(pm))};
    Texture2d* ret=t.get();
    texCache[std::move(name)] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load texture \"",name,"\"");
    return &fallback;
    }
  }

ProtoMesh* Resources::implLoadMesh(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=aniMeshCache.find(name);
  if(it!=aniMeshCache.end())
    return it->second.get();

  if(name=="Hum_Head_Pony.MMB")//"Sna_Body.MDM"
    Log::d("");

  try {
    ZenLoad::PackedMesh        sPacked;
    ZenLoad::zCModelMeshLib    library;
    auto                       code=loadMesh(sPacked,library,name);
    std::unique_ptr<ProtoMesh> t{code==MeshLoadCode::Static ? new ProtoMesh(sPacked) : new ProtoMesh(library)};
    ProtoMesh* ret=t.get();
    aniMeshCache[name] = std::move(t);
    if(code==MeshLoadCode::Static && sPacked.subMeshes.size()>0)
      phyMeshCache[ret].reset(PhysicMeshShape::load(sPacked));
    if(code==MeshLoadCode::Error)
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load mesh \"",name,"\"");
    return nullptr;
    }
  }

Skeleton* Resources::implLoadSkeleton(std::string name) {
  if(name.size()==0)
    return nullptr;

  if(name.rfind(".MDS")==name.size()-4 ||
     name.rfind(".mds")==name.size()-4)
    std::memcpy(&name[name.size()-3],"MDH",3);

  auto it=skeletonCache.find(name);
  if(it!=skeletonCache.end())
    return it->second.get();

  try {
    ZenLoad::zCModelMeshLib library(name,gothicAssets,1.f);
    std::unique_ptr<Skeleton> t{new Skeleton(library,name)};
    Skeleton* ret=t.get();
    skeletonCache[name] = std::move(t);
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load skeleton \"",name,"\"");
    return nullptr;
    }
  }

Animation *Resources::implLoadAnimation(std::string name) {
  if(name.size()<4)
    return nullptr;

  auto it=animCache.find(name);
  if(it!=animCache.end())
    return it->second.get();

  try {
    Animation* ret=nullptr;
    if(gothic.isGothic2()){
      if(name.rfind(".MDS")==name.size()-4 ||
         name.rfind(".mds")==name.size()-4 ||
         name.rfind(".MDH")==name.size()-4)
        std::memcpy(&name[name.size()-3],"MSB",3);
      ZenLoad::ZenParser            zen(name,gothicAssets);
      ZenLoad::MdsParserBin         p(zen);

      std::unique_ptr<Animation> t{new Animation(p,name.substr(0,name.size()-4),false)};
      ret=t.get();
      animCache[name] = std::move(t);
      } else {
      if(name.rfind(".MDH")==name.size()-4)
        std::memcpy(&name[name.size()-3],"MDS",3);
      ZenLoad::ZenParser zen(name,gothicAssets);
      ZenLoad::MdsParserTxt p(zen);

      std::unique_ptr<Animation> t{new Animation(p,name.substr(0,name.size()-4),true)};
      ret=t.get();
      animCache[name] = std::move(t);
      }
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load animation \"",name,"\"");
    return nullptr;
    }
  }

SoundEffect *Resources::implLoadSound(const char* name) {
  if(name==nullptr || *name=='\0')
    return nullptr;

  auto it=sndCache.find(name);
  if(it!=sndCache.end())
    return it->second.get();

  if(!getFileData(name,fBuff))
    return nullptr;

  try {
    Tempest::MemReader rd(fBuff.data(),fBuff.size());

    auto s = sound.load(rd);
    std::unique_ptr<SoundEffect> t{new SoundEffect(std::move(s))};
    SoundEffect* ret=t.get();
    sndCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load sound \"",name,"\"");
    return nullptr;
    }
  }

Sound Resources::implLoadSoundBuffer(const char *name) {
  if(!getFileData(name,fBuff))
    return Sound();
  try {
    Tempest::MemReader rd(fBuff.data(),fBuff.size());
    return Sound(rd);
    }
  catch(...){
    Log::e("unable to load sound \"",name,"\"");
    return Sound();
    }
  }

Dx8::Segment *Resources::implLoadMusic(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=musicCache.find(name);
  if(it!=musicCache.end())
    return it->second.get();

  try {
    std::u16string p = gothic.path();
    p.append(name.begin(),name.end());
    Tempest::RFile fin(p);

    std::vector<uint8_t> data(fin.size());
    fin.read(reinterpret_cast<char*>(&data[0]),data.size());

    Dx8::Riff riff{data.data(),data.size()};

    std::unique_ptr<Dx8::Segment> t{new Dx8::Segment(riff)};
    Dx8::Segment* ret=t.get();
    musicCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    musicCache[name] = nullptr;
    Log::e("unable to load music \"",name,"\"");
    return nullptr;
    }
  }

bool Resources::hasFile(const std::string &fname) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->gothicAssets.hasFile(fname);
  }

const Texture2d *Resources::loadTexture(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(name);
  }

const Tempest::Texture2d* Resources::loadTexture(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(name);
  }

const Texture2d *Resources::loadTexture(const std::string &name, int32_t iv, int32_t ic) {
  if(name.size()>=128)// || (iv==0 && ic==0))
    return loadTexture(name);

  char v[16]={};
  char c[16]={};
  char buf1[128]={};
  char buf2[128]={};

  std::snprintf(v,sizeof(v),"V%d",iv);
  std::snprintf(c,sizeof(c),"C%d",ic);
  std::strcpy(buf1,name.c_str());

  emplaceTag(buf1,'V');
  std::snprintf(buf2,sizeof(buf2),buf1,v);

  emplaceTag(buf2,'C');
  std::snprintf(buf1,sizeof(buf1),buf2,c);

  return loadTexture(buf1);
  }

const ProtoMesh *Resources::loadMesh(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadMesh(name);
  }

const Skeleton *Resources::loadSkeleton(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSkeleton(name);
  }

const Animation *Resources::loadAnimation(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadAnimation(name);
  }

const PhysicMeshShape *Resources::physicMesh(const ProtoMesh *view) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  if(view==nullptr)
    return nullptr;
  auto it = inst->phyMeshCache.find(view);
  if(it!=inst->phyMeshCache.end())
    return it->second.get();
  return nullptr;
  }

SoundEffect *Resources::loadSound(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSound(name);
  }

SoundEffect *Resources::loadSound(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSound(name.c_str());
  }

Sound Resources::loadSoundBuffer(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSoundBuffer(name.c_str());
  }

Sound Resources::loadSoundBuffer(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSoundBuffer(name);
  }

Dx8::Segment *Resources::loadMusic(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadMusic(name);
  }

bool Resources::getFileData(const char *name, std::vector<uint8_t> &dat) {
  dat.clear();
  return inst->gothicAssets.getFileData(name,dat);
  }

std::vector<uint8_t> Resources::getFileData(const char *name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

std::vector<uint8_t> Resources::getFileData(const std::string &name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

Resources::MeshLoadCode Resources::loadMesh(ZenLoad::PackedMesh& sPacked, ZenLoad::zCModelMeshLib& library, std::string name) {
  std::vector<uint8_t> data;
  std::vector<uint8_t> dds;

  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    // Strip the extension ".***"
    // Add "compiled"-extension
    if(name.rfind(".3DS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".3ds")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".mms")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".MMS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".ASC")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3); else
    if(name.rfind(".asc")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3);
    }

  if(name.rfind(".MRM")==name.size()-4) {
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmsh.packMesh(sPacked,1.f);
    return MeshLoadCode::Static;
    }

  if(name.rfind(".MMB")==name.size()-4) {
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    if(zmm.getMesh().getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmm.getMesh().packMesh(sPacked,1.f);
    for(auto& i:sPacked.vertices){
      // FIXME: hack with morph mesh-normals
      std::swap(i.Normal.y,i.Normal.z);
      i.Normal.z=-i.Normal.z;
      }
    return MeshLoadCode::Static;
    }

  if(name.rfind(".MDMS")==name.size()-5 ||
     name.rfind(".MDS") ==name.size()-4 ||
     name.rfind(".MDL") ==name.size()-4 ||
     name.rfind(".MDM") ==name.size()-4 ||
     name.rfind(".mds") ==name.size()-4){
    library = loadMDS(name);
    return MeshLoadCode::Dynamic;
    }

  return MeshLoadCode::Error;
  }

ZenLoad::zCModelMeshLib Resources::loadMDS(std::string &name) {
  if(name.rfind(".MDMS")==name.size()-5){
    name.resize(name.size()-1);
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
    }
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDL",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDM",3);
  if(hasFile(name)) {
    ZenLoad::zCModelMeshLib lib(name,gothicAssets,1.f);
    std::memcpy(&name[name.size()-3],"MDH",3);
    if(hasFile(name)) {
      auto data=getFileData(name);
      if(!data.empty()){
        ZenLoad::ZenParser parser(data.data(), data.size());
        lib.loadMDH(parser,1.f);
        }
      }
    return lib;
    }
  std::memcpy(&name[name.size()-3],"MDH",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  return ZenLoad::zCModelMeshLib();
  }

const AttachBinder *Resources::bindMesh(const ProtoMesh &anim, const Skeleton &s, const char *defBone) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);

  if(anim.submeshId.size()==0){
    static AttachBinder empty;
    return &empty;
    }
  BindK k = BindK(&s,&anim,defBone ? defBone : "");

  auto it = inst->bindCache.find(k);
  if(it!=inst->bindCache.end())
    return it->second.get();

  std::unique_ptr<AttachBinder> ret(new AttachBinder(s,anim,defBone));
  auto p = ret.get();
  inst->bindCache[k] = std::move(ret);
  return p;
  }
