// Executes the changed inventory and catalogue methods with native services mocked.
// Native transaction behaviour and WCS AI compatibility still require Workbench.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
const armory=fs.readFileSync(path.join(root,'Common/AROR_SoldierArmory.c'),'utf8');
const info=fs.readFileSync(path.join(root,'BlackMarket/AROR_PrefabInfo.c'),'utf8');
let assertions=0,player=0;
const check=(ok,message)=>{assertions++;if(!ok)throw Error(message)};
for(const [name,fn]of Object.entries({Contains(x){return this.includes(x)},Insert(x){this.push(x)},RemoveItem(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1)}}))
 Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const env={GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:()=>player})}),
 AROR_SoldierArmorySwapCallback:class {},AROR_ForceSpawner:{IsAlive:x=>!!x&&!x.dead},
 Replication:{IsServer:()=>true},Print(){},LogLevel:{WARNING:1},EEntityCatalogType:{ITEM:0,VEHICLE:1}};
for(const name of ['InventoryItemComponent','BaseWeaponComponent','ChimeraCharacter','SCR_Faction'])env[name]={Cast:x=>x};
const methods=['ReplaceWeapon_S','FinishSwap_S','ReplaceMagazines_S'];
function compile(source,name){
 const match=source.match(new RegExp(String.raw`^\t[^\r\n]*\b${name}\(([^\r\n]*)\)\r?\n\t\{([\s\S]*?)^\t\}`,'m'));
 if(!match)throw Error('Missing '+name);
 const args=match[1].replace(/\b\w+\s+(\w+)/g,'$1');
 let body=match[2].replace(/\b(?:IEntity|bool|int|string|ResourceName|InventoryItemComponent|InventoryStorageSlot|BaseInventoryStorageComponent|BaseWeaponComponent|BaseMuzzleComponent|BaseMagazineWell|ChimeraCharacter|SCR_\w+|AROR_\w+|FactionManager|EEntityCatalogType)\s+(\w+)/g,'let $1')
  .replace(/let (\w+);/g,'let $1=null;').replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
 for(const method of methods)body=body.replace(new RegExp(String.raw`(?<![.\w])${method}\(`,'g'),'this.'+method+'(');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
// Build methods after environment fields are set: compile captures the mocked service references.
let deleted=[];env.SCR_EntityHelper={DeleteEntityAndChildren:x=>deleted.push(x)};
const replace=compile(armory,'ReplaceWeapon_S'),finish=compile(armory,'FinishSwap_S');
function fixture({accept=true,same=false,canReplace=true}={}){
 deleted=[];player=0;const events=[],storage={};
 const oldWell={Type:()=>1},newWell={Type:()=>same?1:2};
 const oldWeapon={parent:{GetStorage:()=>storage,GetID:()=>3},FindComponent:()=>({GetParentSlot:()=>oldWeapon.parent})};
 const newMuzzle={GetDefaultMagazineOrProjectileName:()=>'new-mag',GetMagazineWell:()=>newWell};
 const weaponComponent={GetCurrentMuzzle:()=>newMuzzle};
 const newWeapon={parent:null,FindComponent:type=>type===env.InventoryItemComponent?{GetParentSlot:()=>newWeapon.parent}:weaponComponent};
 env.AROR_ForceSpawner.SpawnPrefab=()=>newWeapon;
 const inventory={items:new Set([oldWeapon]),Contains(x){return this.items.has(x)},CanReplaceItem:()=>canReplace,
  TryReplaceItem(item,target,index,cb){check(item===newWeapon&&target===storage&&index===3,'replacement targets the original slot');events.push('request');this.callback=cb;return accept}};
 const character={GetOrigin:()=>[0,0,0],GetCharacterController:()=>({SelectWeapon:()=>events.push('select')})};
 const ctx={m_aPendingSwaps:[],FinishSwap_S:finish,ReplaceMagazines_S:()=>events.push('magazines')};
 replace.call(ctx,character,inventory,oldWeapon,{GetCurrentMuzzle:()=>({GetMagazineWell:()=>oldWell})},'picked');
 return {ctx,events,inventory,character,newWeapon,oldWeapon,complete(){inventory.items.delete(oldWeapon);oldWeapon.parent=null;inventory.items.add(newWeapon);newWeapon.parent={};finish.call(ctx,inventory.callback,true)}};
}
let f=fixture();
check(f.inventory.Contains(f.oldWeapon)&&!deleted.length,'pending request retains the original weapon');
check(f.ctx.m_aPendingSwaps.length===1&&!f.events.includes('select')&&!f.events.includes('magazines'),'selection and ammunition wait for native completion');
f.complete();check(f.events.join(',')==='request,magazines,select','successful completion changes ammunition then selects the equipped gun');
check(deleted.length===1&&deleted[0]===f.oldWeapon&&f.ctx.m_aPendingSwaps.length===0,'only detached old weapon retired after success');
finish.call(f.ctx,f.inventory.callback,true);check(f.events.length===3,'duplicate completion is idempotent');
f=fixture();finish.call(f.ctx,f.inventory.callback,false);
check(f.inventory.Contains(f.oldWeapon)&&deleted.length===1&&deleted[0]===f.newWeapon,'failed transaction keeps original gun and removes unused candidate');
check(!f.events.includes('magazines')&&!f.events.includes('select'),'failure preserves ammunition and selection');
f=fixture({accept:false});check(!f.ctx.m_aPendingSwaps.length&&deleted[0]===f.newWeapon,'immediate native rejection releases callback and candidate');
f=fixture({canReplace:false});check(!f.events.length&&deleted[0]===f.newWeapon,'incompatible slot never queues replacement');
f=fixture({same:true});f.complete();check(!f.events.includes('magazines')&&f.events.includes('select'),'compatible magazines remain intact');
f=fixture();player=1;f.complete();check(!f.events.includes('magazines')&&!f.events.includes('select'),'player takeover prevents follow-up equipment changes');
f=fixture();f.newWeapon.parent={otherInventory:true};finish.call(f.ctx,f.inventory.callback,false);
check(!deleted.length,'failure never deletes a candidate acquired by another inventory');
// Catalogue absence is expected for role identity factions; lookup preserves all loaded asset factions.
let requested=[],factions=[],generalCatalog=null;
env.SCR_EntityCatalogManagerComponent={GetInstance:()=>({GetEntityCatalogOfType(type,print){check(print===false,'general catalogue is also optional');return generalCatalog}})};
const factionManager={GetFactionsCount:()=>factions.length,GetFactionByIndex:i=>factions[i]};
env.GetGame=()=>({GetFactionManager:()=>factionManager});
const find=compile(info,'FindUiInfo');
const ui={name:'content-mod item'};
factions=[{GetFactionEntityCatalogOfType(type,print){requested.push([type,print]);return null}},
 {GetFactionEntityCatalogOfType(type,print){requested.push([type,print]);return {GetEntryWithPrefab:p=>p==='known'?{GetEntityUiInfo:()=>ui}:null}}}];
check(find('known',false)===ui,'item lookup reaches a later content faction after an absent role catalogue');
check(requested.length===2&&requested.every(([type,print])=>type===0&&print===false),'optional item catalogues are probed quietly');
requested=[];check(find('unknown',true)===null,'unknown vehicle retains display-name fallback');
check(requested.every(([type,print])=>type===1&&print===false),'vehicle lookup preserves catalogue type');
requested=[];check(find('',false)===null&&!requested.length,'empty resource does not query catalogues');
generalCatalog={GetEntryWithPrefab:p=>p==='general-only'?{GetEntityUiInfo:()=>ui}:null};
requested=[];check(find('general-only',false)===ui&&!requested.length,'factionless entries resolve before faction probes');
console.log(JSON.stringify({assertions,note:'Native inventory transactions, AI/WCS execution and catalogue resources remain unverified in Workbench.'}));
