// Repeatable local audit plus repository source/mock regression suites. No engine build is attempted.
const fs = require('fs'), path = require('path'), cp = require('child_process');
const root = path.resolve(__dirname, '..');
const failureSignature = output => output.match(/^(?:AssertionError(?: \[[^\]]+\])?|ReferenceError|TypeError|SyntaxError|Error): .+$/m)?.[0] || output.trim();
function run(file) {
  const p = cp.spawnSync(process.execPath, [path.join(__dirname, file)], {
    cwd: root, encoding: 'utf8', timeout: 120000, windowsHide: true,
    env: { ...process.env, ELECTRON_RUN_AS_NODE: '1' }
  });
  const output = ((p.stdout || '') + (p.stderr || '')).replace(/^.*crashpad.*(?:\r?\n|$)/gm, '').trim();
  return { name: file, passed: p.status === 0 && !p.error, exitCode: p.status, error: p.error?.message, output };
}
const assetAudit = run('Audit-WCSAssets.js');
if (!assetAudit.passed) { console.error(assetAudit); process.exit(1); }
const previous = JSON.parse(fs.readFileSync(path.join(root, 'docs/validation/PLAYER_SQUADS_SUITE_2026-11-07.json'), 'utf8'));
const results = fs.readdirSync(__dirname).filter(f => /^Test-.+\.js$/.test(f)).sort().map(file => {
  const result = run(file), baseline = previous.results.find(r => r.name === file);
  result.baselinePassed = baseline?.passed;
  result.matchesPreviousFailure = !result.passed && baseline?.passed === false && failureSignature(result.output) === failureSignature(baseline.output);
  result.newFailure = !result.passed && !result.matchesPreviousFailure;
  console.log(`${result.passed ? 'PASS' : result.newFailure ? 'NEW FAILURE' : 'KNOWN FAILURE'} ${file}`);
  return result;
});
const powershellChecks = [
  ['Test-OperationUIResources.ps1'],
  ['Test-StrongholdResources.ps1', '-ResourceDatabasePath', path.join(process.env.AROR_BASE_GAME_ROOT || 'D:/SteamLibrary/steamapps/common/Arma Reforger/addons/data', 'resourceDatabase.rdb')]
].map(([file, ...args]) => {
  const p = cp.spawnSync('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', path.join(__dirname, file), ...args],
    { cwd: root, encoding: 'utf8', timeout: 30000, windowsHide: true });
  const result = { name: file, passed: p.status === 0 && !p.error, exitCode: p.status, output: ((p.stdout || '') + (p.stderr || '')).trim() };
  console.log(`${result.passed ? 'PASS' : 'FAIL'} ${file}`); return result;
});
const resourceAudit = JSON.parse(fs.readFileSync(path.join(root, 'docs/validation/WCS_ASSET_AUDIT_2026-11-07.json'), 'utf8'));
const report = {
  generatedAt: new Date().toISOString(), baseCommit: cp.execFileSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8', windowsHide: true }).trim(),
  nativeVerified: false, scope: 'Installed resource indexes, ID-based addon config override model, actual source methods with native services mocked. No Enforce compilation or gameplay.',
  assetAudit: { passed: assetAudit.passed, summary: resourceAudit.summary, missingDependencies: resourceAudit.missingDependencies },
  results, powershellChecks, summary: { total: results.length, passed: results.filter(r => r.passed).length,
    knownFailures: results.filter(r => !r.passed && !r.newFailure).map(r => r.name), newFailures: results.filter(r => r.newFailure).map(r => r.name) }
};
fs.writeFileSync(path.join(root, 'docs/validation/WCS_SUITE_2026-11-07.json'), JSON.stringify(report, null, 2) + '\n');
console.log(JSON.stringify(report.summary, null, 2));
// Known failures are reported, never silently treated as a wholly passing suite.
if (report.summary.newFailures.length || report.summary.knownFailures.length || powershellChecks.some(r => !r.passed)) process.exitCode = 1;
