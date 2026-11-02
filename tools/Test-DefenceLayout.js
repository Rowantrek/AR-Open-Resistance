/* Actual-source compact layout/terrain/arc tests. Native geometry and AI still require Workbench. */
function runDefenceLayoutTests(source,presets={}) {
 let assertions=0;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 const types={TOWN:0,HQ:1,AIRPORT:2,CITY:3,FACTORY:4,MILITARY_BASE:5,PORT:6,RESOURCES:7,OUTPOST:8};
 const env={AROR_ELocationType:types,Math:{AbsFloat:Math.abs,Sin:Math.sin,Cos:Math.cos,Floor:Math.floor,Mod:(a,b)=>a%b,Min:Math.min,Max:Math.max,DEG2RAD:Math.PI/180},
   vector:{Zero:[0,0,0]},Vector:(...v)=>v,TraceParam:class{},TraceFlags:{WORLD:1,ENTS:2,VISIBILITY:4},
   vadd:(a,b)=>a.map((v,i)=>v+b[i]),vmul:(a,b)=>a.map(v=>v*b)};
 for(const m of source.matchAll(/static const float (TRIPOD_\w+) = ([\d.]+);/g))env[m[1]]=Number(m[2]);
 function compile(name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!m)throw Error('Missing '+name);
  const args=m[1].replace(/\bout /g,'').replace(/\b(?:BaseWorld|AROR_ELocationType|vector|float|int) /g,'');
  let body=m[2].replace(/float\.MAX/g,'Number.MAX_VALUE')
   .replace(/array<(float|vector)> (\w+) = \{([^}]+)\};/g,(_,type,name,values)=>'let '+name+'=['+(type==='vector'?values.replace(/"([\d .-]+)"/g,(_,v)=>JSON.stringify(v.split(' ').map(Number))):values)+'];')
   .replace(/\b(?:float|int|vector|TraceParam) (\w+);/g,'let $1=0;')
   .replace(/\b(?:float|int|vector|TraceParam) (\w+) =/g,'let $1 =')
   .replace('directions[side] * (halfExtent - 4)','vmul(directions[side],halfExtent-4)')
   .replace('point + Vector(0, 1.4, 0)','vadd(point,Vector(0,1.4,0))')
   .replace('trace.Start + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * 20','vadd(trace.Start,vmul(Vector(Math.Sin(angle),0,Math.Cos(angle)),20))')
   .replace('trace.Start + direction * 20','vadd(trace.Start,vmul(direction,20))')
   .replace('point + Vector(0, TRIPOD_HEIGHT, 0)','vadd(point,Vector(0,TRIPOD_HEIGHT,0))')
   .replace('low.Start + direction * TRIPOD_CHECK_DISTANCE','vadd(low.Start,vmul(direction,TRIPOD_CHECK_DISTANCE))');
  if(name==='TerrainFits')body=body.replace(/return false;/g,'return {ok:false,relief};').replace('return relief <= maxRelief;','return {ok:relief<=maxRelief,relief};');
  if(name==='SiteSearchOffset')body=body.replace('(candidate - 1) / 8','Math.Floor((candidate - 1) / 8)');
  if(name==='DenseSiteSearchOffset')body=body.replace('remaining / (ring * 2)','Math.Floor(remaining / (ring * 2))');
  if(name==='RepairOffset')body=body.replace('(attempt - 1) / 8','Math.Floor((attempt - 1) / 8)');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
 }
 const half=compile('HalfExtent'),point=compile('LocalPosition'),facing=compile('Facing'),repair=compile('RepairOffset'),terrain=compile('TerrainFits'),arc=compile('ClearMGArc');
 const overlaps=compile('FootprintsOverlap');
 check(overlaps([0,0,0],24,45,[41.79,0,-43.8],24,0),'resource squares overlap before consistent orientation');
 check(!overlaps([0,0,0],24,45,[41.79,0,-43.8],24,45),'rotated compact resource perimeters retain eight metres of access clearance');
 check(overlaps([0,0,0],26,315,[16.79,0,-18.8],26,315),'conflicting native resource proposals must not be adopted together');
 check(overlaps([0,0,0],18,0,[40,0,0],18,0),'nearby compounds retain the access gap');
 check(!overlaps([0,0,0],18,0,[50,0,0],18,0),'well separated compounds pass');
 const search=compile('SiteSearchOffset');
 check(search(0).every(v=>v===0),'site search begins at existing centre');
 check(Math.abs(Math.hypot(...search(1))-25)<1e-8,'first alternative uses 25m ring');
 // Enforce integer division is explicit in the harness for the relocation ring.
 check(Math.abs(Math.hypot(...search(121))-400)<1e-8,'last search ring reaches 400m');
 check(Math.abs(Math.hypot(...search(128))-400)<1e-8,'integer ring division keeps final bearing inside 400m');
 check(Array.from({length:129},(_,i)=>search(i)).every(p=>Math.hypot(...p)<=400.000001),'every relocation alternative remains within bounded radius');
 const dense=compile('DenseSiteSearchOffset'),grid=Array.from({length:4225},(_,i)=>dense(i));
 check(grid[0].every(v=>v===0),'dense audit begins at the authored centre');
 check(new Set(grid.map(v=>v.join(','))).size===4225,'dense audit visits every 25m square-grid point once');
 check(grid.every(p=>Math.abs(p[0])<=800&&Math.abs(p[2])<=800),'dense search square remains within authored bounds');
 check(grid.some(p=>p[0]===50&&p[2]===25),'dense search fills gaps between the former eight bearings');
 const radial=grid.filter(p=>Math.hypot(p[0],p[2])<=800);
 check(radial.every(p=>Math.hypot(...p)<=800)&&radial.length>3000,'radial filter supplies over 3000 bounded clearing candidates');
 for(const type of [types.OUTPOST,types.MILITARY_BASE,types.RESOURCES,types.FACTORY,types.PORT,types.AIRPORT]){
  const h=half(type);
  for(let i=0;i<8;i++){const p=point(0,i,h);check(Math.abs(p[0])<=h-4&&Math.abs(p[2])<=h-4,'team inside wall footprint');}
  for(let i=0;i<4;i++){const p=point(1,i,h);check(Math.max(Math.abs(p[0]),Math.abs(p[2]))===h-4,'MG at authored perimeter stand-off');}
  const northTeam=point(0,4,h),northMG=point(1,0,h);
  check(Math.hypot(northTeam[0]-northMG[0],northTeam[2]-northMG[2])>=7,'cardinal infantry position does not collide with the MG clearance gate');
 }
 const a=point(0,0,18),b=point(0,1,18);check(Math.abs(a[0]+b[0])<1e-8&&Math.abs(a[2]+b[2])<1e-8,'first two outpost teams occupy opposed sectors');
 check([0,1,2,3].map(i=>facing(1,i)).join(',')==='0,270,90,180','MGs face outward on all four sides');
 check(facing(3,0)===180&&facing(2,0)===180,'hulls face south entrance');
 for(let i=0;i<25;i++){const offset=repair(i),radius=Math.hypot(offset[0],offset[2]);check(radius<=4.500001,'local repair remains inside a bounded 4.5m area');
  const expected=i===0?0:Math.min(3,Math.floor((i-1)/8)+1)*1.5;check(Math.abs(radius-expected)<1e-6,'repair tries complete 1.5m, 3m and 4.5m rings');}
 let samples=0,water=false,slope=0,oneHill=false;
 const world={GetSurfaceY:(x,z)=>{samples++;return 10+slope*x+(oneHill&&x>10&&z>10?3:0);},GetOceanHeight:()=>water?10:0};
 check(terrain(world,[0,10,0],18,2,0,0).ok&&samples===49,'complete flat footprint sampled');
 slope=0.1;check(!terrain(world,[0,10,0],18,2,0,0).ok,'flat centre with sloping perimeter rejected');slope=0;
 slope=2.78125/68;
 check(!terrain(world,[0,10,0],34,2,0,0).ok,'Levie measured relief fails the original 2m whole-footprint limit');
 check(terrain(world,[0,10,0],34,3,0,0).ok,'Levie site-specific 3m allowance admits measured relief without shrinking its layout');
 slope=4/68;check(!terrain(world,[0,10,0],34,3,0,0).ok,'tailored allowance still rejects a steeper footprint');slope=0;
 oneHill=true;check(!terrain(world,[0,10,0],18,2,0,0).ok,'isolated high corner rejected');oneHill=false;
 water=true;check(!terrain(world,[0,10,0],18,2,0,0).ok,'shoreline/water intrusion rejected');water=false;
 samples=0;check(terrain(world,[0,10,0],32,2,0,45).ok&&samples===49,'rotated compound samples bounded footprint');
 // A narrow quay: water from 26 m south of the centre
 const quay={GetSurfaceY:()=>10,GetOceanHeight:(x,z)=>z<-26?10:0};
 check(!terrain(quay,[0,10,0],42,2,0,0).ok,'square footprint reaching over the harbour is rejected');
 check(terrain(quay,[0,10,0],42,2,0,0,25).ok,'rectangular footprint along the quay stays on land');
 check(!terrain(quay,[0,10,0],42,2,0,90,25).ok,'rectangle turned across the quay reaches the water');
 let traces=[],blocked=0;
 const visibility={TraceMove:trace=>{traces.push(trace);return traces.length===blocked?0.2:1;}};
 check(arc(visibility,[0,10,0],0)&&traces.length===6,'centre and flank firing lanes require both standing and tripod clearance');
 check(traces.filter((_,i)=>i%2===0).every(t=>t.Flags===7&&Math.abs(Math.hypot(t.End[0]-t.Start[0],t.End[2]-t.Start[2])-20)<1e-8),'high MG rays include terrain and visibility geometry over 20m');
 check(traces.filter((_,i)=>i%2===1).every(t=>t.Flags===6&&Math.abs(t.Start[1]-10.6)<1e-8&&Math.abs(Math.hypot(t.End[0]-t.Start[0],t.End[2]-t.Start[2])-8)<1e-8),'tripod rays check nearby props at muzzle height without rejecting distant rising terrain');
 for(const ray of [1,2,3,4,5,6]){traces=[];blocked=ray;check(!arc(visibility,[0,10,0],90),'bush/obstacle on any firing ray rejects gun position');}
 for(const [name,text] of Object.entries(presets)){
  const h=Number(text.match(/m_fDefenceHalfExtent ([\d.]+)/)[1]);let count=0,aligned=true;
  for(const marker of text.matchAll(/^([ \t]*)(?:GenericEntity[ \t]+)?AROR_\w+_Defence_(\d+)_(\d+) \{\r?\n([\s\S]*?)^\1\}/gm)){
   const kind=Number(marker[2]),ordinal=Number(marker[3]),body=marker[4];
   const coords=body.match(/^\s+coords ([\d.\-]+) 0 ([\d.\-]+)/m),angles=body.match(/^\s+angles 0 ([\d.\-]+) 0/m);let expected=point(kind,ordinal,h);
   if(name.endsWith('Compact')&&name!=='OutpostCompact'){
    if(kind===0)expected=[[-12,0,12],[12,0,12],[-16,0,-16],[16,0,-16],[0,0,10],[0,0,-16]][ordinal];
    if(kind===1)expected=[[0,0,20],[-20,0,6],[20,0,6]][ordinal];
    if(name==='MilitaryOutpostCoastalCompact'){
     if(kind===0)expected=[[-10,0,10],[10,0,10],[-14,0,-14],[14,0,-14],[0,0,8],[0,0,-14]][ordinal];
     if(kind===1)expected=[[0,0,16],[-16,0,6],[16,0,6]][ordinal];
    }
   }
   aligned=aligned&&Math.abs(Number(coords[1])-expected[0])<=0.001&&Math.abs(Number(coords[2])-expected[2])<=0.001;
   if(kind>0)aligned=aligned&&Number(angles?.[1]??0)===facing(kind,ordinal);
   count++;
  }
  check(aligned&&count>0,'authored '+name+' marker positions/facing match runtime layout');
 }
 return {assertions,note:'Native terrain/trace services mocked; actual bushes, walls, gun height, navigation and AI need Workbench.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance'),presets={};
 for(const preset of JSON.parse(fs.readFileSync(path.join(__dirname,'../docs/MILITARY_DEFENCE_PRESETS.json'),'utf8')).Presets)presets[preset.Name]=fs.readFileSync(path.join(root,'Prefabs/Compositions/Locations/AROR_Defence_'+preset.Name+'.et'),'utf8');
 console.log(runDefenceLayoutTests(fs.readFileSync(path.join(root,'Scripts/Game/Garrisons/AROR_DefenceLayout.c'),'utf8'),presets));
}
