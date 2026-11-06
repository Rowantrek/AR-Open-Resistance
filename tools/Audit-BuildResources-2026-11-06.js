// Audit tracked resource identity and the freshness of historical test evidence.
const fs=require('fs'),path=require('path'),cp=require('child_process'),crypto=require('crypto');
const root=path.resolve(__dirname,'..');
const files=cp.execFileSync('git',['ls-files','-z'],{cwd:root,encoding:'utf8'}).split('\0').filter(Boolean);
const roots=files.filter(f=>f.endsWith('/addon.gproj')).map(f=>path.posix.dirname(f));
const resources=new Map(), guids=new Map(), missingMeta=[],badMeta=[],missingLocal=[],mismatches=[];
const textFiles=[];
for(const f of files){
 if(!/\.(c|et|conf|layout|layer|ent|gproj|meta|acp|sig|imageset)$/.test(f))continue;
 const content=fs.readFileSync(path.join(root,f),'utf8');textFiles.push([f,content]);
 const addon=roots.find(r=>f.startsWith(r+'/'));if(!addon)continue;
 const rel=f.slice(addon.length+1);
 if(f.endsWith('.meta')){
  const m=content.match(/Name\s+"\{([A-Fa-f0-9]{16})\}([^"\r\n]+)"/);
  if(m){const key=m[1].toUpperCase();if(!guids.has(key))guids.set(key,[]);guids.get(key).push(f);resources.set(rel.slice(0,-5),{guid:key,file:f});if(m[2].toLowerCase()!==rel.slice(0,-5).toLowerCase())badMeta.push({file:f,declared:m[2]});}
 } else if(!/\.(c|gproj|layer)$/.test(f)&&!fs.existsSync(path.join(root,f+'.meta')))missingMeta.push(f);
}
for(const [f,content] of textFiles){if(f.endsWith('.meta'))continue;
 for(const m of content.matchAll(/\{([A-Fa-f0-9]{16})\}([^"\r\n{}]+\.(?:et|conf|layout|acp|sig|wav|imageset|ent))/g)){
  const r=resources.get(m[2]);if(r&&r.guid!==m[1].toUpperCase())mismatches.push({source:f,resource:m[2],used:m[1],metadata:r.guid});
  if(!r&&/(?:^|\/)AROR_|^Sounds\/Radio\//.test(m[2]))missingLocal.push({source:f,resource:m[2],guid:m[1]});
 }
}
const prior=JSON.parse(fs.readFileSync(path.join(root,'docs/validation/QRF_AUTOMATED_RESULTS.json'),'utf8'));
const stale=Object.entries(prior.sourceHashes).filter(([f,h])=>!fs.existsSync(path.join(root,f))||crypto.createHash('sha256').update(fs.readFileSync(path.join(root,f))).digest('hex')!==h).map(([f])=>f);
const report={generatedAt:new Date().toISOString(),trackedFiles:files.length,addonRoots:roots,metadataResources:resources.size,duplicateGuids:[...guids].filter(([k,v])=>v.length>1),missingMeta,badMeta,localGuidMismatches:mismatches,localReferencesNotInTrackedMetadata:missingLocal,priorQRFReport:{generatedAt:prior.generatedAt,passed:prior.passed,recordedHashes:Object.keys(prior.sourceHashes).length,changedFiles:stale}};
fs.writeFileSync(path.join(root,'docs/validation/BUILD_AUDIT_2026-11-06_RESOURCES.json'),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({...report,priorQRFReport:{...report.priorQRFReport,changedFiles:stale.length}},null,2));
