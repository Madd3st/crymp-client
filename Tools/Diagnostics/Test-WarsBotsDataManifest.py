"""VS-shell engine-free test of the actual runtime manifest reader/normalizer, with tamper controls.

Explicit code checkout required: never use the QA branch's stale game code. No game linking or live data writes.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess
ap=argparse.ArgumentParser()
ap.add_argument('--code-root',type=Path,required=True)
ap.add_argument('--source',required=True)
ap.add_argument('--data',type=Path,required=True)
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--expect-reject',action='store_true')
args=ap.parse_args()
code=args.code_root.resolve();src=code/'Code/CryGame/Bots/BotNavMesh.cpp';out=args.out.resolve()
assert subprocess.check_output(['git','-C',str(code),'rev-parse','HEAD']).decode().strip()==args.source
subprocess.run(['git','-C',str(code),'diff','--exit-code','HEAD','--',str(src)],check=True,capture_output=True)
out.mkdir(parents=True,exist_ok=True)
s=src.read_text(encoding='utf-8-sig');actual=s[s.index('// T-6:'):s.index('\n}\n\nNavMeshCore* CurrentNavMesh')]
cpp=r'''
#include <string>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#define WB_LOG(...) ((void)0)
std::filesystem::path directory;
std::filesystem::path DataDirectory(const wchar_t*) {return directory;}
namespace WarsBots { namespace {
'''+actual+r'''
} }
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});}
int main(int argc,char** argv){
 if(argc!=2)return 2;directory=std::filesystem::u8path(argv[1]);int fails=0;
 for(const char* kind: {"navmesh","links","spots"}){
  const char* ext=std::strcmp(kind,"links")==0?"links.txt":std::strcmp(kind,"spots")==0?"spots.txt":"navmesh";
  const std::string data=read(directory/(std::string("Shore.")+ext));
  const bool ok=WarsBots::ManifestOk("Shore",kind,data);
  std::printf("Actual ManifestOk %s=%d rawFnv=%s manifest=%s\n",kind,ok,WarsBots::Fnv64(data.data(),data.size()).c_str(),WarsBots::ManifestFnv("Shore",kind).c_str());
  if(!ok)++fails;
  if(WarsBots::ManifestOk("Shore",kind,data+"tamper"))return 3;
  if(std::strcmp(kind,"navmesh")){
   std::string crlf;for(char c:data){if(c=='\n'&&(crlf.empty()||crlf.back()!='\r'))crlf+='\r';crlf+=c;}
   if(ok && !WarsBots::ManifestOk("Shore",kind,crlf))return 4;
  }
 }
 return fails?1:0;
}
'''
(out/'caller.cpp').write_bytes(cpp.encode('utf-8'))
comp=subprocess.run(['cl','/nologo','/std:c++17','/EHsc',str(out/'caller.cpp'),'/Fo:'+str(out)+'/', '/Fe:'+str(out/'caller.exe')],capture_output=True)
(out/'compile.log').write_bytes(comp.stdout+comp.stderr)
assert comp.returncode==0,'Compile failure cannot count as semantic rejection'
result=subprocess.run([str(out/'caller.exe'),str(args.data.resolve())],capture_output=True)
(out/'run.log').write_bytes(result.stdout+result.stderr)
print(result.stdout.decode(errors='replace'))
expected=1 if args.expect_reject else 0
assert result.returncode==expected, f'Expected {expected}, observed {result.returncode}'
receipt={'source':args.source,'actual_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'data_hashes':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in args.data.iterdir() if p.is_file()},'expected_reject':args.expect_reject,'actual_exit':result.returncode,'tamper_and_crlf_controls':'PASS','game_linked':False}
(out/'result.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
