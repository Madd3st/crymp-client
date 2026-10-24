// wbdemo/2 wire encoding: little endian, explicit fields, never native struct padding.
#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace WarsBots::DemoCodec {
struct Bytes {
 std::vector<uint8_t> data;
 void U8(uint8_t x){data.push_back(x);}
 void U16(uint16_t x){U8(uint8_t(x));U8(uint8_t(x>>8));}
 void U32(uint32_t x){U16(uint16_t(x));U16(uint16_t(x>>16));}
 void I16(int16_t x){U16(uint16_t(x));}
 void F32(float x){if(!std::isfinite(x))throw std::runtime_error("nonfinite demo value");uint32_t bits;std::memcpy(&bits,&x,4);U32(bits);}
 void Raw(const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);data.insert(data.end(),b,b+n);}
 void Record(uint8_t type,const Bytes& p){if(p.data.size()>65535)throw std::runtime_error("demo record too large");U8(type);U16(uint16_t(p.data.size()));if(!p.data.empty())Raw(p.data.data(),p.data.size());}
};
inline int16_t Yaw(float degrees){
 if(!std::isfinite(degrees))throw std::runtime_error("nonfinite yaw");
 int x=int(std::lround(std::fmod(degrees,360.f)*100.f));x=(x+18000)%36000;if(x<0)x+=36000;return int16_t(x-18000);
}
inline uint16_t YawDelta(int16_t a,int16_t b){int x=int(a)-int(b);if(x>18000)x-=36000;if(x<-18000)x+=36000;return uint16_t(std::abs(x));}
struct ActorState {
 uint32_t id=0;float x=0,y=0,z=0;int16_t yaw=0,pitch=0;
 uint8_t stance=4,flags=0,mode=3,energy=0,health=0;
 uint16_t weapon=0;uint32_t weaponId=0;uint16_t peakYaw=0;
 void Write(Bytes& b)const{b.U32(id);b.F32(x);b.F32(y);b.F32(z);b.I16(yaw);b.I16(pitch);b.U8(stance);b.U8(flags);b.U8(mode);b.U8(energy);b.U8(health);b.U16(weapon);b.U32(weaponId);b.U16(peakYaw);}
};
}
