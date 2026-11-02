/* Estimated horizontal authoring checks, not native collision/terrain acceptance.
 * Run with Node, or call runFortifiedLayoutChecks({relativeResourcePath: source}).
 */
function runFortifiedLayoutChecks(files) {
 let assertions=0;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 const folder='Prefabs/Compositions/Locations/';
 function close(text,start){let depth=0,quote=false;for(let i=start;i<text.length;i++){if(text[i]==='"'&&text[i-1]!=='\\')quote=!quote;if(quote)continue;if(text[i]==='{')depth++;if(text[i]==='}'&&!--depth)return i;}throw Error('Unclosed resource block');}
 function children(text){
  return [...text.matchAll(/^  GenericEntity (\w+)(?: : "([^"]+)")? \{/gm)].map(m=>{
   const block=text.slice(m.index,close(text,m.index+m[0].length-1)+1);
   const p=block.match(/\n   coords ([-\d.]+) ([-\d.]+) ([-\d.]+)/),a=block.match(/\n   angles 0 ([-\d.]+) 0/);
   if(!p)throw Error('Missing child transform '+m[1]);
   return {name:m[1],ref:m[2]||'',block,x:+p[1],z:+p[3],yaw:a?+a[1]:0,id:block.match(/ID "([^"]+)"/)?.[1]};
  });
 }
 const sizes={Sandbag_01_long_burlap:[1.3,.45],Sandbag_01_long_high_burlap:[1.3,.5],Sandbag_01_bunker_burlap_camonet:[5,5],TableMilitary_US_01:[1.5,.75],GuardTower_S_US_01:[1.75,1.75]};
 function envelope(p){
  const name=p.ref.split('/').pop().replace('.et','');
  const size=name.startsWith('EquipmentBoxWooden_')?[1,1]:sizes[name];
  if(!size)throw Error('Missing estimated envelope '+p.ref);
  const r=p.yaw*Math.PI/180;
  return [Math.abs(Math.cos(r))*size[0]+Math.abs(Math.sin(r))*size[1],Math.abs(Math.sin(r))*size[0]+Math.abs(Math.cos(r))*size[1]];
 }
 let variants=0,stores=0;
 for(const [path,text]of Object.entries(files)){
  if(!/AROR_Defence_(?:Military)?Outpost_(?:Compact|Checkpoint|Coastal)Fortified\.et$/.test(path))continue;
  variants++;
  const half=+text.match(/m_fDefenceHalfExtent (\d+)/)[1],markers=children(text).filter(p=>p.block.includes('AROR_DefencePositionComponent')).map(p=>({...p,kind:+p.block.match(/m_iKind (\d+)/)[1],ordinal:+p.block.match(/m_iOrdinal (\d+)/)[1]}));
  const scenery=children(text).filter(p=>p.ref);
  const military=path.includes('MilitaryOutpost_'),teamCount=military?6:4,gunCount=military?4:2;
  check(markers.filter(p=>p.kind===0).length===teamCount,'team markers '+path);
  check(markers.filter(p=>p.kind===1).length===gunCount,'gun markers '+path);
  check(markers.filter(p=>p.kind===4).length===gunCount+(military?1:0),'crew staging '+path);
  check(markers.filter(p=>p.kind===3).length===(military?1:0)&&!markers.some(p=>p.kind===2),'compact hull allocation '+path);
  if(military)check(text.includes('m_bOmitParkedArmour 1'),'compact recipe omits spare parked hull '+path);
  const assetPads=markers.filter(p=>[1,2,3].includes(p.kind));
  for(const p of markers){
   check(Math.max(Math.abs(p.x),Math.abs(p.z))<=half-2,'marker within bounded footprint '+p.name);
   for(const q of assetPads){if(p===q)continue;const min=[0,4].includes(p.kind)?7:12;check(Math.hypot(p.x-q.x,p.z-q.z)>=min,'logical actor spacing '+p.name+'/'+q.name);}
  }
  for(const role of ['US','USSR']){
   const wrapperPath=path.replace('.et','_Equipped_'+role+'.et'),wrapper=files[wrapperPath];
   check(!!wrapper,'equipped wrapper '+wrapperPath);
   const boxes=children(wrapper);stores+=boxes.length;
   check(boxes.length===4&&new Set(boxes.map(b=>b.block.match(/m_sCategory "([^"]+)"/)?.[1])).size===4,'four distinct supply categories '+wrapperPath);
   check(new Set([...scenery,...markers,...boxes].map(p=>p.id)).size===scenery.length+markers.length+boxes.length,'unique effective child IDs '+wrapperPath);
   const canonical=(military?'MilitaryOutpost':'Outpost')+'_Equipped_'+role;
   const previous=children(files[folder+'AROR_Defence_'+canonical+'.et']);
   check(boxes.every((b,i)=>b.id===previous[i].id),'container IDs retained across layout changes '+wrapperPath);
   const props=[...scenery,...boxes];
   for(const p of props){
    // Composite prefabs (bunkers, slotted compositions) bring their own Hierarchy: repeating it drops the component block
    const composite=/Compositions\/Slotted|_bunker_/.test(p.ref);
    check(p.block.includes('Hierarchy {')!==composite&&p.block.includes('AROR_DefenceWallComponent {'),'groundable prop '+p.name);
    const [a,b]=envelope(p);
    check(Math.hypot(Math.max(0,Math.abs(p.x)-a),Math.max(0,Math.abs(p.z)-b))>=2,'clear flag interaction area '+p.name);
   }
   for(const m of markers){
    const pad=({0:2.5,1:1.5,2:5,3:5,4:.6}[m.kind])*Math.SQRT2;
    // A raised gun stands on its own guard tower
    const raised=m.kind===1&&/m_fElevation/.test(m.block);
    check(props.every(p=>{if(raised&&/GuardTower_/.test(p.ref)&&Math.hypot(m.x-p.x,m.z-p.z)<.5)return true;const[a,b]=envelope(p);return Math.abs(m.x-p.x)>=a+pad+.2||Math.abs(m.z-p.z)>=b+pad+.2;}),'estimated pad clearance at any root yaw '+m.name+'/'+role);
   }
   for(const g of markers.filter(p=>p.kind===1))for(const delta of[-15,0,15]){
    const angle=(g.yaw+delta)*Math.PI/180;
    let clear=true;
    // Tripod guns fire from about 0.6 m: low perimeter sandbags in the first 8 m block them too (ClearMGArc)
    for(let t=.25;t<=20;t+=.25)for(const p of props.filter(p=>/high_burlap|bunker_burlap/.test(p.ref)||(t<=8&&/long_burlap/.test(p.ref)&&/_(Perimeter|Wall)_/.test(p.name)))){
     const[a,b]=envelope(p);
     if(Math.abs(g.x+Math.sin(angle)*t-p.x)<a+.15&&Math.abs(g.z+Math.cos(angle)*t-p.z)<b+.15)clear=false;
    }
    check(clear,'estimated 20m outward high-cover gun ray '+g.name+'/'+delta);
   }
   // Find a walking route from the south entrance to the flag through actual authored prop envelopes.
   const gate=military?10:path.includes('Checkpoint')?0:8;
   const start=[gate,-half-2],goal=[0,0],queue=[start],seen=new Set([start.join(',')]);
   let reached=false;
   while(queue.length&&!reached){const[x,z]=queue.shift();if(x===goal[0]&&z===goal[1]){reached=true;break;}for(const[dx,dz]of[[1,0],[-1,0],[0,1],[0,-1]]){
    const nx=x+dx,nz=z+dz,key=nx+','+nz;
    if(Math.abs(nx)>half+2||Math.abs(nz)>half+2||seen.has(key))continue;
    if(props.some(p=>{const[a,b]=envelope(p);return Math.abs(nx-p.x)<a+.4&&Math.abs(nz-p.z)<b+.4;}))continue;
    seen.add(key);queue.push([nx,nz]);
   }}
   check(reached,'estimated infantry entrance route to flag '+wrapperPath);
   for(const hull of markers.filter(p=>p.kind===3)){
    let clear=true;
    for(let z=-half-2;z<=hull.z;z+=.25)if(props.some(p=>{const[a,b]=envelope(p);return Math.abs(hull.x-p.x)<a+3.5&&Math.abs(z-p.z)<b+3.5;}))clear=false;
    check(clear,'estimated seven-metre vehicle entrance lane '+wrapperPath);
   }
  }
 }
 check(variants===4&&stores===32,'four compact geometry families with US/USSR equipment wrappers');
 return {assertions,note:'Estimated envelopes only; native bounds, terrain, navigation and loot acceptance required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path'),addon=path.join(__dirname,'../AROpenResistance/Open Resistance'),dir='Prefabs/Compositions/Locations/';
 const files=Object.fromEntries(fs.readdirSync(path.join(addon,dir)).filter(n=>n.startsWith('AROR_Defence_')&&n.endsWith('.et')).map(n=>[dir+n,fs.readFileSync(path.join(addon,dir,n),'utf8')]));
 console.log(runFortifiedLayoutChecks(files));
}
