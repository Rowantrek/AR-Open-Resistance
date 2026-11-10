// Native compilation/flight/performance are not provided by this runner. Every child must finish its report.
const fs=require('fs'),path=require('path'),cp=require('child_process'),os=require('os'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
const suites=[...fs.readFileSync(path.join(__dirname,'Validate-QRFRelease.js'),'utf8').match(/const suites=\[([\s\S]*?)\];/)[1].matchAll(/'([^']+\.js)'/g)].map(m=>m[1]);
const all=[...new Set([...suites,'Test-HighCommandSimulation.js','Test-PhoneSession.js','Test-GarrisonHelicopterProximity.js','Test-PlayerSquads.js','Test-PlayerSquadMutations.js','Test-TownCorrections.js','Test-QRFSourceStructure.ps1','Test-ExecutorSettings.ps1','Test-TownSites.ps1'])];
const selected=process.argv.slice(2),names=selected.length?selected:all;
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'AROR-town-corrections-'));
const hook=path.join(tmp,'artifacts.cjs');
fs.writeFileSync(hook,`const fs=require('fs'),path=require('path');const read=fs.readFileSync,write=fs.writeFileSync,root=${JSON.stringify(root)},tmp=${JSON.stringify(tmp)};
function dest(p){if(typeof p!=='string')return p;const rel=path.relative(root,path.resolve(p));return rel.startsWith('docs'+path.sep)?path.join(tmp,rel):p;}
fs.writeFileSync=function(p,...args){const out=dest(p);if(out!==p)fs.mkdirSync(path.dirname(out),{recursive:true});return write.call(this,out,...args);};
fs.readFileSync=function(p,...args){const out=dest(p);return read.call(this,out!==p&&fs.existsSync(out)?out:p,...args);};`);
const results=[];
for(const name of names){
 const ps=name.endsWith('.ps1');
 const began=Date.now();
 const child=cp.spawnSync(ps?'powershell.exe':process.execPath,ps?['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(__dirname,name)]:['--require',hook,path.join(__dirname,name)],
  {cwd:root,encoding:'utf8',timeout:180000,windowsHide:true,env:{...process.env,ELECTRON_RUN_AS_NODE:'1',NODE_OPTIONS:'--require '+JSON.stringify(hook)}});
 const output=((child.stdout||'')+(child.stderr||'')).replace(/^.*crashpad.*(?:\r?\n|$)/gm,'').trim();
 const assertions=Number(output.match(/["']?assertions["']?\s*:\s*(\d+)/)?.[1]||0);
 const complete=ps?/Checked|OK|Verified|passed/i.test(output):assertions>0||/checks passed/i.test(output);
 const passed=child.status===0&&!child.error&&complete&&!/^\w*Error(?: \[[^\]]+\])?:/m.test(output);
 const record={name,passed,assertions,elapsedMs:Date.now()-began,exitCode:child.status,error:child.error?.message,output};results.push(record);
 console.log((passed?'PASS ':'FAIL ')+name+' '+assertions);
 if(!passed)console.log(output.slice(-4200));
}
const changed=[...new Set(['diff','untracked'].flatMap(kind=>cp.execFileSync('git',kind==='diff'?['diff','--name-only','-z']:['ls-files','--others','--exclude-standard','-z'],{cwd:root,encoding:'utf8'}).split('\0').filter(f=>/\.(c|js|ps1|layer|conf|et|md)$/.test(f))))];
const hashes=Object.fromEntries(changed.filter(f=>fs.existsSync(path.join(root,f))).map(f=>[f,crypto.createHash('sha256').update(fs.readFileSync(path.join(root,f))).digest('hex')]));
const report={generatedAt:new Date().toISOString(),snapshot:cp.execFileSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim(),nativeVerified:false,
 scope:'Current working tree: actual-source policy/lifecycle regressions and existing mocked/static suites. No native compilation or dedicated-server stress test.',
 results,sourceHashes:hashes,temporaryArtifacts:tmp,summary:{checks:results.length,passing:results.filter(r=>r.passed).length,assertions:results.filter(r=>r.passed).reduce((s,r)=>s+r.assertions,0),failing:results.filter(r=>!r.passed).map(r=>r.name)}};
const output=path.join(root,'docs/validation/TOWN_CORRECTIONS_2026-11-10.json');fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report.summary));
if(report.summary.failing.length)process.exitCode=1;
