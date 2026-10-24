// Derived graph only; iterative SCC and bounded condensation reachability cache.
#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>
#include <utility>
namespace WarsBots {
class BotReachability {
public:
 void Reset(){m_component.clear();m_out.clear();m_bits.clear();m_size.clear();m_words=0;m_ready=false;}
 bool Build(const std::vector<std::vector<int>>& graph) {
  Reset();const int n=int(graph.size());if(!n)return false;
  std::vector<std::vector<int>> reverse(n);std::vector<char> seen(n,0);std::vector<int> order;
  for(int a=0;a<n;++a)for(int b:graph[a]){if(b<0||b>=n)return false;reverse[b].push_back(a);}
  std::vector<std::pair<int,bool>> stack;
  for(int root=0;root<n;++root)if(!seen[root]) {
   stack.emplace_back(root,false);
   while(!stack.empty()){
    const auto item=stack.back();stack.pop_back();const int a=item.first;
    if(item.second){order.push_back(a);continue;}if(seen[a])continue;
    seen[a]=1;stack.emplace_back(a,true);
    for(int b:graph[a])if(!seen[b])stack.emplace_back(b,false);
   }
  }
  m_component.assign(n,-1);std::vector<int> todo;
  for(auto it=order.rbegin();it!=order.rend();++it)if(m_component[*it]<0){
   const int c=int(m_size.size());m_size.push_back(0);m_component[*it]=c;todo.push_back(*it);
   while(!todo.empty()){const int a=todo.back();todo.pop_back();++m_size[c];
    for(int b:reverse[a])if(m_component[b]<0){m_component[b]=c;todo.push_back(b);}}
  }
  const int count=int(m_size.size());m_words=(std::size_t(count)+63)/64;
  // Fail open instead of allocating an unbounded quadratic cache on unusual maps.
  if(std::size_t(count)*m_words>16u*1024*1024/sizeof(std::uint64_t))return false;
  m_out.resize(count);std::vector<int> indegree(count,0),topo;
  for(int a=0;a<n;++a)for(int b:graph[a])if(m_component[a]!=m_component[b])m_out[m_component[a]].push_back(m_component[b]);
  for(auto& row:m_out){std::sort(row.begin(),row.end());row.erase(std::unique(row.begin(),row.end()),row.end());for(int b:row)++indegree[b];}
  for(int c=0;c<count;++c)if(!indegree[c])topo.push_back(c);
  for(std::size_t i=0;i<topo.size();++i)for(int b:m_out[topo[i]])if(!--indegree[b])topo.push_back(b);
  if(topo.size()!=m_size.size())return false;
  m_bits.assign(std::size_t(count)*m_words,0);
  for(auto it=topo.rbegin();it!=topo.rend();++it){const int c=*it;auto* row=&m_bits[std::size_t(c)*m_words];row[c/64]|=std::uint64_t(1)<<(c%64);
   for(int b:m_out[c]){const auto* child=&m_bits[std::size_t(b)*m_words];for(std::size_t w=0;w<m_words;++w)row[w]|=child[w];}}
  return m_ready=true;
 }
 bool Ready()const{return m_ready;}
 int Component(int node)const{return node>=0&&node<int(m_component.size())?m_component[node]:-1;}
 int Count()const{return int(m_size.size());}
 int Size(int c)const{return c>=0&&c<int(m_size.size())?m_size[c]:0;}
 bool Reaches(int a,int b)const{const int c=Component(a),d=Component(b);return m_ready&&c>=0&&d>=0&&(m_bits[std::size_t(c)*m_words+d/64]&(std::uint64_t(1)<<(d%64)));}
 bool Closed(int node)const{const int c=Component(node);return m_ready&&c>=0&&m_out[c].empty()&&m_size[c]<int(m_component.size());}
private:
 std::vector<int> m_component,m_size;std::vector<std::vector<int>> m_out;
 std::vector<std::uint64_t> m_bits;std::size_t m_words=0;bool m_ready=false;
};
}
