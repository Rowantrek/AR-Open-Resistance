// Read installed resource indexes without modifying them; verify the WCS/RHS preset references.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
const addons=path.join(process.env.USERPROFILE,'OneDrive/Documents/My Games/ArmaReforger/addons');
const projects=fs.readdirSync(addons,{withFileTypes:true}).filter(x=>x.isDirectory()).map(x=>path.join(addons,x.name));
const dbs=['D:/SteamLibrary/steamapps/common/Arma Reforger/addons/data',...projects].map(p=>path.join(p,'resourceDatabase.rdb')).filter(p=>fs.existsSync(p)).map(p=>({file:p,buffer:fs.readFileSync(p)}));
for(const db of dbs){db.text=db.buffer.toString('latin1');db.lower=db.text.toLowerCase();}
const targetRoot=path.join(root,'OptionalAddons/OpenResistanceWCSRHSCompat/OpenResistanceWCSRHSCompat');
const files=fs.readdirSync(path.join(targetRoot,'Configs/FactionPresets')).filter(n=>n.endsWith('.conf')).map(n=>path.join(targetRoot,'Configs/FactionPresets',n));
const references=new Map();
for(const file of files)for(const m of fs.readFileSync(file,'utf8').matchAll(/\{([A-Fa-f0-9]{16})\}([^"\r\n]+\.et)/g))references.set(m[1]+m[2],{guid:m[1],resource:m[2]});
const results=[];
for(const ref of references.values()){
 const candidates=[];
 for(const db of dbs){const index=db.lower.indexOf(ref.resource.toLowerCase()+'\0');if(index>=0){const guid=db.buffer.readBigUInt64LE(index+ref.resource.length+7).toString(16).padStart(16,'0').toUpperCase();candidates.push({database:db.file,guid});}}
 const guidBytes=Buffer.alloc(8);guidBytes.writeBigUInt64LE(BigInt('0x'+ref.guid));
 const guidLocations=candidates.some(c=>c.guid===ref.guid)?[]:dbs.filter(db=>db.buffer.indexOf(guidBytes)>=0).map(db=>({database:db.file,context:db.text.slice(Math.max(0,db.buffer.indexOf(guidBytes)-230),db.buffer.indexOf(guidBytes)).replace(/[^\x20-\x7e]/g,'|')}));
 results.push({...ref,verified:candidates.some(c=>c.guid===ref.guid),candidates,guidLocations});
}
const loadedIds=new Set(projects.map(p=>{const f=path.join(p,'addon.gproj');return fs.existsSync(f)?fs.readFileSync(f,'utf8').match(/GUID\s+"([A-Fa-f0-9]+)"/)?.[1]:null}));
loadedIds.add('58D0FB3206B6F859');loadedIds.add('6AA282336705EED2');loadedIds.add('6ABF7D8F39B53EB2');loadedIds.add('6AC238614411B347');
const project=fs.readFileSync(path.join(targetRoot,'addon.gproj'),'utf8');
const dependencies=[...project.match(/Dependencies\s*\{([^}]+)/)[1].matchAll(/"([A-Fa-f0-9]{16})"/g)].map(m=>({guid:m[1],found:loadedIds.has(m[1])}));
const report={generatedAt:new Date().toISOString(),note:'Installed resource-index identity checks only; no native inventory, compilation or flight acceptance.',databases:dbs.length,references:results.length,verified:results.filter(r=>r.verified).length,unverified:results.filter(r=>!r.verified),dependencies};
fs.writeFileSync(path.join(root,'docs/validation/BUILD_AUDIT_2026-11-06_WCS_RHS.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify(report,null,2));
