#include "CryGame/Bots/BotDemoCodec.h"
#include <cassert>
#include <fstream>
#include <limits>
#include <string>
using namespace WarsBots::DemoCodec;
int main(int argc,char** argv){
 assert(Yaw(180)==-18000 && Yaw(-180)==-18000 && Yaw(359.999f)==0);
 assert(Yaw(179.999f)==-18000 && Yaw(-179.999f)==-18000);
 assert(YawDelta(17900,-17900)==200);
 Bytes order;order.U32(0x01020304);assert((order.data==std::vector<uint8_t>{4,3,2,1}));
 bool rejected=false;try{Bytes b;b.F32(std::numeric_limits<float>::quiet_NaN());}catch(...){rejected=true;}assert(rejected);
 Bytes out;std::string header="{\"map\":\"test\",\"build\":\"codec-unit\",\"hz\":20}";
 out.Raw("WBDM",4);out.U16(2);out.U32(uint32_t(header.size()));out.Raw(header.data(),header.size());
 const char* strings[]={"WarsBot_01","normal/assault","SCAR","ATTACKING","test"};
 for(uint16_t i=0;i<5;++i){Bytes b;b.U16(i+1);auto n=std::strlen(strings[i]);b.U8(uint8_t(n));b.Raw(strings[i],n);out.Record(1,b);}
 Bytes join;join.U32(0);join.U32(1);join.U8(1);join.U16(1);join.U16(2);join.U8(1);assert(join.data.size()==14);out.Record(2,join);
 ActorState row;row.id=1;row.x=10;row.y=20;row.z=30;row.yaw=Yaw(90);row.pitch=1500;row.stance=0;row.flags=5;row.energy=200;row.health=100;row.weapon=3;row.weaponId=123;row.peakYaw=9000;
 Bytes state;row.Write(state);assert(state.data.size()==33);
 Bytes frame;frame.U32(100);frame.U8(1);frame.Raw(state.data.data(),state.data.size());out.Record(3,frame);
 Bytes brain;brain.U32(100);brain.U32(1);brain.U16(4);brain.U32(0);brain.U8(0);for(int i=0;i<9;++i)brain.F32(float(i));brain.U16(5);brain.U8(2);brain.U8(4);brain.U32(42);assert(brain.data.size()==59);out.Record(4,brain);
 Bytes event;event.U32(100);event.U8(1);event.U32(1);event.U32(0);event.F32(0);for(int i=0;i<3;++i)event.F32(0);assert(event.data.size()==29);out.Record(5,event);
 Bytes intent;intent.U32(100);intent.U32(1);intent.F32(0);intent.F32(1);intent.U8(2);intent.U8(2);intent.U8(1);intent.U8(0);intent.U32(0);intent.U32(0);intent.U16(5);assert(intent.data.size()==30);out.Record(6,intent);
 Bytes inv;inv.U32(100);inv.U32(1);inv.U8(1);inv.U16(3);inv.U16(30);inv.U16(90);out.Record(7,inv);
 if(argc>1){std::ofstream file(argv[1],std::ios::binary);file.write(reinterpret_cast<const char*>(out.data.data()),out.data.size());assert(file.good());}
}
