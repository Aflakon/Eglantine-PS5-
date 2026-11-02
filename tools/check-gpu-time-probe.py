#!/usr/bin/env python3
"""Exercise the actual GPU timestamp probe with observable Vulkan operations."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
using u32=unsigned;using u64=uint64_t;
constexpr unsigned VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO=1,VK_QUERY_TYPE_TIMESTAMP=2,
 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT=3,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT=4,VK_QUERY_RESULT_64_BIT=1,VK_SUCCESS=0;
namespace Eden::DevVulkan { inline bool gpu_time=true; }
struct State { unsigned creates{},destroys{},waits{},resets{},writes{};bool ready=true,alive=true;unsigned bits=8;double period=2; } state;
struct QueryInfo {unsigned sType;void* pNext;unsigned flags,queryType,queryCount,pipelineStatistics;};
using PFN_vkCmdWriteTimestamp=void(*)(unsigned,unsigned,unsigned,unsigned);
void Write(unsigned,unsigned,unsigned,unsigned){++state.writes;}
namespace vk {
struct QueryPool {
 unsigned id{};QueryPool()=default;explicit QueryPool(unsigned x):id(x){}
 QueryPool(const QueryPool&)=delete;
 QueryPool(QueryPool&& x):id(std::exchange(x.id,0)){}
 QueryPool& operator=(QueryPool&& x){if(id){assert(state.alive);++state.destroys;}id=std::exchange(x.id,0);return *this;}
 ~QueryPool(){if(id){assert(state.alive);++state.destroys;}}
 explicit operator bool()const{return id;}
 unsigned operator*()const{return id;}
};
struct CommandBuffer {unsigned operator*()const{return 1;}void ResetQueryPool(unsigned,unsigned,unsigned){++state.resets;}};
}
struct Logical {
 unsigned operator*()const{return 1;}
 vk::QueryPool CreateQueryPool(QueryInfo){return vk::QueryPool{++state.creates};}
 int GetQueryResults(unsigned,unsigned,unsigned,size_t,void* out,size_t,unsigned){
  if(!state.ready)return 1;auto p=static_cast<u64*>(out);p[0]=250;p[1]=7;return 0;
 }
};
struct Physical {
 struct Props {struct Limits {double timestampPeriod;}limits;};
 struct Family {unsigned timestampValidBits;};
 Props GetProperties()const{return {{state.period}};}
 std::vector<Family> GetQueueFamilyProperties()const{return {{state.bits}};}
};
struct Dispatch {
 void* vkGetDeviceProcAddr(unsigned,const char*)const{return reinterpret_cast<void*>(&Write);}
 int vkDeviceWaitIdle(unsigned)const{++state.waits;return 0;}
};
struct Device {
 mutable Logical logical;Physical physical;Dispatch dispatch;
 const Dispatch& GetDispatchLoader()const{return dispatch;}
 Logical& GetLogical()const{return logical;}
 const Physical& GetPhysical()const{return physical;}
 unsigned GetGraphicsFamily()const{return 0;}
};
'''+(root/'headless/gpu_time_probe.inc').read_text()+r'''
int main(){Device d;vk::CommandBuffer cmd;
 Eden::DevVulkan::gpu_time=false;GpuTimeBegin(d,cmd);assert(state.creates==0);GpuTimeReset(d);assert(state.waits==0);
 Eden::DevVulkan::gpu_time=true;
 GpuTimeBegin(d,cmd);GpuTimeEnd(cmd);GpuTimeBegin(d,cmd);GpuTimeEnd(cmd);
 assert(state.creates==1&&state.resets==2&&state.writes==4);
 state.ready=false;GpuTimeCollect(d);assert(gpu_time.first_pending==0);
 state.ready=true;GpuTimeCollect(d);assert(gpu_time.total_ns==26&&gpu_time.total_submissions==1); // 8-bit wrap: (7-250)&255 = 13
 GpuTimeReset(d);assert(state.waits==1&&state.destroys==1&&!gpu_time.pool&&gpu_time.next==0);
 state.ready=false;
 for(unsigned i=0;i<GpuTimeProbe::SLOTS;++i){GpuTimeBegin(d,cmd);GpuTimeEnd(cmd);}
 auto resets=state.resets;GpuTimeBegin(d,cmd);GpuTimeEnd(cmd);
 assert(gpu_time.failed&&state.resets==resets&&gpu_time.next==GpuTimeProbe::SLOTS);
 GpuTimeReset(d);assert(state.destroys==2);
 state.bits=0;GpuTimeBegin(d,cmd);assert(gpu_time.failed&&!gpu_time.pool);GpuTimeReset(d);
 state.bits=64;state.period=0;GpuTimeBegin(d,cmd);assert(gpu_time.failed&&!gpu_time.pool);GpuTimeReset(d);
 state.period=1;state.ready=true;GpuTimeBegin(d,cmd);assert(gpu_time.pool&&!gpu_time.failed);GpuTimeReset(d);
 assert(state.creates==state.destroys);state.alive=false; // no query pool may outlive its device
}
'''
with tempfile.TemporaryDirectory(prefix='gpu-time-probe-')as temp:
 p=Path(temp);(p/'check.cpp').write_text(code)
 subprocess.run(['clang++-18','-std=c++20','-O1','-g','-fsanitize=address,undefined',str(p/'check.cpp'),'-o',str(p/'check')],check=True)
 subprocess.run([str(p/'check')],check=True,timeout=10)
print('GPU timestamp probe PASS: disabled path, readiness without wait, timestamp wrap, bounded ring, reset/relaunch and pool-before-device destruction')
