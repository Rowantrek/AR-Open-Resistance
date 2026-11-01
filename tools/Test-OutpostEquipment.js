/* Actual supply audit and stocking methods with mocked engine inventories and presets. No native loot/UI acceptance. */
function runOutpostEquipmentTests(source) {
 let assertions=0,server=true,play=true,owner=null,logs=[],timers=[],removed=[],spawned=[],deleted=[],preset=null,roll=0;
 const check=(value,message)=>{assertions++;if(!value)throw Error(message);};
 for(const [name,fn]of Object.entries({IsEmpty(){return this.length===0;},Count(){return this.length;},Insert(x){this.push(x);}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
 Object.defineProperty(String.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
 const categories={UNITS:0,VEHICLES:1,GEAR:2,EQUIPMENT:3,WEAPONS:4,WEAPON_ACCESSORIES:5,AMMUNITION:6};
 const env={Replication:{IsServer:()=>server},AROR_LocationComponent:{Cast:x=>x},BaseInventoryStorageComponent:{Cast:x=>x},InventoryStorageManagerComponent:{Cast:x=>x},
  AROR_FactionPresetsComponent:{GetInstance:()=>({GetPreset:role=>preset&&preset.role===role?preset:null})},
  AROR_EFactionPresetCategory:categories,AROR_EEquipmentType:{MEDICAL:0,BINOCULARS:1,RADIO:2},
  Resource:{Load:name=>({IsValid:()=>!name.includes('broken'),name})},EntitySpawnParams:class{constructor(){this.Transform=[];}},ETransformMode:{WORLD:0},
  SCR_EntityHelper:{DeleteEntityAndChildren:e=>deleted.push(e)},Math:{RandomInt:(a,b)=>a+(roll++%(b-a))},
  Print:(message,level)=>logs.push({message,level}),LogLevel:{NORMAL:0,WARNING:1,ERROR:2},
  string:{Format:(message,...values)=>message.replace(/%(\d+)/g,(_,i)=>String(values[i-1]))},
  GetGame:()=>({InPlayMode:()=>play,GetWorld:()=>({}),SpawnEntityPrefab:resource=>{const item={prefab:resource.name};spawned.push(item);return item;},
   GetCallqueue:()=>({CallLater:(...args)=>timers.push(args),Remove:method=>removed.push(method)})})};
 const methods=['CheckContents_S','Stock_S','StockPool','FindInventory','LocationId','GetOwner'];
 function compile(name){
  const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!match)throw Error('Missing actual supply method '+name);
  const args=match[1].replace(/\bnotnull /g,'').replace(/array<\w+>\s+/g,'').replace(/\b(?:IEntity|AROR_\w+|int)\s+/g,'');
  let body=match[2].replace(/super\.\w+\([^;]*\);/g,'')
   .replace(/array<\w+> (\w+) = \{\};/g,'let $1=[];')
   .replace(/foreach \(\w+ (\w+) : (\w+)\)/g,'for(const $1 of $2)').replace(/\.InsertAll\((\w+)\)/g,'.push(...$1)')
   .replace(/\b(?:int|string|IEntity|Resource|EntitySpawnParams|AROR_\w+|BaseInventoryStorageComponent|InventoryStorageManagerComponent) (\w+);/g,'let $1=0;')
   .replace(/\b(?:int|string|IEntity|Resource|EntitySpawnParams|AROR_\w+|BaseInventoryStorageComponent|InventoryStorageManagerComponent) (\w+) =/g,'let $1 =')
   .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
  for(const method of methods)body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(','g'),'this.'+method+'(');
  body=body.replace(/(?<![.\w])CheckContents_S\b(?!\()/g,'this.CheckContents_S');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
 }
 const inspect=compile('CheckContents_S'),init=compile('OnPostInit'),dispose=compile('OnDelete');
 const component={GetOwner:()=>owner,m_sCategory:'Weapons',m_iStockCount:6,CheckContents_S:inspect,Stock_S:compile('Stock_S'),StockPool:compile('StockPool'),
  FindInventory:compile('FindInventory'),LocationId:compile('LocationId')};
 let reads=0;
 const inventory={GetAll:items=>{reads++;items.push({weapon:true},{magazine:true});}};
 const storing=(...storages)=>(_type,out)=>out.push(...storages);
 const parent={FindComponent:()=>({GetId:()=> 'everon_test_outpost'})};
 owner={GetParent:()=>parent,FindComponent:()=>null,GetChildren:()=>null,FindComponents:storing(inventory)};
 inspect.call(component);
 check(reads===1&&logs.some(l=>l.message.includes('observedItems=2')),'audit reports physical item observations');
 check(logs[0].message.includes('site=everon_test_outpost')&&logs[0].message.includes('category=Weapons'),'inventory results identify the site and category');
 logs=[];owner.GetChildren=()=>null;owner.FindComponents=storing({GetAll:()=>0},null);inspect.call(component);
 check(logs.some(l=>l.level===1&&l.message.includes('observedItems=0')),'empty or previously looted crates are reported');
 check(logs.every(l=>l.message.includes('no refill performed')),'empty inventory never triggers a restock');
 logs=[];owner.GetChildren=()=>null;owner.FindComponents=storing();inspect.call(component);
 check(logs.some(l=>l.level===2&&l.message.includes('has no native storage')),'incompatible container assets fail visibly');
 logs=[];owner=null;inspect.call(component);check(!logs.length,'deleted containers are safe');
 owner={GetParent:()=>parent,FindComponent:()=>null,GetChildren:()=>null,FindComponents:storing(inventory)};server=false;inspect.call(component);
 check(!logs.length&&reads===1,'clients never inspect or seed server stock');
 init.call(component,owner);check(!timers.length,'client initialization schedules no inventory work');
 server=true;play=false;init.call(component,owner);check(!timers.length,'edit mode does not inspect inventories');
 play=true;init.call(component,owner);
 check(timers.length===1&&timers[0][1]===5000&&timers[0][2]===false,'server schedules one delayed inspection, not a refill loop');
 dispose.call(component,owner);check(removed.length===1&&removed[0]===inspect,'deletion removes the pending inspection');
 let parentQueries=0;const cycle={FindComponent:()=>{parentQueries++;return null;},GetParent:()=>cycle};
 logs=[];owner={GetParent:()=>parent,GetChildren:()=>null,FindComponents:storing(inventory,{GetAll:items=>items.push({bandage:true})})};inspect.call(component);
 check(logs.some(l=>l.message.includes('observedItems=3')),'items across every storage component on the box are counted');
 logs=[];owner={GetParent:()=>cycle,FindComponent:()=>null,GetChildren:()=>null,FindComponents:storing(inventory)};inspect.call(component);
 check(parentQueries===16&&logs[0].message.includes('site=unregistered'),'unexpected parent hierarchies are bounded');

 // Stocking once for the army. Presets no longer list items, so the real pool is empty until outposts get an item source
 preset={role:1,GetId:()=>'US_ARMY'};
 for(const category of ['Weapons','Ammunition','Medical','Equipment','Unknown']){
  const out=[];component.m_sCategory=category;component.StockPool(preset,1,out);
  check(!out.length,category+' store has no item source yet');
 }
 logs=[];check(component.Stock_S(1)===0&&logs.some(l=>l.level===1&&l.message.includes('no item source')),'an unstocked store says so instead of failing');
 // Stocking mechanics, with a stubbed pool standing in for the future item source
 const stubPool=['{A}Rifle.et','{B}MG.et'];
 component.StockPool=(p,role,out)=>{if(component.m_sCategory==='Weapons')out.push(...stubPool);};
 component.m_sCategory='Weapons';
 let accepted=[];const manager={TryInsertItem:item=>{if(item.prefab.includes('MG'))return false;accepted.push(item);return true;}};
 owner={GetParent:()=>parent,GetChildren:()=>null,FindComponent:()=>manager,GetWorldTransform:()=>{}};
 logs=[];roll=0;
 const placed=component.Stock_S(1);
 check(spawned.length===6&&placed===3&&accepted.length===3,'stocks the configured number of picks and counts only inserted items');
 check(deleted.length===3&&deleted.every(item=>item.prefab.includes('MG')),'items the container refuses are deleted, never left on the ground');
 check(logs.some(l=>l.message.includes('stocked 3 of 6')&&l.message.includes('US_ARMY')),'stocking logs the result and the preset used');
 component.m_sCategory='Ammunition';logs=[];check(component.Stock_S(2)===0&&logs.some(l=>l.level===1&&l.message.includes('no item source')),'a category without items stocks nothing, visibly');component.m_sCategory='Weapons';
 owner.FindComponent=()=>null;logs=[];spawned=[];
 check(component.Stock_S(1)===0&&!spawned.length&&logs.some(l=>l.level===2&&l.message.includes('no inventory manager')),'a decorative prop is reported, nothing is spawned');
 const part={FindComponent:()=>manager,GetSibling:()=>null};owner.GetChildren=()=>part;accepted=[];
 check(component.Stock_S(1)>0,'an inventory manager on a container part is used');
 server=false;spawned=[];check(component.Stock_S(1)===0&&!spawned.length,'clients never stock');server=true;
 component.m_iStockCount=0;spawned=[];check(component.Stock_S(1)===0&&!spawned.length,'a zero stock count leaves the store empty');
 return {assertions,note:'Native inventories mocked. Container contents, access, depletion and save/load require Workbench.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path');
 console.log(runOutpostEquipmentTests(fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Garrisons/AROR_OutpostSupplyComponent.c'),'utf8')));
}
